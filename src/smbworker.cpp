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
#include <smb2/libsmb2.h>
#include "smbutils.h"

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

    struct smb2_context *ctx = smb2_init_context();
    if (!ctx) {
        emit finished(false, QObject::tr("SMB2 上下文创建失败"));
        return;
    }

    QString userStr = m_task->username();
    QString domStr = m_task->domain();
    if (domStr.isEmpty())
        parseDomainUser(userStr, domStr, userStr);

    QByteArray user = userStr.toUtf8();
    QByteArray pass = m_task->password().toUtf8();
    QByteArray dom  = domStr.toUtf8();
    if (!dom.isEmpty())
        smb2_set_domain(ctx, dom.constData());
    if (!user.isEmpty())
        smb2_set_user(ctx, user.constData());
    if (!pass.isEmpty())
        smb2_set_password(ctx, pass.constData());

    QStringList segments = url.path().split('/', Qt::SkipEmptyParts);
    if (segments.isEmpty()) {
        smb2_destroy_context(ctx);
        emit finished(false, QObject::tr("无效的 SMB 路径"));
        return;
    }
    QString share = segments.takeFirst();
    QString remotePath = "/" + segments.join('/');

    if (smb2_connect_share(ctx, url.host().toUtf8().constData(),
                           share.toUtf8().constData(), user.isEmpty() ? NULL : user.constData()) < 0) {
        QString err = QString::fromLocal8Bit(smb2_get_error(ctx));
        smb2_destroy_context(ctx);
        emit finished(false, err);
        return;
    }

    struct smb2_stat_64 st;
    if (smb2_stat(ctx, remotePath.toUtf8().constData(), &st) == 0) {
        emit progress(m_offset, st.smb2_size);
    } else {
        st.smb2_size = 0;
    }

    struct smb2fh *remote = smb2_open(ctx, remotePath.toUtf8().constData(), O_RDONLY);
    if (!remote) {
        QString err = QString::fromLocal8Bit(smb2_get_error(ctx));
        smb2_disconnect_share(ctx);
        smb2_destroy_context(ctx);
        emit finished(false, err);
        return;
    }

    if (m_offset > 0) {
        uint64_t current = 0;
        if (smb2_lseek(ctx, remote, m_offset, SEEK_SET, &current) < 0) {
            QString err = QString::fromLocal8Bit(smb2_get_error(ctx));
            smb2_close(ctx, remote);
            smb2_disconnect_share(ctx);
            smb2_destroy_context(ctx);
            emit finished(false, err);
            return;
        }
    }

    const int bufSize = 4096;
    uint8_t buf[bufSize];
    qint64 received = m_offset;
    qint64 total = st.smb2_size;

    while (!m_cancelRequested) {
        if (m_pauseRequested) {
            msleep(100);
            continue;
        }

        int n = smb2_read(ctx, remote, buf, bufSize);
        if (n < 0) {
            QString err = QString::fromLocal8Bit(smb2_get_error(ctx));
            smb2_close(ctx, remote);
            smb2_disconnect_share(ctx);
            smb2_destroy_context(ctx);
            emit finished(false, err);
            return;
        }
        if (n == 0)
            break;
        if (file.write(reinterpret_cast<char*>(buf), n) != n) {
            smb2_close(ctx, remote);
            smb2_disconnect_share(ctx);
            smb2_destroy_context(ctx);
            emit finished(false, QObject::tr("写入文件失败"));
            return;
        }
        received += n;
        emit progress(received, total);
    }

    smb2_close(ctx, remote);
    smb2_disconnect_share(ctx);
    smb2_destroy_context(ctx);
    file.close();

    if (m_cancelRequested) {
        emit finished(false, QObject::tr("用户取消"));
    } else {
        emit finished(true, QString());
    }
}

