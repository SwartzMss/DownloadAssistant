#include "directoryworker.h"
#include "downloadmanager.h"
#include <QDir>
#include <QFileInfo>
#include <QUrl>
#include "logger.h"

static QString toUncPath(QString path)
{
    path.remove('\r');
    path.remove('\n');
    path = path.trimmed();
    if (path.startsWith("smb://", Qt::CaseInsensitive)) {
        QUrl u(path);
        QString p = u.path();
        if (p.startsWith('/'))
            p.remove(0, 1);
        p.replace('/', '\\');
        return QStringLiteral("\\\\") + u.host() + QLatin1Char('\\') + p;
    }
    path.replace('/', '\\');
    return path;
}

DirectoryWorker::DirectoryWorker(const QString &dirUrl, const QString &localPath,
                                 DownloadManager *manager, QObject *parent)
    : QThread(parent), m_dirUrl(dirUrl), m_localPath(localPath), m_manager(manager)
{
    LOG_INFO(QString("DirectoryWorker 创建 - URL: %1, 本地路径: %2")
                 .arg(dirUrl)
                 .arg(localPath));
}

void DirectoryWorker::run()
{
    LOG_INFO(QString("开始扫描目录: %1").arg(m_dirUrl));
    scanDirectory(m_dirUrl, m_localPath);
    LOG_INFO("目录扫描完成");
    emit finished();
}

void DirectoryWorker::scanDirectory(const QString &dirUrl, const QString &localPath)
{
    if (!m_manager) {
        LOG_WARNING("DownloadManager 不存在，无法扫描目录");
        return;
    }

    QString uncPath = toUncPath(dirUrl);
    QDir dir(uncPath);
    if (!dir.exists()) {
        LOG_WARNING(QString("目录不存在: %1").arg(uncPath));
        return;
    }

    QDir().mkpath(localPath);

    QFileInfoList list = dir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot);
    for (const QFileInfo &info : list) {
        QString name = info.fileName();
        QString childUrl = dirUrl;
        if (!childUrl.endsWith('/'))
            childUrl += '/';
        childUrl += name;

        if (info.isDir()) {
            LOG_INFO(QString("进入子目录: %1").arg(childUrl));
            QString subLocal = QDir(localPath).filePath(name);
            scanDirectory(childUrl, subLocal);
        } else {
            LOG_INFO(QString("添加文件任务: %1").arg(childUrl));
            QString taskId = m_manager->addTask(childUrl, localPath);
            m_manager->startTask(taskId);
        }
    }
}

