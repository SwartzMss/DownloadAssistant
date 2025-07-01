#include "smbdownloader.h"
#include "downloadtask.h"
#include "smbworker.h"
#include "pathutils.h"
#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QMessageBox>
#include <QApplication>
#include <QDebug>
#include <QThread>
#include <QTimer>
#include <QtGlobal>
#include "logger.h"

SmbDownloader::SmbDownloader(QObject *parent)
    : QObject(parent)
    , m_chunkSize(1024 * 1024)
{
    LOG_INFO("SmbDownloader 初始化");
}

SmbDownloader::~SmbDownloader()
{
    LOG_INFO("SmbDownloader 析构");
    // 清理所有活动下载
    for (auto info : m_activeDownloads.values()) {
        for (SmbWorker *w : info->workers) {
            w->requestCancel();
            w->wait();
            delete w;
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
    info->file = nullptr;
    info->speedTimer = nullptr;
    info->lastBytesReceived = 0;
    info->lastSpeedUpdate = QDateTime::currentMSecsSinceEpoch();
    info->totalBytes = 0;
    info->smoothedSpeed = 0.0;

    // 检查文件是否存在以确定断点续传
    QUrl url(task->url());
    QString filePath = task->savePath();
    if (!filePath.endsWith('/') && !filePath.endsWith('\\'))
        filePath += '/';
    QString fileName = url.fileName();
    if (fileName.isEmpty())
        fileName = "downloaded_file";
    filePath += fileName;

    QFileInfo fileInfo(filePath);
    QDir().mkpath(fileInfo.absolutePath());

    // 先获取远程文件大小
    QString unc = toUncPath(task->url());
    QFile remoteFile(unc);
    if (!remoteFile.open(QIODevice::ReadOnly)) {
        delete info;
        emit downloadFailed(task, tr("无法打开远程文件"));
        return false;
    }
    qint64 totalSize = remoteFile.size();
    remoteFile.close();
    task->setTotalSize(totalSize);
    info->totalBytes = totalSize;

    // 打开本地文件，预分配大小
    QFile *localFile = new QFile(filePath);
    if (!localFile->open(QIODevice::ReadWrite)) {
        delete localFile;
        delete info;
        emit downloadFailed(task, tr("无法创建文件"));
        return false;
    }
    localFile->resize(totalSize);
    info->file = localFile;

    // 创建工作线程，按块分割
    int chunkCount = (int)((totalSize + m_chunkSize - 1) / m_chunkSize);
    for (int i = 0; i < chunkCount; ++i) {
        qint64 offset = i * m_chunkSize;
        qint64 size = qMin(m_chunkSize, totalSize - offset);
        SmbWorker *w = new SmbWorker(task, localFile, &info->mutex, offset, size, this);
        connect(w, &SmbWorker::progress, this, &SmbDownloader::onDownloadProgress);
        connect(w, &SmbWorker::finished, this, [this, task](bool success, const QString &err) {
            onDownloadFinished(task, success, err);
        });
        info->workers.append(w);
        info->progressMap[w] = 0;
    }

    // 创建速度计时器
    info->speedTimer = new QTimer(this);
    info->speedTimer->setInterval(1000); // 每秒更新一次速度
    connect(info->speedTimer, &QTimer::timeout, this, &SmbDownloader::updateSpeed);
    info->speedTimer->start();

    // 添加到活动下载列表
    m_activeDownloads[task] = info;

    // 更新任务状态并启动线程
    task->setStatus(DownloadTask::Downloading);
    for (SmbWorker *w : info->workers) {
        w->start();
    }

    emit downloadStarted(task);
    return true;
}

void SmbDownloader::pauseDownload(DownloadTask *task)
{
    LOG_INFO(QString("暂停 SMB 下载 - 任务ID: %1").arg(task->id()));
    
    DownloadInfo *info = findDownloadInfo(task);
    if (!info) {
        LOG_WARNING(QString("找不到下载信息 - 任务ID: %1").arg(task->id()));
        return;
    }

    // 暂停下载
    for (SmbWorker *w : info->workers)
        w->requestPause();
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

    DownloadInfo *info = findDownloadInfo(task);
    if (info && !info->workers.isEmpty()) {
        for (SmbWorker *w : info->workers)
            w->resumeWork();
        task->setStatus(DownloadTask::Downloading);
        emit downloadResumed(task);
    } else {
        // 若未找到下载信息，重新开始下载
        startDownload(task);
    }
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
    for (SmbWorker *w : info->workers) {
        w->requestCancel();
        w->wait();
    }

    // 删除已下载的部分文件
    QUrl url(task->url());
    QString filePath = task->savePath();
    if (!filePath.endsWith('/') && !filePath.endsWith('\\'))
        filePath += '/';
    QString fileName = url.fileName();
    if (fileName.isEmpty())
        fileName = "downloaded_file";
    filePath += fileName;
    QFile::remove(filePath);

    task->setStatus(DownloadTask::Cancelled);
    cleanupDownload(task);
    
    LOG_INFO(QString("SMB 下载已取消 - 任务ID: %1").arg(task->id()));
    emit downloadCancelled(task);
}



void SmbDownloader::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    SmbWorker *worker = qobject_cast<SmbWorker*>(sender());
    if (!worker)
        return;

    DownloadTask *task = nullptr;
    DownloadInfo *info = nullptr;

    for (auto it = m_activeDownloads.begin(); it != m_activeDownloads.end(); ++it) {
        if (it.value()->workers.contains(worker)) {
            task = it.key();
            info = it.value();
            break;
        }
    }

    if (!task || !info)
        return;

    if (info->totalBytes > 0) {
        task->setTotalSize(info->totalBytes);
    }

    info->progressMap[worker] = bytesReceived;
    qint64 sum = 0;
    for (qint64 v : info->progressMap.values())
        sum += v;
    task->setDownloadedSize(sum);
    emit downloadProgress(task, sum, info->totalBytes);
}

void SmbDownloader::onDownloadFinished(DownloadTask *task, bool success, const QString &error)
{
    DownloadInfo *info = findDownloadInfo(task);
    if (!info)
        return;

    if (!success) {
        task->setStatus(DownloadTask::Failed);
        emit downloadFailed(task, error);
        for (SmbWorker *w : info->workers) {
            if (w->isRunning())
                w->requestCancel();
        }
        cleanupDownload(task);
        return;
    }

    info->finishedCount++;
    if (info->finishedCount == info->workers.size()) {
        task->setStatus(DownloadTask::Completed);
        emit downloadCompleted(task);
        cleanupDownload(task);
    }
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
                qint64 bytesDiff = task->downloadedSize() - info->lastBytesReceived;
                double newSpeed = (bytesDiff * 1000.0) / timeDiff; // 字节/秒

                if (bytesDiff > 0) {
                    info->smoothedSpeed = 0.7 * info->smoothedSpeed + 0.3 * newSpeed;
                }

                info->lastSpeedUpdate = currentTime;
                info->lastBytesReceived = task->downloadedSize();
                task->setSpeed(static_cast<qint64>(info->smoothedSpeed));
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

    for (SmbWorker *w : info->workers) {
        w->wait();
        delete w;
    }
    info->workers.clear();
    if (info->file) {
        info->file->close();
        delete info->file;
        info->file = nullptr;
    }
    
    if (info->speedTimer) {
        info->speedTimer->stop();
        delete info->speedTimer;
    }
    
    delete info;
}

