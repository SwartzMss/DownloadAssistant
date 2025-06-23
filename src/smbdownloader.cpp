#include "smbdownloader.h"
#include "downloadtask.h"
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QFileInfo>
#include <QDir>
#include <QStandardPaths>
#include <QMessageBox>
#include <QApplication>
#include <QDebug>
#include <QFile>
#include <QThread>
#include <QTimer>
#include "logger.h"

SmbDownloader::SmbDownloader(QObject *parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
{
    LOG_INFO("SmbDownloader 初始化");
    
    connect(m_networkManager, &QNetworkAccessManager::finished,
            this, &SmbDownloader::onDownloadFinished);
}

SmbDownloader::~SmbDownloader()
{
    LOG_INFO("SmbDownloader 析构");
    // 清理所有活动下载
    for (auto info : m_activeDownloads.values()) {
        if (info->reply) {
            info->reply->abort();
        }
        if (info->file) {
            info->file->close();
            delete info->file;
        }
        if (info->speedTimer) {
            info->speedTimer->stop();
            delete info->speedTimer;
        }
        delete info;
    }
    m_activeDownloads.clear();
}

bool SmbDownloader::startDownload(DownloadTask *task)
{
    if (!task) {
        LOG_ERROR("下载任务为空");
        return false;
    }
    
    LOG_INFO(QString("开始 SMB 下载 - 任务ID: %1, URL: %2").arg(task->id()).arg(task->url()));
    
    // 检查任务状态
    if (task->status() != DownloadTask::Downloading) {
        LOG_WARNING(QString("任务状态不正确 - ID: %1, 状态: %2").arg(task->id()).arg(static_cast<int>(task->status())));
        return false;
    }
    
    // 创建下载信息
    DownloadInfo *info = new DownloadInfo;
    info->task = task;
    info->reply = nullptr;
    info->file = nullptr;
    info->speedTimer = nullptr;
    info->lastBytesReceived = 0;
    info->lastSpeedUpdate = 0;
    info->supportsResume = false;
    info->downloadedBytes = 0;
    info->totalBytes = 0;
    info->startTime = QDateTime::currentDateTime();
    
    // 准备文件路径
    QString filePath = task->savePath();
    if (!filePath.endsWith('/') && !filePath.endsWith('\\')) {
        filePath += '/';
    }
    
    // 从 URL 中提取文件名
    QUrl url(task->url());
    QString fileName = url.fileName();
    if (fileName.isEmpty()) {
        fileName = "downloaded_file";
    }
    filePath += fileName;
    
    LOG_INFO(QString("文件保存路径: %1").arg(filePath));
    
    // 创建目录
    QFileInfo fileInfo(filePath);
    QDir().mkpath(fileInfo.absolutePath());
    
    // 检查文件是否已存在，支持断点续传
    if (fileInfo.exists()) {
        info->supportsResume = true;
        task->setSupportsResume(true);
        task->setDownloadedSize(fileInfo.size());
        info->downloadedBytes = fileInfo.size();
    }

    // 创建网络请求
    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", "DownloadAssistant/1.0");
    
    // 设置断点续传头
    if (info->supportsResume && info->downloadedBytes > 0) {
        LOG_INFO(QString("检测到断点续传 - 已下载: %1 字节").arg(info->downloadedBytes));
        request.setRawHeader("Range", QString("bytes=%1-").arg(info->downloadedBytes).toUtf8());
    }

    // 发送请求
    info->reply = m_networkManager->get(request);
    
    // 连接信号
    connect(info->reply, &QNetworkReply::downloadProgress,
            this, &SmbDownloader::onDownloadProgress);
    connect(info->reply, &QNetworkReply::errorOccurred,
            this, &SmbDownloader::onNetworkError);
    connect(info->reply, &QNetworkReply::sslErrors,
            this, &SmbDownloader::onSslErrors);

    // 创建文件
    info->file = new QFile(filePath);
    if (!info->file->open(QIODevice::WriteOnly | QIODevice::Append)) {
        LOG_ERROR(QString("无法打开文件: %1").arg(filePath));
        task->setStatus(DownloadTask::Failed);
        task->setErrorMessage(tr("无法创建文件"));
        emit downloadFailed(task, tr("无法创建文件：%1").arg(info->file->errorString()));
        cleanupDownload(task);
        return false;
    }

    // 创建速度计时器
    info->speedTimer = new QTimer(this);
    info->speedTimer->setInterval(1000); // 每秒更新一次速度
    connect(info->speedTimer, &QTimer::timeout, this, &SmbDownloader::updateSpeed);
    info->speedTimer->start();

    // 添加到活动下载列表
    m_activeDownloads[task] = info;
    
    // 更新任务状态
    task->setStatus(DownloadTask::Downloading);
    
    emit downloadStarted(task);
    return true;
}

void SmbDownloader::pauseDownload(DownloadTask *task)
{
    LOG_INFO(QString("暂停 SMB 下载 - 任务ID: %1").arg(task->id()));
    
    DownloadInfo *info = findDownloadInfo(task);
    if (!info || !info->reply) {
        LOG_WARNING(QString("找不到下载信息 - 任务ID: %1").arg(task->id()));
        return;
    }

    // 暂停下载
    info->reply->abort();
    task->setStatus(DownloadTask::Paused);
    
    LOG_INFO(QString("SMB 下载已暂停 - 任务ID: %1").arg(task->id()));
    emit downloadPaused(task);
}

void SmbDownloader::resumeDownload(DownloadTask *task)
{
    LOG_INFO(QString("恢复 SMB 下载 - 任务ID: %1").arg(task->id()));
    
    if (task->status() != DownloadTask::Paused) {
        LOG_WARNING(QString("任务不在暂停状态 - 任务ID: %1").arg(task->id()));
        return;
    }

    // 重新开始下载
    startDownload(task);
    emit downloadResumed(task);
}

void SmbDownloader::cancelDownload(DownloadTask *task)
{
    LOG_INFO(QString("取消 SMB 下载 - 任务ID: %1").arg(task->id()));
    
    DownloadInfo *info = findDownloadInfo(task);
    if (!info) {
        LOG_WARNING(QString("找不到下载信息 - 任务ID: %1").arg(task->id()));
        return;
    }

    // 取消下载
    if (info->reply) {
        info->reply->abort();
    }
    
    task->setStatus(DownloadTask::Cancelled);
    cleanupDownload(task);
    
    LOG_INFO(QString("SMB 下载已取消 - 任务ID: %1").arg(task->id()));
    emit downloadCancelled(task);
}



void SmbDownloader::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        return;
    }

    // 查找对应的下载信息
    DownloadTask *task = nullptr;
    DownloadInfo *info = nullptr;
    
    for (auto it = m_activeDownloads.begin(); it != m_activeDownloads.end(); ++it) {
        if (it.value()->reply == reply) {
            task = it.key();
            info = it.value();
            break;
        }
    }

    if (!task || !info) {
        return;
    }

    // 更新进度
    qint64 totalReceived = info->supportsResume ? 
                          task->downloadedSize() + bytesReceived : bytesReceived;
    
    if (bytesTotal > 0) {
        task->setTotalSize(bytesTotal);
    }
    
    task->setDownloadedSize(totalReceived);
    
    // 写入文件
    if (info->file && info->file->isOpen()) {
        QByteArray data = reply->readAll();
        if (!data.isEmpty()) {
            info->file->write(data);
            info->file->flush();
        }
    }

    // 更新速度计算
    info->lastBytesReceived = totalReceived;
    
    emit downloadProgress(task, totalReceived, bytesTotal);
}

void SmbDownloader::onDownloadFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        return;
    }

    // 查找对应的下载信息
    DownloadTask *task = nullptr;
    DownloadInfo *info = nullptr;
    
    for (auto it = m_activeDownloads.begin(); it != m_activeDownloads.end(); ++it) {
        if (it.value()->reply == reply) {
            task = it.key();
            info = it.value();
            break;
        }
    }

    if (!task || !info) {
        return;
    }

    // 检查下载是否成功
    if (reply->error() == QNetworkReply::NoError) {
        // 写入剩余数据
        if (info->file && info->file->isOpen()) {
            QByteArray data = reply->readAll();
            if (!data.isEmpty()) {
                info->file->write(data);
            }
            info->file->close();
        }

        task->setStatus(DownloadTask::Completed);
        emit downloadCompleted(task);
    } else {
        task->setStatus(DownloadTask::Failed);
        emit downloadFailed(task, reply->errorString());
    }

    cleanupDownload(task);
}

void SmbDownloader::onNetworkError(QNetworkReply::NetworkError error)
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        return;
    }

    // 查找对应的任务
    DownloadTask *task = nullptr;
    for (auto it = m_activeDownloads.begin(); it != m_activeDownloads.end(); ++it) {
        if (it.value()->reply == reply) {
            task = it.key();
            break;
        }
    }

    if (task) {
        task->setStatus(DownloadTask::Failed);
        emit downloadFailed(task, reply->errorString());
    }
}

void SmbDownloader::onSslErrors(const QList<QSslError> &errors)
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        return;
    }

    // 对于开发阶段，忽略 SSL 错误
    reply->ignoreSslErrors();
}

void SmbDownloader::updateSpeed()
{
    for (auto it = m_activeDownloads.begin(); it != m_activeDownloads.end(); ++it) {
        DownloadInfo *info = it.value();
        DownloadTask *task = it.key();
        
        if (info->speedTimer && info->speedTimer->isActive()) {
            qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
            qint64 timeDiff = currentTime - info->lastSpeedUpdate;
            
            if (timeDiff > 0) {
                qint64 bytesDiff = info->lastBytesReceived - task->downloadedSize();
                qint64 speed = (bytesDiff * 1000) / timeDiff; // 字节/秒
                info->lastSpeedUpdate = currentTime;
            }
        }
    }
}

SmbDownloader::DownloadInfo* SmbDownloader::findDownloadInfo(DownloadTask *task)
{
    return m_activeDownloads.value(task, nullptr);
}

void SmbDownloader::cleanupDownload(DownloadTask *task)
{
    DownloadInfo *info = m_activeDownloads.take(task);
    if (!info) {
        return;
    }

    if (info->reply) {
        info->reply->deleteLater();
    }
    
    if (info->file) {
        info->file->close();
        delete info->file;
    }
    
    if (info->speedTimer) {
        info->speedTimer->stop();
        delete info->speedTimer;
    }
    
    delete info;
}

