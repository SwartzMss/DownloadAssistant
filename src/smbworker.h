#ifndef SMBWORKER_H
#define SMBWORKER_H

#include <QThread>
#include <QString>
#include <QFile>
#include <QMutex>

class DownloadTask;

class SmbWorker : public QThread
{
    Q_OBJECT
public:
    explicit SmbWorker(DownloadTask *task, QFile *file, QMutex *mutex,
                       qint64 startOffset, qint64 chunkSize,
                       QObject *parent = nullptr);

    void requestPause();
    void requestCancel();
    void resumeWork();

signals:
    void progress(qint64 bytesReceived, qint64 bytesTotal);
    void finished(bool success, const QString &error);

protected:
    void run() override;

private:
    DownloadTask *m_task;
    QFile *m_file;
    QMutex *m_mutex;
    bool m_pauseRequested;
    bool m_cancelRequested;
    qint64 m_startOffset;
    qint64 m_chunkSize;
    qint64 m_bytesReceived;
}; 

#endif // SMBWORKER_H
