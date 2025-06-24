#ifndef SMBWORKER_H
#define SMBWORKER_H

#include <QThread>
#include <QString>

class DownloadTask;

class SmbWorker : public QThread
{
    Q_OBJECT
public:
    explicit SmbWorker(DownloadTask *task, QObject *parent = nullptr);

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
    bool m_pauseRequested;
    bool m_cancelRequested;
    qint64 m_offset;
};

#endif // SMBWORKER_H
