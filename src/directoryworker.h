#ifndef DIRECTORYWORKER_H
#define DIRECTORYWORKER_H

#include <QThread>
#include <QString>

class DownloadManager;

class DirectoryWorker : public QThread
{
    Q_OBJECT
public:
    DirectoryWorker(const QString &dirUrl, const QString &localPath,
                    DownloadManager *manager, QObject *parent = nullptr);

signals:
    void finished();

protected:
    void run() override;

private:
    void scanDirectory(const QString &dirUrl, const QString &localPath);

    QString m_dirUrl;
    QString m_localPath;
    DownloadManager *m_manager;
};

#endif // DIRECTORYWORKER_H
