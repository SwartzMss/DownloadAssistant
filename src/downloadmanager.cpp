#include "downloadmanager.h"
#include "downloadtask.h"
#include "smbdownloader.h"
#include <QStandardPaths>
#include <QDir>
#include <QDebug>
#include <QUuid>
#include <QCoreApplication>
#include "logger.h"

DownloadManager::DownloadManager(QObject *parent)
    : QObject(parent)
    , m_smbDownloader(new SmbDownloader(this))
    , m_settings(new QSettings(QCoreApplication::applicationDirPath() + "/config.ini", QSettings::IniFormat, this))
    , m_maxConcurrentDownloads(3)
    , m_activeDownloadCount(0)
{
    LOG_INFO("DownloadManager 初始化开始");
    
    // 设置默认保存路径
    m_defaultSavePath = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (m_defaultSavePath.isEmpty()) {
        m_defaultSavePath = QDir::homePath() + "/Downloads";
    }
    
    // 从设置中加载配置
    m_defaultSavePath = m_settings->value("defaultSavePath", m_defaultSavePath).toString();
    m_maxConcurrentDownloads = m_settings->value("maxConcurrentDownloads", 3).toInt();
    
    LOG_INFO(QString("默认保存路径: %1").arg(m_defaultSavePath));
    LOG_INFO(QString("最大并发下载数: %1").arg(m_maxConcurrentDownloads));
    
    // 连接下载器信号
    connect(m_smbDownloader, &SmbDownloader::downloadStarted,
            this, &DownloadManager::onDownloadStarted);
    connect(m_smbDownloader, &SmbDownloader::downloadPaused,
            this, &DownloadManager::onDownloadPaused);
    connect(m_smbDownloader, &SmbDownloader::downloadResumed,
            this, &DownloadManager::onDownloadResumed);
    connect(m_smbDownloader, &SmbDownloader::downloadCancelled,
            this, &DownloadManager::onDownloadCancelled);
    connect(m_smbDownloader, &SmbDownloader::downloadCompleted,
            this, &DownloadManager::onDownloadCompleted);
    connect(m_smbDownloader, &SmbDownloader::downloadFailed,
            this, &DownloadManager::onDownloadFailed);
    
    // 加载保存的任务
    loadTasks();
    
    LOG_INFO("DownloadManager 初始化完成");
}

DownloadManager::~DownloadManager()
{
    LOG_INFO("DownloadManager 析构");
    saveTasks();
    
    // 清理任务
    for (DownloadTask *task : m_tasks) {
        task->deleteLater();
    }
    m_tasks.clear();
}

QString DownloadManager::addTask(const QString &url, const QString &savePath, DownloadTask::ProtocolType protocol)
{
    LOG_INFO(QString("添加下载任务 - URL: %1, 协议: %2").arg(url).arg(static_cast<int>(protocol)));
    
    QString taskId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    
    DownloadTask *task = new DownloadTask(this);
    task->setId(taskId);
    task->setUrl(url);
    task->setSavePath(savePath.isEmpty() ? m_defaultSavePath : savePath);
    task->setProtocol(protocol);
    task->setStatus(DownloadTask::Pending);
    
    m_tasks[taskId] = task;
    
    LOG_INFO(QString("任务已添加 - ID: %1").arg(taskId));
    
    emit taskAdded(taskId);
    saveTasks();
    
    return taskId;
}

void DownloadManager::removeTask(const QString &taskId)
{
    LOG_INFO(QString("移除下载任务 - ID: %1").arg(taskId));
    
    if (!m_tasks.contains(taskId)) {
        LOG_WARNING(QString("任务不存在 - ID: %1").arg(taskId));
        return;
    }
    
    DownloadTask *task = m_tasks[taskId];
    if (task->status() == DownloadTask::Downloading) {
        m_activeDownloadCount--;
    }
    
    // 取消下载
    if (task->status() == DownloadTask::Completed ||
        task->status() == DownloadTask::Failed ||
        task->status() == DownloadTask::Cancelled) {
        // 已完成的任务直接删除
        m_tasks.remove(taskId);
        task->deleteLater();
    } else {
        // 正在进行的任务先取消再删除
        cancelTask(taskId);
        m_tasks.remove(taskId);
        task->deleteLater();
    }
    
    LOG_INFO(QString("任务已移除 - ID: %1").arg(taskId));
    saveTasks();
}

void DownloadManager::removeCompletedTasks()
{
    LOG_INFO("移除已完成的任务");
    
    QList<DownloadTask*> completedTasks;
    for (DownloadTask *task : m_tasks) {
        if (task->status() == DownloadTask::Completed ||
            task->status() == DownloadTask::Failed ||
            task->status() == DownloadTask::Cancelled) {
            completedTasks.append(task);
        }
    }
    
    for (DownloadTask *task : completedTasks) {
        removeTask(task->id());
    }
}

void DownloadManager::startTask(const QString &taskId)
{
    LOG_INFO(QString("开始下载任务 - ID: %1").arg(taskId));
    
    if (!m_tasks.contains(taskId)) {
        LOG_WARNING(QString("任务不存在 - ID: %1").arg(taskId));
        return;
    }
    
    DownloadTask *task = m_tasks[taskId];
    if (task->status() == DownloadTask::Downloading) {
        LOG_WARNING(QString("任务已在下载中 - ID: %1").arg(taskId));
        return;
    }
    
    if (m_activeDownloadCount >= m_maxConcurrentDownloads) {
        LOG_INFO(QString("达到最大并发数，任务进入队列 - ID: %1").arg(taskId));
        task->setStatus(DownloadTask::Queued);
        return;
    }
    
    m_activeDownloadCount++;
    task->setStatus(DownloadTask::Downloading);
    
    LOG_INFO(QString("任务开始下载 - ID: %1, 当前活跃下载数: %2").arg(taskId).arg(m_activeDownloadCount));
    
    // 根据协议类型选择下载器
    if (task->protocol() == DownloadTask::SMB) {
        m_smbDownloader->startDownload(task);
    } else {
        LOG_WARNING(QString("不支持的协议类型 - ID: %1, 协议: %2").arg(taskId).arg(static_cast<int>(task->protocol())));
    }
}

void DownloadManager::pauseTask(const QString &taskId)
{
    LOG_INFO(QString("暂停下载任务 - ID: %1").arg(taskId));
    
    if (!m_tasks.contains(taskId)) {
        LOG_WARNING(QString("任务不存在 - ID: %1").arg(taskId));
        return;
    }
    
    DownloadTask *task = m_tasks[taskId];
    if (task->status() != DownloadTask::Downloading) {
        LOG_WARNING(QString("任务不在下载状态 - ID: %1").arg(taskId));
        return;
    }
    
    task->setStatus(DownloadTask::Paused);
    m_activeDownloadCount--;
    
    LOG_INFO(QString("任务已暂停 - ID: %1, 当前活跃下载数: %2").arg(taskId).arg(m_activeDownloadCount));
    
    if (task->protocol() == DownloadTask::SMB) {
        m_smbDownloader->pauseDownload(task);
    }
    
    processNextTask();
}

void DownloadManager::resumeTask(const QString &taskId)
{
    LOG_INFO(QString("恢复下载任务 - ID: %1").arg(taskId));
    
    if (!m_tasks.contains(taskId)) {
        LOG_WARNING(QString("任务不存在 - ID: %1").arg(taskId));
        return;
    }
    
    DownloadTask *task = m_tasks[taskId];
    if (task->status() != DownloadTask::Paused) {
        LOG_WARNING(QString("任务不在暂停状态 - ID: %1").arg(taskId));
        return;
    }
    
    if (m_activeDownloadCount >= m_maxConcurrentDownloads) {
        LOG_INFO(QString("达到最大并发数，任务进入队列 - ID: %1").arg(taskId));
        task->setStatus(DownloadTask::Queued);
        return;
    }
    
    m_activeDownloadCount++;
    task->setStatus(DownloadTask::Downloading);
    
    LOG_INFO(QString("任务恢复下载 - ID: %1, 当前活跃下载数: %2").arg(taskId).arg(m_activeDownloadCount));
    
    if (task->protocol() == DownloadTask::SMB) {
        m_smbDownloader->resumeDownload(task);
    }
}

void DownloadManager::cancelTask(const QString &taskId)
{
    LOG_INFO(QString("取消下载任务 - ID: %1").arg(taskId));
    
    if (!m_tasks.contains(taskId)) {
        LOG_WARNING(QString("任务不存在 - ID: %1").arg(taskId));
        return;
    }
    
    DownloadTask *task = m_tasks[taskId];
    if (task->status() == DownloadTask::Downloading) {
        m_activeDownloadCount--;
    }
    
    task->setStatus(DownloadTask::Cancelled);
    
    LOG_INFO(QString("任务已取消 - ID: %1, 当前活跃下载数: %2").arg(taskId).arg(m_activeDownloadCount));
    
    if (task->protocol() == DownloadTask::SMB) {
        m_smbDownloader->cancelDownload(task);
    }
    
    processNextTask();
}

void DownloadManager::startAllTasks()
{
    LOG_INFO("开始所有任务");
    
    for (DownloadTask *task : m_tasks) {
        if (task->status() == DownloadTask::Pending ||
            task->status() == DownloadTask::Paused) {
            startTask(task->id());
        }
    }
}

void DownloadManager::pauseAllTasks()
{
    LOG_INFO("暂停所有任务");
    
    for (DownloadTask *task : m_tasks) {
        if (task->status() == DownloadTask::Downloading) {
            pauseTask(task->id());
        }
    }
}

void DownloadManager::cancelAllTasks()
{
    LOG_INFO("取消所有任务");
    
    for (DownloadTask *task : m_tasks) {
        if (task->status() == DownloadTask::Downloading ||
            task->status() == DownloadTask::Paused) {
            cancelTask(task->id());
        }
    }
}

QList<DownloadTask*> DownloadManager::getAllTasks() const
{
    return m_tasks.values();
}

QList<DownloadTask*> DownloadManager::getActiveTasks() const
{
    QList<DownloadTask*> activeTasks;
    for (DownloadTask *task : m_tasks) {
        if (task->status() == DownloadTask::Downloading ||
            task->status() == DownloadTask::Paused) {
            activeTasks.append(task);
        }
    }
    return activeTasks;
}

QList<DownloadTask*> DownloadManager::getCompletedTasks() const
{
    QList<DownloadTask*> completedTasks;
    for (DownloadTask *task : m_tasks) {
        if (task->status() == DownloadTask::Completed) {
            completedTasks.append(task);
        }
    }
    return completedTasks;
}

QList<DownloadTask*> DownloadManager::getFailedTasks() const
{
    QList<DownloadTask*> failedTasks;
    for (DownloadTask *task : m_tasks) {
        if (task->status() == DownloadTask::Failed) {
            failedTasks.append(task);
        }
    }
    return failedTasks;
}

DownloadTask* DownloadManager::getTask(const QString &taskId) const
{
    return m_tasks.value(taskId, nullptr);
}

QString DownloadManager::getDefaultSavePath() const
{
    return m_defaultSavePath;
}

void DownloadManager::setDefaultSavePath(const QString &path)
{
    m_defaultSavePath = path;
    m_settings->setValue("defaultSavePath", path);
}

int DownloadManager::getMaxConcurrentDownloads() const
{
    return m_maxConcurrentDownloads;
}

void DownloadManager::setMaxConcurrentDownloads(int max)
{
    m_maxConcurrentDownloads = max;
    m_settings->setValue("maxConcurrentDownloads", max);
}

void DownloadManager::saveTasks()
{
    LOG_INFO("保存任务列表");
    
    m_settings->beginGroup("Tasks");
    m_settings->remove(""); // 清除旧的任务数据
    
    int index = 0;
    for (DownloadTask *task : m_tasks) {
        QString prefix = QString("Task%1/").arg(index++);
        m_settings->setValue(prefix + "id", task->id());
        m_settings->setValue(prefix + "url", task->url());
        m_settings->setValue(prefix + "savePath", task->savePath());
        m_settings->setValue(prefix + "fileName", task->fileName());
        m_settings->setValue(prefix + "protocol", static_cast<int>(task->protocol()));
        m_settings->setValue(prefix + "status", static_cast<int>(task->status()));
        m_settings->setValue(prefix + "downloadedSize", task->downloadedSize());
        m_settings->setValue(prefix + "totalSize", task->totalSize());
        m_settings->setValue(prefix + "supportsResume", task->supportsResume());
    }
    
    m_settings->setValue("count", index);
    m_settings->endGroup();
    
    LOG_INFO(QString("已保存 %1 个任务").arg(index));
}

void DownloadManager::loadTasks()
{
    LOG_INFO("加载任务列表");
    
    m_settings->beginGroup("Tasks");
    int count = m_settings->value("count", 0).toInt();
    
    for (int i = 0; i < count; ++i) {
        QString prefix = QString("Task%1/").arg(i);
        
        QString id = m_settings->value(prefix + "id").toString();
        QString url = m_settings->value(prefix + "url").toString();
        QString savePath = m_settings->value(prefix + "savePath").toString();
        QString fileName = m_settings->value(prefix + "fileName").toString();
        DownloadTask::ProtocolType protocol = static_cast<DownloadTask::ProtocolType>(
            m_settings->value(prefix + "protocol", 0).toInt());
        DownloadTask::Status status = static_cast<DownloadTask::Status>(
            m_settings->value(prefix + "status", 0).toInt());
        qint64 downloadedSize = m_settings->value(prefix + "downloadedSize", 0).toLongLong();
        qint64 totalSize = m_settings->value(prefix + "totalSize", 0).toLongLong();
        bool supportsResume = m_settings->value(prefix + "supportsResume", false).toBool();
        
        // 创建任务对象
        DownloadTask *task = new DownloadTask(this);
        task->setId(id);
        task->setUrl(url);
        task->setSavePath(savePath);
        task->setFileName(fileName);
        task->setProtocol(protocol);
        task->setStatus(status);
        task->setDownloadedSize(downloadedSize);
        task->setTotalSize(totalSize);
        task->setSupportsResume(supportsResume);
        
        m_tasks[id] = task;
        
        LOG_INFO(QString("加载任务 - ID: %1, URL: %2").arg(id).arg(url));
    }
    
    m_settings->endGroup();
    
    LOG_INFO(QString("已加载 %1 个任务").arg(count));
}

void DownloadManager::onTaskStatusChanged(DownloadTask::Status status)
{
    updateActiveDownloadCount();
    
    // 如果任务完成，尝试开始下一个任务
    if (status == DownloadTask::Completed || 
        status == DownloadTask::Failed || 
        status == DownloadTask::Cancelled) {
        processNextTask();
    }
}

void DownloadManager::onDownloadStarted(DownloadTask *task)
{
    LOG_INFO(QString("下载开始 - ID: %1").arg(task->id()));
    emit taskStarted(task->id());
}

void DownloadManager::onDownloadPaused(DownloadTask *task)
{
    LOG_INFO(QString("下载暂停 - ID: %1").arg(task->id()));
    emit taskPaused(task->id());
}

void DownloadManager::onDownloadResumed(DownloadTask *task)
{
    LOG_INFO(QString("下载恢复 - ID: %1").arg(task->id()));
    emit taskResumed(task->id());
}

void DownloadManager::onDownloadCancelled(DownloadTask *task)
{
    LOG_INFO(QString("下载取消 - ID: %1").arg(task->id()));
    m_activeDownloadCount--;
    emit taskCancelled(task->id());
    processNextTask();
}

void DownloadManager::onDownloadCompleted(DownloadTask *task)
{
    LOG_INFO(QString("下载完成 - ID: %1").arg(task->id()));
    m_activeDownloadCount--;
    task->setStatus(DownloadTask::Completed);
    emit taskCompleted(task->id());
    processNextTask();
}

void DownloadManager::onDownloadFailed(DownloadTask *task, const QString &error)
{
    LOG_ERROR(QString("下载失败 - ID: %1, 错误: %2").arg(task->id()).arg(error));
    m_activeDownloadCount--;
    task->setStatus(DownloadTask::Failed);
    task->setErrorMessage(error);
    emit taskFailed(task->id(), error);
    processNextTask();
}

void DownloadManager::processNextTask()
{
    LOG_DEBUG("处理下一个任务");
    
    if (m_activeDownloadCount >= m_maxConcurrentDownloads) {
        LOG_DEBUG("已达到最大并发数，跳过处理");
        return;
    }
    
    for (DownloadTask *task : m_tasks) {
        if (task->status() == DownloadTask::Pending ||
            task->status() == DownloadTask::Queued) {
            LOG_INFO(QString("开始处理排队任务 - ID: %1").arg(task->id()));
            startTask(task->id());
            break;
        }
    }
}

void DownloadManager::updateActiveDownloadCount()
{
    m_activeDownloadCount = 0;
    for (DownloadTask *task : m_tasks) {
        if (task->status() == DownloadTask::Downloading) {
            m_activeDownloadCount++;
        }
    }
}

