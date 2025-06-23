#ifndef HTTPDOWNLOADER_H
#define HTTPDOWNLOADER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include <QMap>
#include "downloadtask.h"

class HttpDownloader : public QObject
{
    Q_OBJECT
public:
    explicit HttpDownloader(QObject *parent = nullptr);
    ~HttpDownloader();

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
    void onReadyRead();
    void onFinished();
    void onProgress(qint64 bytesReceived, qint64 bytesTotal);
    void updateSpeed();

private:
    struct DownloadInfo {
        DownloadTask *task;
        QNetworkReply *reply;
        QFile *file;
        QTimer *speedTimer;
        qint64 lastBytesReceived;
        qint64 lastSpeedUpdate;
        qint64 totalBytes;
    };

    QNetworkAccessManager m_manager;
    QMap<DownloadTask*, DownloadInfo*> m_activeDownloads;

    DownloadInfo* findDownloadInfo(DownloadTask *task);
    void cleanupDownload(DownloadTask *task);
};

#endif // HTTPDOWNLOADER_H
