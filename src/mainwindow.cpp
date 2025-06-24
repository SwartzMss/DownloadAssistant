#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "downloadmanager.h"
#include "downloadtask.h"
#include <QMessageBox>
#include <QFileDialog>
#include <QStandardPaths>
#include <QApplication>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QSettings>
#include <QCoreApplication>
#include <QCloseEvent>
#include <algorithm>
#include <QSet>
#include "logger.h"
#include "tasktablewidget.h"
#include <QPushButton>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QUrl>

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

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_downloadManager(new DownloadManager(this))
    , m_updateTimer(new QTimer(this))
    , m_taskModel(new QStandardItemModel(this))
    , m_trayIcon(nullptr)
    , m_trayMenu(nullptr)
    , m_settings(new QSettings(QCoreApplication::applicationDirPath() + "/config.ini", QSettings::IniFormat, this))
{
    LOG_INFO("MainWindow 初始化开始");
    
    ui->setupUi(this);

    
    setupUI();
    setupConnections();
    ui->taskTable->setupTable();
    
    // 设置默认保存路径
    ui->savePathEdit->setText(m_downloadManager->getDefaultSavePath());
    
    // 启动更新定时器
    m_updateTimer->setInterval(1000); // 每秒更新一次
    connect(m_updateTimer, &QTimer::timeout, this, &MainWindow::onUpdateTimer);
    m_updateTimer->start();
    
    // 加载已保存的任务
    updateStatusBar();

    // 加载书签和任务
    loadBookmarks();
    loadTasks();

    // 设置应用程序和窗口图标
    setWindowIcon(QIcon(":/images/icon.png"));
    
    // 托盘可用性判断
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        showError(tr("系统托盘不可用！"));
        return;
    }
    createTrayMenu();
    createTrayIcon();
    
    LOG_INFO("MainWindow 初始化完成");
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setupUI()
{
    // 设置窗口属性
    setWindowTitle(tr("DownloadAssistant - 下载助手"));
    resize(800, 600);

    // 设置状态栏
    statusBar()->showMessage(tr("就绪"));

    ui->remoteFileTable->setColumnCount(4);
    ui->remoteFileTable->setHorizontalHeaderLabels({tr("名称"), tr("大小"), tr("类型"), tr("操作")});
    // 允许用户拖动列宽，默认根据内容调整初始宽度
    for (int i = 0; i < 4; ++i)
        ui->remoteFileTable->horizontalHeader()->setSectionResizeMode(i, QHeaderView::Interactive);
    ui->remoteFileTable->horizontalHeader()->setStretchLastSection(true);
}

void MainWindow::setupConnections()
{
    // 连接 UI 信号
    connect(ui->connectButton, &QPushButton::clicked, this, &MainWindow::onConnectClicked);
    connect(ui->browseButton, &QPushButton::clicked, this, &MainWindow::onBrowseClicked);
    connect(ui->clearButton, &QPushButton::clicked, this, &MainWindow::onClearClicked);
    connect(ui->addBookmarkButton, &QPushButton::clicked, this, &MainWindow::onAddBookmarkClicked);
    connect(ui->bookmarkCombo, &QComboBox::currentTextChanged, this, &MainWindow::onBookmarkSelected);
    connect(ui->startAllButton, &QPushButton::clicked, this, &MainWindow::onStartAllClicked);
    connect(ui->pauseAllButton, &QPushButton::clicked, this, &MainWindow::onPauseAllClicked);
    connect(ui->removeButton, &QPushButton::clicked, this, &MainWindow::onRemoveClicked);
    connect(ui->clearCompletedButton, &QPushButton::clicked, this, &MainWindow::onClearCompletedClicked);
    connect(ui->taskTable, &TaskTableWidget::startTaskRequested, this, &MainWindow::onStartTaskClicked);
    connect(ui->taskTable, &TaskTableWidget::pauseTaskRequested, this, &MainWindow::onPauseTaskClicked);
    connect(ui->taskTable, &TaskTableWidget::resumeTaskRequested, this, &MainWindow::onResumeTaskClicked);
    connect(ui->taskTable, &TaskTableWidget::cancelTaskRequested, this, &MainWindow::onCancelTaskClicked);
    
    // 连接下载管理器信号
    connect(m_downloadManager, &DownloadManager::taskAdded, this, &MainWindow::onTaskAdded);
    connect(m_downloadManager, &DownloadManager::taskRemoved, this, &MainWindow::onTaskRemoved);
    connect(m_downloadManager, &DownloadManager::taskStarted, this, &MainWindow::onTaskStarted);
    connect(m_downloadManager, &DownloadManager::taskPaused, this, &MainWindow::onTaskPaused);
    connect(m_downloadManager, &DownloadManager::taskResumed, this, &MainWindow::onTaskResumed);
    connect(m_downloadManager, &DownloadManager::taskCancelled, this, &MainWindow::onTaskCancelled);
    connect(m_downloadManager, &DownloadManager::taskCompleted, this, &MainWindow::onTaskCompleted);
    connect(m_downloadManager, &DownloadManager::taskFailed, this, &MainWindow::onTaskFailed);
    connect(m_downloadManager, &DownloadManager::allTasksCompleted, this, &MainWindow::onAllTasksCompleted);
    connect(m_downloadManager, &DownloadManager::taskProgress, this, &MainWindow::onTaskProgress);
}


void MainWindow::onConnectClicked()
{
    LOG_INFO("点击连接按钮");
    QString url = ui->urlEdit->text().trimmed();
    if (url.isEmpty()) {
        showWarning(tr("请输入 SMB 地址"));
        return;
    }
    LOG_INFO(QString("连接地址: %1").arg(url));
    fetchSmbFileList(url);
}

void MainWindow::onBrowseClicked()
{
    LOG_INFO("点击浏览按钮");
    QString dir = QFileDialog::getExistingDirectory(this, tr("选择保存目录"),
                                                   ui->savePathEdit->text());
    if (!dir.isEmpty()) {
        ui->savePathEdit->setText(dir);
        m_downloadManager->setDefaultSavePath(dir);
        LOG_INFO(QString("选择保存路径: %1").arg(dir));
    }
}

void MainWindow::onClearClicked()
{
    LOG_INFO("清空输入");
    ui->urlEdit->clear();
    ui->savePathEdit->setText(m_downloadManager->getDefaultSavePath());
    ui->remoteFileTable->setRowCount(0);
}

void MainWindow::onAddBookmarkClicked()
{
    QString alias = ui->aliasEdit->text().trimmed();
    QString url = ui->urlEdit->text().trimmed();
    if (alias.isEmpty() || url.isEmpty()) {
        showWarning(tr("书签名称和地址不能为空"));
        return;
    }

    m_bookmarks[alias] = url;
    if (ui->bookmarkCombo->findText(alias) == -1)
        ui->bookmarkCombo->addItem(alias);

    m_settings->beginGroup("Bookmarks");
    m_settings->setValue(alias, url);
    m_settings->endGroup();

    ui->aliasEdit->clear();
    showInfo(tr("书签已保存"));
}

void MainWindow::onBookmarkSelected(const QString &name)
{
    if (m_bookmarks.contains(name)) {
        ui->urlEdit->setText(m_bookmarks.value(name));
    }
}

void MainWindow::onStartAllClicked()
{
    LOG_INFO("开始全部任务");
    m_downloadManager->startAllTasks();
    showInfo(tr("已开始所有任务"));
}

void MainWindow::onPauseAllClicked()
{
    LOG_INFO("暂停全部任务");
    m_downloadManager->pauseAllTasks();
    showInfo(tr("已暂停所有任务"));
}

void MainWindow::onRemoveClicked()
{
    LOG_INFO("点击删除任务");
    QList<QTableWidgetItem*> selectedItems = ui->taskTable->selectedItems();
    if (selectedItems.isEmpty()) {
        showError(tr("请先选择要删除的任务"));
        return;
    }
    
    QSet<int> selectedRows;
    for (QTableWidgetItem *item : selectedItems) {
        selectedRows.insert(item->row());
    }
    
    // 从后往前删除，避免索引变化
    QList<int> rows = selectedRows.values();
    std::sort(rows.begin(), rows.end(), std::greater<int>());
    
    for (int row : rows) {
        QTableWidgetItem *item = ui->taskTable->item(row, 0);
        if (item) {
            DownloadTask *task = static_cast<DownloadTask*>(item->data(Qt::UserRole).value<void*>());
            if (task) {
                LOG_INFO(QString("删除任务 - ID: %1").arg(task->id()));
                m_downloadManager->removeTask(task->id());
            }
        }
    }
}


void MainWindow::onClearCompletedClicked()
{
    LOG_INFO("清空已完成的任务");
    m_downloadManager->removeCompletedTasks();
    showInfo(tr("已清空已完成的任务"));
}

void MainWindow::onStartTaskClicked(DownloadTask *task)
{
    if (!task)
        return;
    LOG_INFO(QString("开始任务 - ID: %1").arg(task->id()));
    m_downloadManager->startTask(task->id());
}

void MainWindow::onPauseTaskClicked(DownloadTask *task)
{
    if (!task)
        return;
    LOG_INFO(QString("暂停任务 - ID: %1").arg(task->id()));
    m_downloadManager->pauseTask(task->id());
}

void MainWindow::onResumeTaskClicked(DownloadTask *task)
{
    if (!task)
        return;
    LOG_INFO(QString("恢复任务 - ID: %1").arg(task->id()));
    m_downloadManager->resumeTask(task->id());
}

void MainWindow::onCancelTaskClicked(DownloadTask *task)
{
    if (!task)
        return;
    LOG_INFO(QString("取消任务 - ID: %1").arg(task->id()));
    m_downloadManager->cancelTask(task->id());
}

void MainWindow::onTaskAdded(const QString &taskId)
{
    DownloadTask *task = m_downloadManager->getTask(taskId);
    if (task) {
        ui->taskTable->addTask(task);
        updateStatusBar();
        LOG_INFO(QString("任务已添加到界面 - ID: %1").arg(taskId));
    }
}

void MainWindow::onTaskRemoved(const QString &taskId)
{
    // 通过 taskId 查找并删除对应的行
    for (int row = 0; row < ui->taskTable->rowCount(); ++row) {
        QTableWidgetItem *item = ui->taskTable->item(row, 1); // 文件名列
        if (item) {
            DownloadTask *task = static_cast<DownloadTask*>(item->data(Qt::UserRole).value<void*>());
            if (task && task->id() == taskId) {
                ui->taskTable->removeTask(task);
                LOG_INFO(QString("任务已从界面移除 - ID: %1").arg(taskId));
                break;
            }
        }
    }
    updateStatusBar();
}

void MainWindow::onTaskStarted(const QString &taskId)
{
    DownloadTask *task = m_downloadManager->getTask(taskId);
    if (task) {
        ui->taskTable->updateTask(task);
        LOG_INFO(QString("任务开始 - ID: %1").arg(taskId));
    }
}

void MainWindow::onTaskPaused(const QString &taskId)
{
    DownloadTask *task = m_downloadManager->getTask(taskId);
    if (task) {
        ui->taskTable->updateTask(task);
        LOG_INFO(QString("任务暂停 - ID: %1").arg(taskId));
    }
}

void MainWindow::onTaskResumed(const QString &taskId)
{
    DownloadTask *task = m_downloadManager->getTask(taskId);
    if (task) {
        ui->taskTable->updateTask(task);
        LOG_INFO(QString("任务恢复 - ID: %1").arg(taskId));
    }
}

void MainWindow::onTaskCancelled(const QString &taskId)
{
    DownloadTask *task = m_downloadManager->getTask(taskId);
    if (task) {
        ui->taskTable->updateTask(task);
        LOG_INFO(QString("任务取消 - ID: %1").arg(taskId));
    }
}

void MainWindow::onTaskCompleted(const QString &taskId)
{
    DownloadTask *task = m_downloadManager->getTask(taskId);
    if (task) {
        ui->taskTable->updateTask(task);
        LOG_INFO(QString("任务完成 - ID: %1").arg(taskId));
        showInfo(tr("下载完成：%1").arg(task->fileName()));
    }
}

void MainWindow::onTaskFailed(const QString &taskId, const QString &error)
{
    DownloadTask *task = m_downloadManager->getTask(taskId);
    if (task) {
        ui->taskTable->updateTask(task);
        LOG_WARNING(QString("任务失败 - ID: %1, 错误: %2").arg(taskId).arg(error));
        showError(tr("下载失败：%1 - %2").arg(task->fileName()).arg(error));
    }
}

void MainWindow::onAllTasksCompleted()
{
    LOG_INFO("所有下载任务已完成");
    showInfo(tr("所有下载任务已完成"));
}

void MainWindow::onUpdateTimer()
{
    // 更新所有任务的行
    for (int row = 0; row < ui->taskTable->rowCount(); ++row) {
        QTableWidgetItem *item = ui->taskTable->item(row, 0);
        if (item) {
            DownloadTask *task = static_cast<DownloadTask*>(item->data(Qt::UserRole).value<void*>());
            if (task) {
                ui->taskTable->updateTask(task);
            }
        }
    }
}


void MainWindow::updateStatusBar()
{
    QList<DownloadTask*> allTasks = m_downloadManager->getAllTasks();
    QList<DownloadTask*> activeTasks = m_downloadManager->getActiveTasks();
    QList<DownloadTask*> completedTasks = m_downloadManager->getCompletedTasks();
    
    QString status = tr("总任务：%1 | 活动：%2 | 已完成：%3")
                    .arg(allTasks.size())
                    .arg(activeTasks.size())
                    .arg(completedTasks.size());
    
    statusBar()->showMessage(status);
}

void MainWindow::showError(const QString &message)
{
    LOG_ERROR(message);
    QMessageBox::critical(this, tr("错误"), message);
}

void MainWindow::showWarning(const QString &message)
{
    LOG_WARNING(message);
    QMessageBox::warning(this, tr("警告"), message);
}

void MainWindow::showInfo(const QString &message)
{
    LOG_INFO(message);
    QMessageBox::information(this, tr("信息"), message);
}

QString MainWindow::formatBytes(qint64 bytes) const
{
    const qint64 KB = 1024;
    const qint64 MB = 1024 * KB;
    const qint64 GB = 1024 * MB;
    
    if (bytes >= GB) {
        return QString("%1 GB").arg(static_cast<double>(bytes) / GB, 0, 'f', 2);
    } else if (bytes >= MB) {
        return QString("%1 MB").arg(static_cast<double>(bytes) / MB, 0, 'f', 2);
    } else if (bytes >= KB) {
        return QString("%1 KB").arg(static_cast<double>(bytes) / KB, 0, 'f', 2);
    } else {
        return QString("%1 B").arg(bytes);
    }
}

void MainWindow::fetchSmbFileList(const QString &url)
{
    LOG_INFO(QString("获取 SMB 文件列表: %1").arg(url));
    QString uncPath = toUncPath(url);
    ui->remoteFileTable->setRowCount(0);

    QDir dir(uncPath);
    if (!dir.exists()) {
        showError(tr("无法打开目录"));
        return;
    }

    QFileInfoList list = dir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot);
    int row = 0;
    for (const QFileInfo &info : list) {
        QString name = info.fileName();
        bool isDir = info.isDir();
        qint64 size = info.size();

        ui->remoteFileTable->insertRow(row);
        ui->remoteFileTable->setItem(row, 0, new QTableWidgetItem(name));
        ui->remoteFileTable->setItem(row, 1, new QTableWidgetItem(isDir ? QString() : formatBytes(size)));
        ui->remoteFileTable->setItem(row, 2, new QTableWidgetItem(isDir ? tr("目录") : tr("文件")));

        QPushButton *btn = new QPushButton(isDir ? tr("下载目录") : tr("下载"));
        ui->remoteFileTable->setCellWidget(row, 3, btn);
        QString fullUrl = url;
        if (!fullUrl.endsWith('/'))
            fullUrl += '/';
        fullUrl += name;
        if (isDir) {
            connect(btn, &QPushButton::clicked, this, [this, fullUrl]() {
                onDownloadDirectoryClicked(fullUrl);
            });
        } else {
            connect(btn, &QPushButton::clicked, this, [this, fullUrl]() {
                onDownloadFileClicked(fullUrl);
            });
        }
        ++row;
    }

    ui->remoteFileTable->resizeColumnsToContents();

    LOG_INFO("SMB 文件列表获取完成");
}

void MainWindow::onDownloadFileClicked(const QString &fileUrl)
{
    LOG_INFO(QString("开始下载文件: %1").arg(fileUrl));

    QString savePath = ui->savePathEdit->text().trimmed();
    if (savePath.isEmpty())
        savePath = m_downloadManager->getDefaultSavePath();

    // 添加任务后立即启动下载，避免用户还需手动点击开始
    QString taskId = m_downloadManager->addTask(fileUrl, savePath, DownloadTask::SMB);
    m_downloadManager->startTask(taskId);
}

void MainWindow::onDownloadDirectoryClicked(const QString &dirUrl)
{
    LOG_INFO(QString("开始下载目录: %1").arg(dirUrl));

    QString savePath = ui->savePathEdit->text().trimmed();
    if (savePath.isEmpty())
        savePath = m_downloadManager->getDefaultSavePath();

    QString dirName = QUrl(dirUrl).fileName();
    if (dirName.isEmpty()) {
        QUrl temp(dirUrl);
        QString path = temp.path();
        if (path.endsWith('/'))
            path.chop(1);
        dirName = QFileInfo(path).fileName();
    }

    QString localPath = QDir(savePath).filePath(dirName);
    downloadDirectoryRecursive(dirUrl, localPath);
}

void MainWindow::downloadDirectoryRecursive(const QString &dirUrl, const QString &localPath)
{
    QString uncPath = toUncPath(dirUrl);
    QDir dir(uncPath);
    if (!dir.exists())
        return;

    QDir().mkpath(localPath);

    QFileInfoList list = dir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot);
    for (const QFileInfo &info : list) {
        QString name = info.fileName();
        QString childUrl = dirUrl;
        if (!childUrl.endsWith('/'))
            childUrl += '/';
        childUrl += name;

        if (info.isDir()) {
            QString subLocal = QDir(localPath).filePath(name);
            downloadDirectoryRecursive(childUrl, subLocal);
        } else {
            QString taskId = m_downloadManager->addTask(childUrl, localPath, DownloadTask::SMB);
            m_downloadManager->startTask(taskId);
        }
    }
}


void MainWindow::loadTasks()
{
    // 从下载管理器获取所有已加载的任务并插入到表格中
    LOG_INFO("加载任务到界面");
    QList<DownloadTask*> tasks = m_downloadManager->getAllTasks();

    ui->taskTable->setRowCount(0);
    for (DownloadTask *task : tasks) {
        ui->taskTable->addTask(task);
    }

    updateStatusBar();
}

void MainWindow::loadBookmarks()
{
    LOG_INFO("加载书签");
    ui->bookmarkCombo->clear();
    m_bookmarks.clear();

    m_settings->beginGroup("Bookmarks");
    const QStringList keys = m_settings->allKeys();
    for (const QString &key : keys) {
        QString url = m_settings->value(key).toString();
        m_bookmarks[key] = url;
        ui->bookmarkCombo->addItem(key);
    }
    m_settings->endGroup();
}

void MainWindow::onTaskProgress(const QString &taskId, qint64 bytesReceived, qint64 bytesTotal)
{
    DownloadTask *task = m_downloadManager->getTask(taskId);
    if (task) {
        ui->taskTable->updateTask(task);
    }
}

void MainWindow::createTrayMenu()
{
    LOG_INFO("创建托盘菜单");
    m_trayMenu = new QMenu(this);
    QAction *showAction = m_trayMenu->addAction(tr("显示窗口"));
    connect(showAction, &QAction::triggered, this, &QWidget::showNormal);
    QAction *quitAction = m_trayMenu->addAction(tr("退出"));
    connect(quitAction, &QAction::triggered, qApp, &QApplication::quit);
}

void MainWindow::createTrayIcon()
{
    LOG_INFO("创建托盘图标");
    m_trayIcon = new QSystemTrayIcon(this);
    m_trayIcon->setIcon(windowIcon());
    m_trayIcon->setContextMenu(m_trayMenu);
    connect(m_trayIcon, &QSystemTrayIcon::activated, this, &MainWindow::onTrayIconActivated);
    m_trayIcon->show();
}

void MainWindow::onTrayIconActivated(QSystemTrayIcon::ActivationReason reason)
{
    LOG_INFO("托盘图标被激活");
    if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
        if (isHidden()) {
            showNormal();
        } else {
            hide();
        }
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_trayIcon && m_trayIcon->isVisible()) {
        LOG_INFO("窗口隐藏到托盘");
        hide();
        event->ignore();
    } else {
        LOG_INFO("窗口关闭");
        QMainWindow::closeEvent(event);
    }
}
