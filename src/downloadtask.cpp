#include "downloadtask.h"
#include <QDebug>
#include <QFileInfo>
#include <QUrl>
#include <QUuid>
#include "logger.h"

DownloadTask::DownloadTask(QObject *parent)
    : QObject(parent)
    , m_status(Pending)
    , m_progress(0.0)
    , m_downloadedSize(0)
    , m_totalSize(0)
    , m_speed(0)
    , m_supportsResume(false)
    , m_protocol(SMB)
{
    LOG_DEBUG("创建新的下载任务");
    generateId();
}

DownloadTask::DownloadTask(const QString &url, const QString &savePath, QObject *parent)
    : DownloadTask(parent)
{
    setUrl(url);
    setSavePath(savePath);
}

DownloadTask::DownloadTask(const QString &url, const QString &savePath, ProtocolType protocol, QObject *parent)
    : DownloadTask(url, savePath, parent)
{
    setProtocol(protocol);
}

DownloadTask::~DownloadTask()
{
    LOG_DEBUG(QString("销毁下载任务 - ID: %1").arg(m_id));
}

void DownloadTask::setUrl(const QString &url)
{
    m_url = url;
    
    // 从 URL 中提取文件名
    QUrl urlObj(url);
    QString fileName = urlObj.fileName();
    if (fileName.isEmpty()) {
        fileName = "downloaded_file";
    }
    setFileName(fileName);
    
    // 根据 URL 协议自动检测协议类型
    if (url.startsWith("smb://", Qt::CaseInsensitive)) {
        setProtocol(SMB);
    } else if (url.startsWith("http://", Qt::CaseInsensitive) || 
               url.startsWith("https://", Qt::CaseInsensitive)) {
        setProtocol(HTTP_HTTPS);
    }
}

void DownloadTask::setSavePath(const QString &path)
{
    m_savePath = path;
}

void DownloadTask::setFileName(const QString &name)
{
    m_fileName = name;
}

void DownloadTask::setStatus(Status status)
{
    if (m_status != status) {
        m_status = status;
        emit statusChanged(status);
        
        // 记录状态变化
        QString statusText;
        switch (status) {
        case Pending: statusText = "等待中"; break;
        case Queued: statusText = "排队中"; break;
        case Downloading: statusText = "下载中"; break;
        case Paused: statusText = "暂停"; break;
        case Completed: statusText = "完成"; break;
        case Failed: statusText = "失败"; break;
        case Cancelled: statusText = "取消"; break;
        }
        
        LOG_INFO(QString("任务状态变化 - ID: %1, 状态: %2").arg(m_id).arg(statusText));
    }
}

void DownloadTask::setProgress(double progress)
{
    if (qAbs(m_progress - progress) > 0.01) { // 避免频繁更新
        m_progress = qBound(0.0, progress, 100.0);
        emit progressChanged(m_progress);
    }
}

void DownloadTask::setDownloadedSize(qint64 size)
{
    m_downloadedSize = size;
    
    // 更新进度
    if (m_totalSize > 0) {
        setProgress((double)m_downloadedSize / m_totalSize * 100.0);
    }
}

void DownloadTask::setTotalSize(qint64 size)
{
    m_totalSize = size;
    
    // 更新进度
    if (m_totalSize > 0) {
        setProgress((double)m_downloadedSize / m_totalSize * 100.0);
    }
}

void DownloadTask::setSpeed(qint64 speed)
{
    if (m_speed != speed) {
        m_speed = speed;
        emit speedChanged(speed);
    }
}

void DownloadTask::setErrorMessage(const QString &message)
{
    m_errorMessage = message;
    LOG_ERROR(QString("任务错误 - ID: %1, 错误: %2").arg(m_id).arg(message));
    emit errorOccurred(message);
}

void DownloadTask::generateId()
{
    m_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
}

void DownloadTask::setProtocol(ProtocolType protocol)
{
    if (m_protocol != protocol) {
        m_protocol = protocol;
    }
}

QString DownloadTask::protocolText() const
{
    switch (m_protocol) {
    case SMB:
        return QObject::tr("SMB");
    case HTTP_HTTPS:
        return QObject::tr("HTTP/HTTPS");
    default:
        return QObject::tr("未知");
    }
}

QString DownloadTask::statusText() const
{
    switch (m_status) {
    case Pending:
        return QObject::tr("等待中");
    case Queued:
        return QObject::tr("排队中");
    case Downloading:
        return QObject::tr("下载中");
    case Paused:
        return QObject::tr("暂停");
    case Completed:
        return QObject::tr("完成");
    case Failed:
        return QObject::tr("失败");
    case Cancelled:
        return QObject::tr("取消");
    default:
        return QObject::tr("未知");
    }
}

QString DownloadTask::speedText() const
{
    if (m_speed <= 0) {
        return QObject::tr("0 B/s");
    }
    
    const qint64 KB = 1024;
    const qint64 MB = 1024 * KB;
    const qint64 GB = 1024 * MB;
    
    if (m_speed >= GB) {
        return QString("%1 GB/s").arg(static_cast<double>(m_speed) / GB, 0, 'f', 2);
    } else if (m_speed >= MB) {
        return QString("%1 MB/s").arg(static_cast<double>(m_speed) / MB, 0, 'f', 2);
    } else if (m_speed >= KB) {
        return QString("%1 KB/s").arg(static_cast<double>(m_speed) / KB, 0, 'f', 2);
    } else {
        return QString("%1 B/s").arg(m_speed);
    }
}

QString DownloadTask::timeRemainingText() const
{
    if (m_speed <= 0 || m_progress <= 0) {
        return QObject::tr("计算中...");
    }
    
    qint64 remainingBytes = m_totalSize - m_downloadedSize;
    if (remainingBytes <= 0) {
        return QObject::tr("完成");
    }
    
    qint64 remainingSeconds = remainingBytes / m_speed;
    if (remainingSeconds < 60) {
        return QString("%1秒").arg(remainingSeconds);
    } else if (remainingSeconds < 3600) {
        return QString("%1分钟").arg(remainingSeconds / 60);
    } else {
        return QString("%1小时").arg(remainingSeconds / 3600);
    }
} 