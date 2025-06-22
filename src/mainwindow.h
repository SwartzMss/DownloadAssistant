#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStandardItemModel>
#include <QTimer>
#include "downloadmanager.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    // UI 事件处理
    void onAddTaskClicked();
    void onBrowseClicked();
    void onStartTaskClicked();
    void onPauseTaskClicked();
    void onResumeTaskClicked();
    void onCancelTaskClicked();
    void onRemoveTaskClicked();
    void onClearCompletedClicked();
    
    // 下载管理器事件
    void onTaskAdded(const QString &taskId);
    void onTaskRemoved(const QString &taskId);
    void onTaskStarted(const QString &taskId);
    void onTaskPaused(const QString &taskId);
    void onTaskResumed(const QString &taskId);
    void onTaskCancelled(const QString &taskId);
    void onTaskCompleted(const QString &taskId);
    void onTaskFailed(const QString &taskId, const QString &error);
    void onTaskProgress(const QString &taskId, qint64 bytesReceived, qint64 bytesTotal);
    
    // 定时器事件
    void onUpdateTimer();

private:
    // UI 组件
    Ui::MainWindow *ui;
    
    // 核心组件
    DownloadManager *m_downloadManager;
    QStandardItemModel *m_taskModel;
    QTimer *m_updateTimer;
    
    // 辅助方法
    void setupUI();
    void setupConnections();
    void setupTable();
    void setupTaskTable();
    void connectSignalsSlots();
    void loadTasks();
    void updateTaskRow(const QString &taskId);
    void addTaskRow(const QString &taskId);
    void removeTaskRow(const QString &taskId);
    int findTaskRow(const QString &taskId);
    void updateStatusBar();
    void updateTableRow(int row, DownloadTask *task);
    void addTableRow(DownloadTask *task);
    void removeTableRow(DownloadTask *task);
    int findTableRow(DownloadTask *task);
    void createOperationButtons(int row, DownloadTask *task);
    void updateOperationButtons(int row, DownloadTask *task);
    void onClearClicked();
    void onStartAllClicked();
    void onPauseAllClicked();
    void onRemoveClicked();
    void onAllTasksCompleted();
    void showInfo(const QString &message);
    void showError(const QString &message);
    bool validateInput();
    QString formatBytes(qint64 bytes) const;
    QString formatTime(qint64 seconds) const;
    bool isValidUrl(const QString &url) const;
    bool isValidPath(const QString &path) const;
};

#endif // MAINWINDOW_H 