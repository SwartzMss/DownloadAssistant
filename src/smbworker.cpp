#include "smbworker.h"
#include "downloadtask.h"
#include <QFile>
#include <QDir>
#include <QUrl>
#include <QFileInfo>
#include <QThread>
#include <QDebug>
#include <QMutexLocker>
#include "logger.h"
#include "pathutils.h"

SmbWorker::SmbWorker(DownloadTask *task, QFile *file, QMutex *mutex,
                     qint64 startOffset, qint64 chunkSize, QObject *parent)
    : QThread(parent), m_task(task), m_file(file), m_mutex(mutex),
      m_pauseRequested(false), m_cancelRequested(false),
      m_startOffset(startOffset), m_chunkSize(chunkSize), m_bytesReceived(0)
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
    if (!m_task || !m_file || !m_mutex)
        return;

    QString unc = toUncPath(m_task->url());
    QFile remoteFile(unc);
    if (!remoteFile.open(QIODevice::ReadOnly)) {
        emit finished(false, QObject::tr("无法打开远程文件"));
        return;
    }

    if (!remoteFile.seek(m_startOffset)) {
        remoteFile.close();
        emit finished(false, QObject::tr("无法定位远程文件"));
        return;
    }

    if (m_chunkSize <= 0)
        m_chunkSize = remoteFile.size() - m_startOffset;

    emit progress(0, m_chunkSize);

    const int bufSize = 524288; // 512KB
    char buf[bufSize];
    qint64 remaining = m_chunkSize;

    while (!m_cancelRequested && remaining > 0) {
        if (m_pauseRequested) {
            msleep(100);
            continue;
        }

        qint64 toRead = qMin<qint64>(bufSize, remaining);
        qint64 n = remoteFile.read(buf, toRead);
        if (n <= 0) {
            if (n < 0)
                emit finished(false, remoteFile.errorString());
            break;
        }

        {
            QMutexLocker locker(m_mutex);
            if (!m_file->seek(m_startOffset + m_bytesReceived)) {
                emit finished(false, QObject::tr("写入文件失败"));
                remoteFile.close();
                return;
            }
            if (m_file->write(buf, n) != n) {
                emit finished(false, QObject::tr("写入文件失败"));
                remoteFile.close();
                return;
            }
        }

        m_bytesReceived += n;
        remaining -= n;
        emit progress(m_bytesReceived, m_chunkSize);
    }

    remoteFile.close();

    if (m_cancelRequested) {
        emit finished(false, QObject::tr("用户取消"));
    } else {
        emit finished(true, QString());
    }
}

