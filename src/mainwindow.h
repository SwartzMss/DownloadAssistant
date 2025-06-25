#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStandardItemModel>
#include <QTimer>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QSettings>
#include "downloadmanager.h"
#include "tasktablewidget.h"

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
    void onBrowseClicked();
    void onBrowseSmbButtonClicked();
    void onAddTaskButtonClicked();
    
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
    void onTrayIconActivated(QSystemTrayIcon::ActivationReason reason);
    void onDownloadFileClicked(const QString &fileName);
    void onDownloadDirectoryClicked(const QString &dirUrl);

private:
    // UI 组件
    Ui::MainWindow *ui;
    
    // 核心组件
    DownloadManager *m_downloadManager;
    QStandardItemModel *m_taskModel;
    QTimer *m_updateTimer;
    QSystemTrayIcon *m_trayIcon;
    QMenu *m_trayMenu;
    QSettings *m_settings;
    
    // 辅助方法
    void setupUI();
    void setupConnections();
    void loadTasks();
    void updateStatusBar();
    void onStartAllClicked();
    void onPauseAllClicked();
    void onRemoveClicked();
    void onAllTasksCompleted();
    void showInfo(const QString &message);
    void showWarning(const QString &message);
    void showError(const QString &message);
    QString formatBytes(qint64 bytes) const;
    void fetchSmbFileList(const QString &url);
    void downloadDirectoryRecursive(const QString &dirUrl, const QString &localPath);

    void createTrayIcon();
    void createTrayMenu();

protected:
    void closeEvent(QCloseEvent *event) override;
};

#endif // MAINWINDOW_H 