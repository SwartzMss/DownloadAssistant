#include "smbdownloader.h"
#include "downloadtask.h"
#include "smbworker.h"
#include <QFileInfo>
#include <QDir>
#include <QStandardPaths>
#include <QMessageBox>
#include <QApplication>
#include <QDebug>
#include <QThread>
#include <QTimer>
#include "logger.h"

SmbDownloader::SmbDownloader(QObject *parent)
    : QObject(parent)
{
    LOG_INFO("SmbDownloader 初始化");
}

SmbDownloader::~SmbDownloader()
{
    LOG_INFO("SmbDownloader 析构");
    // 清理所有活动下载
    for (auto info : m_activeDownloads.values()) {
        if (info->worker) {
            info->worker->requestCancel();
            info->worker->wait();
            delete info->worker;
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
    info->worker = new SmbWorker(task, this);
    info->speedTimer = nullptr;
    info->lastBytesReceived = task->downloadedSize();
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
    if (fileInfo.exists()) {
        task->setSupportsResume(true);
    }
    
    // 连接信号
    connect(info->worker, &SmbWorker::progress,
            this, &SmbDownloader::onDownloadProgress);
    connect(info->worker, &SmbWorker::finished,
            this, [this, task](bool success, const QString &err) {
                onDownloadFinished(task, success, err);
            });

    // 创建速度计时器
    info->speedTimer = new QTimer(this);
    info->speedTimer->setInterval(1000); // 每秒更新一次速度
    connect(info->speedTimer, &QTimer::timeout, this, &SmbDownloader::updateSpeed);
    info->speedTimer->start();

    // 添加到活动下载列表
    m_activeDownloads[task] = info;

    // 更新任务状态并启动线程
    task->setStatus(DownloadTask::Downloading);
    info->worker->start();

    emit downloadStarted(task);
    return true;
}

void SmbDownloader::pauseDownload(DownloadTask *task)
{
    LOG_INFO(QString("暂停 SMB 下载 - 任务ID: %1").arg(task->id()));
    
    DownloadInfo *info = findDownloadInfo(task);
    if (!info || !info->worker) {
        LOG_WARNING(QString("找不到下载信息 - 任务ID: %1").arg(task->id()));
        return;
    }

    // 暂停下载
    info->worker->requestPause();
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
    if (info && info->worker) {
        info->worker->resumeWork();
    } else {
        startDownload(task);
    }
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
    if (info->worker) {
        info->worker->requestCancel();
        info->worker->wait();
    }
    
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
        if (it.value()->worker == worker) {
            task = it.key();
            info = it.value();
            break;
        }
    }

    if (!task || !info)
        return;

    if (bytesTotal > 0) {
        task->setTotalSize(bytesTotal);
        info->totalBytes = bytesTotal;
    }

    task->setDownloadedSize(bytesReceived);
    emit downloadProgress(task, bytesReceived, bytesTotal);
}

void SmbDownloader::onDownloadFinished(DownloadTask *task, bool success, const QString &error)
{
    DownloadInfo *info = findDownloadInfo(task);
    Q_UNUSED(info);

    if (success) {
        task->setStatus(DownloadTask::Completed);
        emit downloadCompleted(task);
    } else {
        task->setStatus(DownloadTask::Failed);
        emit downloadFailed(task, error);
    }

    cleanupDownload(task);
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

    if (info->worker) {
        info->worker->wait();
        delete info->worker;
    }
    
    if (info->speedTimer) {
        info->speedTimer->stop();
        delete info->speedTimer;
    }
    
    delete info;
}

