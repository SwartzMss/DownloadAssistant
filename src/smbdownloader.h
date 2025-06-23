#ifndef SMBDOWNLOADER_H
#define SMBDOWNLOADER_H

#include <QObject>
#include <QTimer>
#include <QDateTime>
#include <QMap>
#include "smbworker.h"
#include "downloadtask.h"

class SmbDownloader : public QObject
{
    Q_OBJECT

public:
    explicit SmbDownloader(QObject *parent = nullptr);
    ~SmbDownloader();

    // 下载控制
    bool startDownload(DownloadTask *task);
    void pauseDownload(DownloadTask *task);
    void resumeDownload(DownloadTask *task);
    void cancelDownload(DownloadTask *task);
    

signals:
    void downloadStarted(DownloadTask *task);
    void downloadPaused(DownloadTask *task);
    void downloadResumed(DownloadTask *task);
    void downloadCancelled(DownloadTask *task);
    void downloadCompleted(DownloadTask *task);
    void downloadFailed(DownloadTask *task, const QString &error);
    void downloadProgress(DownloadTask *task, qint64 bytesReceived, qint64 bytesTotal);

private slots:
    void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void onDownloadFinished(DownloadTask *task, bool success, const QString &error);
    void updateSpeed();

private:
    struct DownloadInfo {
        DownloadTask *task;
        SmbWorker *worker;
        QTimer *speedTimer;
        qint64 lastBytesReceived;
        qint64 lastSpeedUpdate;
        qint64 totalBytes;
    };

    QMap<DownloadTask*, DownloadInfo*> m_activeDownloads;
    
    // 辅助方法
    DownloadInfo* findDownloadInfo(DownloadTask *task);
    void cleanupDownload(DownloadTask *task);
};

#endif // SMBDOWNLOADER_H 