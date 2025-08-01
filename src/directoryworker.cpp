#include "directoryworker.h"
#include "downloadmanager.h"
#include <QDir>
#include <QFileInfo>
#include <QUrl>
#include "logger.h"
#include "pathutils.h"

DirectoryWorker::DirectoryWorker(const QString &dirUrl, const QString &localPath,
                                 DownloadManager *manager, QObject *parent)
    : QThread(parent), m_dirUrl(dirUrl), m_localPath(localPath), m_manager(manager)
{
}

void DirectoryWorker::run()
{
    scanDirectory(m_dirUrl, m_localPath);
    emit finished();
}

void DirectoryWorker::scanDirectory(const QString &dirUrl, const QString &localPath)
{
    if (!m_manager)
        return;

    QString uncPath = toUncPath(dirUrl);
    QDir dir(uncPath);
    if (!dir.exists())
        return;

    QDir().mkpath(localPath);

    QFileInfoList list = dir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot);
    for (const QFileInfo &info : list) {
        QString name = info.fileName();
        QString childUrl = dirUrl;
        if (!childUrl.endsWith('/'))
            childUrl += '/';
        childUrl += name;

        if (info.isDir()) {
            QString subLocal = QDir(localPath).filePath(name);
            scanDirectory(childUrl, subLocal);
        } else {
            QString taskId = m_manager->addTask(childUrl, localPath);
            m_manager->startTask(taskId);
        }
    }
}

