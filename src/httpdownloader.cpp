#include "httpdownloader.h"
#include "downloadtask.h"
#include <QFile>
#include <QDir>
#include <QUrl>
#include <QFileInfo>
#include <QDateTime>
#include "logger.h"

HttpDownloader::HttpDownloader(QObject *parent)
    : QObject(parent)
{
    LOG_INFO("HttpDownloader 初始化");
}

HttpDownloader::~HttpDownloader()
{
    LOG_INFO("HttpDownloader 析构");
    for (auto info : m_activeDownloads.values()) {
        if (info->reply) {
            info->reply->abort();
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
    m_activeDownloads.clear();
}

bool HttpDownloader::startDownload(DownloadTask *task)
{
    if (!task) {
        LOG_ERROR("下载任务为空");
        return false;
    }

    LOG_INFO(QString("开始 HTTP 下载 - 任务ID: %1, URL: %2").arg(task->id()).arg(task->url()));

    QUrl url(task->url());
    QString filePath = task->savePath();
    if (!filePath.endsWith('/') && !filePath.endsWith('\\'))
        filePath += '/';
    QString fileName = url.fileName();
    if (fileName.isEmpty())
        fileName = "downloaded_file";
    filePath += fileName;

    QDir().mkpath(QFileInfo(filePath).absolutePath());
    QFile *file = new QFile(filePath);
    if (!file->open(QIODevice::ReadWrite)) {
        LOG_ERROR("无法创建文件");
        delete file;
        emit downloadFailed(task, tr("无法创建文件"));
        return false;
    }

    qint64 existingSize = file->size();
    if (existingSize > 0) {
        task->setSupportsResume(true);
    }
    if (!file->seek(existingSize)) {
        file->close();
        emit downloadFailed(task, tr("文件写入失败"));
        delete file;
        return false;
    }

    QNetworkRequest request(url);
    if (existingSize > 0)
        request.setRawHeader("Range", QByteArray("bytes=") + QByteArray::number(existingSize) + "-");

    QNetworkReply *reply = m_manager.get(request);

    DownloadInfo *info = new DownloadInfo{task, reply, file, nullptr, existingSize, QDateTime::currentMSecsSinceEpoch(), 0};
    connect(reply, &QNetworkReply::readyRead, this, &HttpDownloader::onReadyRead);
    connect(reply, &QNetworkReply::finished, this, &HttpDownloader::onFinished);
    connect(reply, &QNetworkReply::downloadProgress, this, &HttpDownloader::onProgress);

    info->speedTimer = new QTimer(this);
    info->speedTimer->setInterval(1000);
    connect(info->speedTimer, &QTimer::timeout, this, &HttpDownloader::updateSpeed);
    info->speedTimer->start();

    m_activeDownloads[task] = info;

    emit downloadStarted(task);
    return true;
}

void HttpDownloader::pauseDownload(DownloadTask *task)
{
    DownloadInfo *info = findDownloadInfo(task);
    if (!info)
        return;

    if (info->reply) {
        info->reply->abort();
    }
    task->setStatus(DownloadTask::Paused);
    emit downloadPaused(task);
}

void HttpDownloader::resumeDownload(DownloadTask *task)
{
    DownloadInfo *info = findDownloadInfo(task);
    if (!info) {
        startDownload(task);
        emit downloadResumed(task);
        return;
    }

    if (task->status() != DownloadTask::Paused)
        return;

    if (info->reply) {
        info->reply->deleteLater();
        info->reply = nullptr;
    }
    if (!info->file->isOpen()) {
        if (!info->file->open(QIODevice::ReadWrite)) {
            emit downloadFailed(task, tr("无法打开文件"));
            cleanupDownload(task);
            return;
        }
        info->file->seek(task->downloadedSize());
    }

    QNetworkRequest request(task->url());
    if (task->downloadedSize() > 0)
        request.setRawHeader("Range", QByteArray("bytes=") + QByteArray::number(task->downloadedSize()) + "-");
    QNetworkReply *reply = m_manager.get(request);
    info->reply = reply;
    info->lastSpeedUpdate = QDateTime::currentMSecsSinceEpoch();
    info->lastBytesReceived = task->downloadedSize();

    connect(reply, &QNetworkReply::readyRead, this, &HttpDownloader::onReadyRead);
    connect(reply, &QNetworkReply::finished, this, &HttpDownloader::onFinished);
    connect(reply, &QNetworkReply::downloadProgress, this, &HttpDownloader::onProgress);

    if (info->speedTimer && !info->speedTimer->isActive())
        info->speedTimer->start();

    task->setStatus(DownloadTask::Downloading);
    emit downloadResumed(task);
}

void HttpDownloader::cancelDownload(DownloadTask *task)
{
    DownloadInfo *info = findDownloadInfo(task);
    if (!info)
        return;

    if (info->reply) {
        info->reply->abort();
    }
    task->setStatus(DownloadTask::Cancelled);
    emit downloadCancelled(task);
    cleanupDownload(task);
}

void HttpDownloader::onReadyRead()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply)
        return;

    for (auto it = m_activeDownloads.begin(); it != m_activeDownloads.end(); ++it) {
        DownloadInfo *info = it.value();
        if (info->reply == reply) {
            if (info->file)
                info->file->write(reply->readAll());
            break;
        }
    }
}

void HttpDownloader::onFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply)
        return;

    DownloadTask *task = nullptr;
    DownloadInfo *info = nullptr;
    for (auto it = m_activeDownloads.begin(); it != m_activeDownloads.end(); ++it) {
        if (it.value()->reply == reply) {
            task = it.key();
            info = it.value();
            break;
        }
    }
    if (!task || !info)
        return;

    info->file->flush();
    info->file->close();

    if (reply->error() == QNetworkReply::NoError) {
        task->setStatus(DownloadTask::Completed);
        emit downloadCompleted(task);
    } else {
        task->setStatus(DownloadTask::Failed);
        emit downloadFailed(task, reply->errorString());
    }

    cleanupDownload(task);
    reply->deleteLater();
}

void HttpDownloader::onProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply)
        return;

    DownloadTask *task = nullptr;
    DownloadInfo *info = nullptr;
    for (auto it = m_activeDownloads.begin(); it != m_activeDownloads.end(); ++it) {
        if (it.value()->reply == reply) {
            task = it.key();
            info = it.value();
            break;
        }
    }
    if (!task || !info)
        return;

    if (bytesTotal > 0) {
        task->setTotalSize(info->totalBytes > 0 ? info->totalBytes : bytesTotal + task->downloadedSize());
        info->totalBytes = task->totalSize();
    }

    task->setDownloadedSize(bytesReceived + task->downloadedSize());
    emit downloadProgress(task, task->downloadedSize(), task->totalSize());
}

void HttpDownloader::updateSpeed()
{
    for (auto it = m_activeDownloads.begin(); it != m_activeDownloads.end(); ++it) {
        DownloadInfo *info = it.value();
        DownloadTask *task = it.key();

        qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
        qint64 timeDiff = currentTime - info->lastSpeedUpdate;
        if (timeDiff <= 0)
            continue;
        qint64 bytesDiff = task->downloadedSize() - info->lastBytesReceived;
        qint64 speed = (bytesDiff * 1000) / timeDiff;
        info->lastSpeedUpdate = currentTime;
        info->lastBytesReceived = task->downloadedSize();
        task->setSpeed(speed);
    }
}

HttpDownloader::DownloadInfo* HttpDownloader::findDownloadInfo(DownloadTask *task)
{
    return m_activeDownloads.value(task, nullptr);
}

void HttpDownloader::cleanupDownload(DownloadTask *task)
{
    DownloadInfo *info = m_activeDownloads.take(task);
    if (!info)
        return;
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

