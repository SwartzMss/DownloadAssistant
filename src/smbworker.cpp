#include "smbworker.h"
#include "downloadtask.h"
#include <QFile>
#include <QDir>
#include <QUrl>
#include <QFileInfo>
#include <QThread>
#include <QDebug>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>

static thread_local QString t_username;
static thread_local QString t_password;

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

void SmbWorker::authCallback(SMBCCTX *ctx, const char *srv, const char *shr,
                             char *wg, int wglen,
                             char *un, int unlen,
                             char *pw, int pwlen)
{
    Q_UNUSED(ctx);
    Q_UNUSED(srv);
    Q_UNUSED(shr);
    if (wg && wglen > 0)
        wg[0] = '\0';
    if (un && unlen > 0) {
        QByteArray u = t_username.toUtf8();
        strncpy(un, u.constData(), unlen - 1);
        un[unlen - 1] = '\0';
    }
    if (pw && pwlen > 0) {
        QByteArray p = t_password.toUtf8();
        strncpy(pw, p.constData(), pwlen - 1);
        pw[pwlen - 1] = '\0';
    }
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

    t_username = m_task->username();
    t_password = m_task->password();

    if (smbc_init(authCallback, 0) < 0) {
        emit finished(false, QString::fromLocal8Bit(strerror(errno)));
        return;
    }

    SMBCCTX *ctx = smbc_get_context();
    if (!ctx) {
        emit finished(false, QObject::tr("SMB 上下文创建失败"));
        return;
    }

    smbc_setOptionUserData(ctx, this);

    QByteArray urlData = url.toString().toUtf8();
    SMBCFILE *remote = smbc_open(urlData.constData(), O_RDONLY, 0);
    if (!remote) {
        emit finished(false, QString::fromLocal8Bit(strerror(errno)));
        return;
    }

    struct stat st;
    if (smbc_stat(urlData.constData(), &st) == 0) {
        emit progress(m_offset, st.st_size);
    }

    if (m_offset > 0) {
        smbc_lseek(remote, m_offset, SEEK_SET);
    }

    const int bufSize = 4096;
    char buf[bufSize];
    qint64 received = m_offset;
    qint64 total = st.st_size;

    while (!m_cancelRequested) {
        if (m_pauseRequested) {
            msleep(100);
            continue;
        }

        int n = smbc_read(remote, buf, bufSize);
        if (n < 0) {
            emit finished(false, QString::fromLocal8Bit(strerror(errno)));
            smbc_close(remote);
            return;
        }
        if (n == 0)
            break;
        if (file.write(buf, n) != n) {
            emit finished(false, QObject::tr("写入文件失败"));
            smbc_close(remote);
            return;
        }
        received += n;
        emit progress(received, total);
    }

    smbc_close(remote);
    file.close();

    if (m_cancelRequested) {
        emit finished(false, QObject::tr("用户取消"));
    } else {
        emit finished(true, QString());
    }
}

