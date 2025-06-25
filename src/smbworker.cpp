#include "smbworker.h"
#include "downloadtask.h"
#include <QFile>
#include <QDir>
#include <QUrl>
#include <QFileInfo>
#include <QThread>
#include <QDebug>
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

SmbWorker::SmbWorker(DownloadTask *task, QObject *parent)
    : QThread(parent), m_task(task), m_pauseRequested(false),
      m_cancelRequested(false), m_offset(0)
{
}

void SmbWorker::requestPause()
{
    m_pauseRequested = true;
}

void SmbWorker::requestCancel()
{
    m_cancelRequested = true;
}

void SmbWorker::resumeWork()
{
    m_pauseRequested = false;
}

void SmbWorker::run()
{
    if (!m_task)
        return;

    QUrl url(m_task->url());
    QString filePath = m_task->savePath();
    if (!filePath.endsWith('/') && !filePath.endsWith('\\'))
        filePath += '/';
    QString fileName = url.fileName();
    if (fileName.isEmpty())
        fileName = "downloaded_file";
    filePath += fileName;

    QFileInfo info(filePath);
    QDir().mkpath(info.absolutePath());

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append)) {
        emit finished(false, QObject::tr("无法创建文件"));
        return;
    }

    m_offset = file.size();

    QString unc = toUncPath(m_task->url());
    LOG_INFO(QString("SmbWorker 尝试打开远程文件: %1").arg(unc));
    QFile remoteFile(unc);
    LOG_INFO("SmbWorker: remoteFile.open() 前");
    if (!remoteFile.open(QIODevice::ReadOnly)) {
        LOG_ERROR(QString("SmbWorker 打开失败: %1").arg(remoteFile.errorString()));
        file.close();
        emit finished(false, QObject::tr("无法打开远程文件: %1").arg(remoteFile.errorString()));
        return;
    }
    LOG_INFO("SmbWorker: remoteFile.open() 成功");

    if (m_offset > 0 && !remoteFile.seek(m_offset)) {
        remoteFile.close();
        file.close();
        LOG_ERROR("SmbWorker: remoteFile.seek() 失败");
        emit finished(false, QObject::tr("无法定位远程文件"));
        return;
    }

    qint64 total = remoteFile.size();
    LOG_INFO(QString("SmbWorker: remoteFile.size() = %1").arg(total));
    emit progress(m_offset, total);

    const int bufSize = 65536;
    char buf[bufSize];
    qint64 received = m_offset;

    while (!m_cancelRequested) {
        if (m_pauseRequested) {
            msleep(100);
            continue;
        }
        qint64 n = remoteFile.read(buf, bufSize);
        if (n < 0) {
            remoteFile.close();
            file.close();
            LOG_ERROR(QString("SmbWorker: 读取数据失败: %1").arg(remoteFile.errorString()));
            emit finished(false, remoteFile.errorString());
            return;
        }
        if (n == 0)
            break;
        if (file.write(buf, n) != n) {
            remoteFile.close();
            file.close();
            LOG_ERROR("SmbWorker: 写入文件失败");
            emit finished(false, QObject::tr("写入文件失败"));
            return;
        }
        received += n;
        emit progress(received, total);
    }

    remoteFile.close();
    file.close();

    if (m_cancelRequested) {
        emit finished(false, QObject::tr("用户取消"));
    } else {
        emit finished(true, QString());
    }
}

