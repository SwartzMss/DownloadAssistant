#include "downloadmanager.h"
#include "downloadtask.h"
#include "smbdownloader.h"
#include <QStandardPaths>
#include <QDir>
#include <QDebug>
#include <QUuid>
#include <QCoreApplication>
#include "logger.h"
#include <QFile>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

DownloadManager::DownloadManager(QObject *parent)
    : QObject(parent)
    , m_smbDownloader(new SmbDownloader(this))
    , m_configPath(QCoreApplication::applicationDirPath() + "/config.json")
    , m_maxConcurrentDownloads(5)
    , m_activeDownloadCount(0)
{
    LOG_INFO("DownloadManager 初始化开始");
    
    // 设置默认保存路径
    m_defaultSavePath = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (m_defaultSavePath.isEmpty()) {
        m_defaultSavePath = QDir::homePath() + "/Downloads";
    }
    
    // 从配置文件中加载配置
    loadFromJson();
    
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
    connect(m_smbDownloader, &SmbDownloader::downloadProgress,
            this, &DownloadManager::onDownloadProgress);

    
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

QString DownloadManager::addTask(const QString &url,
                                 const QString &savePath)
{
    LOG_INFO(QString("添加下载任务 - URL: %1").arg(url));
    
    QString taskId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    
    DownloadTask *task = new DownloadTask(this);
    task->setId(taskId);
    task->setUrl(url);
    task->setSavePath(savePath.isEmpty() ? m_defaultSavePath : savePath);
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
    emit taskRemoved(taskId);
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
        m_tasks.remove(task->id());
        task->deleteLater();
        LOG_INFO(QString("任务已移除 - ID: %1").arg(task->id()));
        emit taskRemoved(task->id());
    }
    saveTasks();
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
    m_smbDownloader->startDownload(task);
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
    
    m_smbDownloader->pauseDownload(task);

    processNextTask();
    saveTasks();
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

    LOG_INFO(QString("任务恢复下载 - ID: %1, 当前活跃下载数: %2").arg(taskId).arg(m_activeDownloadCount));

    // 调用下载器恢复任务，状态将在下载器中更新
    m_smbDownloader->resumeDownload(task);
    saveTasks();
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
    task->setErrorMessage(tr("用户取消"));
    
    LOG_INFO(QString("任务已取消 - ID: %1, 当前活跃下载数: %2").arg(taskId).arg(m_activeDownloadCount));
    
    m_smbDownloader->cancelDownload(task);
    
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
        if (task->status() == DownloadTask::Failed || task->status() == DownloadTask::Cancelled) {
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
    saveTasks();
}

int DownloadManager::getMaxConcurrentDownloads() const
{
    return m_maxConcurrentDownloads;
}

void DownloadManager::setMaxConcurrentDownloads(int max)
{
    if (max < 3) {
        max = 3;
    } else if (max > 8) {
        max = 8;
    }
    m_maxConcurrentDownloads = max;
    saveTasks();
}

void DownloadManager::saveTasks()
{
    LOG_INFO("保存任务列表");
    
    QJsonObject json;
    QJsonArray tasksArray;
    
    for (DownloadTask *task : m_tasks) {
        QJsonObject taskObject;
        taskObject["id"] = task->id();
        taskObject["url"] = task->url();
        taskObject["savePath"] = task->savePath();
        taskObject["fileName"] = task->fileName();
        taskObject["status"] = static_cast<int>(task->status());
        taskObject["downloadedSize"] = task->downloadedSize();
        taskObject["totalSize"] = task->totalSize();
        taskObject["supportsResume"] = task->supportsResume();
        taskObject["errorMessage"] = task->errorMessage();
        tasksArray.append(taskObject);
    }
    
    json["tasks"] = tasksArray;
    json["defaultSavePath"] = m_defaultSavePath;
    json["maxConcurrentDownloads"] = m_maxConcurrentDownloads;
    
    QFile file(m_configPath);
    if (file.open(QIODevice::WriteOnly)) {
        QTextStream stream(&file);
        stream << QJsonDocument(json).toJson();
        file.close();
    }
    
    LOG_INFO(QString("已保存 %1 个任务").arg(m_tasks.size()));
}

void DownloadManager::loadTasks()
{
    LOG_INFO("加载任务列表");

    // 清空旧任务对象，防止重复加载
    for (auto task : m_tasks) {
        task->deleteLater();
    }
    m_tasks.clear();

    QFile file(m_configPath);
    if (file.open(QIODevice::ReadOnly)) {
        QTextStream stream(&file);
        QString jsonString = stream.readAll();
        file.close();
        QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonString.toUtf8());
        QJsonObject json = jsonDoc.object();
        QJsonArray tasksArray = json["tasks"].toArray();
        m_defaultSavePath = json["defaultSavePath"].toString();
        int max = json["maxConcurrentDownloads"].toInt(m_maxConcurrentDownloads);
        if (max < 3) {
            max = 3;
        } else if (max > 8) {
            max = 8;
        }
        m_maxConcurrentDownloads = max;
        for (const QJsonValue &value : tasksArray) {
            QJsonObject taskObject = value.toObject();
            QString id = taskObject["id"].toString();
            QString url = taskObject["url"].toString();
            QString savePath = taskObject["savePath"].toString();
            QString fileName = taskObject["fileName"].toString();
            DownloadTask::Status status = static_cast<DownloadTask::Status>(taskObject["status"].toInt());
            qint64 downloadedSize = taskObject["downloadedSize"].toVariant().toLongLong();
            qint64 totalSize = taskObject["totalSize"].toVariant().toLongLong();
            bool supportsResume = taskObject["supportsResume"].toBool();
            QString errorMessage = taskObject["errorMessage"].toString();
            // 创建任务对象
            DownloadTask *task = new DownloadTask(this);
            task->setId(id);
            task->setUrl(url);
            task->setSavePath(savePath);
            task->setFileName(fileName);
            // 状态修正：如果是 Downloading 或 Queued，重启后恢复为 Pending
            if (status == DownloadTask::Downloading || status == DownloadTask::Queued) {
                status = DownloadTask::Pending;
            }
            task->setStatus(status);
            task->setDownloadedSize(downloadedSize);
            task->setTotalSize(totalSize);
            task->setSupportsResume(supportsResume);
            if (!errorMessage.isEmpty())
                task->setErrorMessage(errorMessage);
            m_tasks[id] = task;
            LOG_INFO(QString("加载任务 - ID: %1, URL: %2").arg(id).arg(url));
        }
        LOG_INFO(QString("已加载 %1 个任务").arg(m_tasks.size()));
    }
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
    task->setErrorMessage(tr("用户取消"));
    emit taskCancelled(task->id());
    processNextTask();
    saveTasks();
}

void DownloadManager::onDownloadCompleted(DownloadTask *task)
{
    LOG_INFO(QString("下载完成 - ID: %1").arg(task->id()));
    m_activeDownloadCount--;
    task->setStatus(DownloadTask::Completed);
    emit taskCompleted(task->id());
    processNextTask();
    saveTasks();
}

void DownloadManager::onDownloadFailed(DownloadTask *task, const QString &error)
{
    LOG_ERROR(QString("下载失败 - ID: %1, 错误: %2").arg(task->id()).arg(error));
    m_activeDownloadCount--;
    task->setStatus(DownloadTask::Failed);
    task->setErrorMessage(error);
    emit taskFailed(task->id(), error);
    processNextTask();
    saveTasks();
}

void DownloadManager::onDownloadProgress(DownloadTask *task, qint64 bytesReceived, qint64 bytesTotal)
{
    Q_UNUSED(bytesReceived);
    Q_UNUSED(bytesTotal);
    emit taskProgress(task->id(), task->downloadedSize(), task->totalSize());
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

void DownloadManager::loadFromJson() {
    loadTasks();
}

void DownloadManager::saveToJson() {
    saveTasks();
}

