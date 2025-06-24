#ifndef DOWNLOADMANAGER_H
#define DOWNLOADMANAGER_H

#include <QObject>
#include <QList>
#include <QMap>
#include <QFile>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "downloadtask.h"
#include "smbdownloader.h"

class DownloadManager : public QObject
{
    Q_OBJECT

public:
    explicit DownloadManager(QObject *parent = nullptr);
    ~DownloadManager();

    // 任务管理
    QString addTask(const QString &url,
                    const QString &savePath = "");
    void removeTask(const QString &taskId);
    void removeCompletedTasks();
    
    // 下载控制
    void startTask(const QString &taskId);
    void pauseTask(const QString &taskId);
    void resumeTask(const QString &taskId);
    void cancelTask(const QString &taskId);
    
    // 批量操作
    void startAllTasks();
    void pauseAllTasks();
    void cancelAllTasks();
    
    // 查询
    DownloadTask* getTask(const QString &taskId) const;
    QList<DownloadTask*> getAllTasks() const;
    QList<DownloadTask*> getActiveTasks() const;
    QList<DownloadTask*> getCompletedTasks() const;
    QList<DownloadTask*> getFailedTasks() const;
    
    // 设置
    QString getDefaultSavePath() const;
    void setDefaultSavePath(const QString &path);
    int getMaxConcurrentDownloads() const;
    void setMaxConcurrentDownloads(int max);
    
    // 持久化
    void saveTasks();
    void loadTasks();

    // 新增
    QString m_configPath;
    void loadFromJson();
    void saveToJson();

signals:
    void taskAdded(const QString &taskId);
    void taskRemoved(const QString &taskId);
    void taskStarted(const QString &taskId);
    void taskPaused(const QString &taskId);
    void taskResumed(const QString &taskId);
    void taskCancelled(const QString &taskId);
    void taskCompleted(const QString &taskId);
    void taskFailed(const QString &taskId, const QString &error);
    void taskProgress(const QString &taskId, qint64 bytesReceived, qint64 bytesTotal);
    void allTasksCompleted();

private slots:
    void onTaskStatusChanged(DownloadTask::Status status);
    void onDownloadStarted(DownloadTask *task);
    void onDownloadPaused(DownloadTask *task);
    void onDownloadResumed(DownloadTask *task);
    void onDownloadCancelled(DownloadTask *task);
    void onDownloadCompleted(DownloadTask *task);
    void onDownloadFailed(DownloadTask *task, const QString &error);
    void onDownloadProgress(DownloadTask *task, qint64 bytesReceived, qint64 bytesTotal);

private:
    QMap<QString, DownloadTask*> m_tasks;
    SmbDownloader *m_smbDownloader;
    
    QString m_defaultSavePath;
    int m_maxConcurrentDownloads;
    int m_activeDownloadCount;
    
    // 辅助方法
    void processNextTask();
    void updateActiveDownloadCount();
};

#endif // DOWNLOADMANAGER_H 