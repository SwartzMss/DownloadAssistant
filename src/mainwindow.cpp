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
#include <QDir>
#include <QFileInfo>
#include <QDate>
#include <QInputDialog>

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
    
    // 设置默认保存路径
    ui->savePathEdit->setText(m_downloadManager->getDefaultSavePath());
    
    // 启动更新定时器
    m_updateTimer->setInterval(1000); // 每秒更新一次
    connect(m_updateTimer, &QTimer::timeout, this, &MainWindow::onUpdateTimer);
    m_updateTimer->start();
    
    // 加载已保存的任务并刷新表格
    loadTasks();
    updateStatusBar();

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

    // 初始化已完成任务表格
    ui->completedTable->setColumnCount(3);
    ui->completedTable->setHorizontalHeaderLabels({tr("文件名"), tr("文件路径"), tr("大小")});
    ui->completedTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->completedTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->completedTable->setAlternatingRowColors(true);
    ui->completedTable->horizontalHeader()->setStretchLastSection(false);
    ui->completedTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents); // 文件名
    ui->completedTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);         // 文件路径
    ui->completedTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents); // 大小

    ui->completedTable->setTextElideMode(Qt::ElideMiddle);
    ui->completedTable->setWordWrap(false);
    ui->completedTable->horizontalHeader()->setMaximumSectionSize(400);
    // 右键菜单
    ui->completedTable->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->completedTable, &QTableWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
        QMenu menu;
        QAction *removeAction = menu.addAction(tr("删除选中"));
        QAction *clearAction = menu.addAction(tr("全部清空"));
        QAction *act = menu.exec(ui->completedTable->viewport()->mapToGlobal(pos));
        if (act == removeAction) {
            QList<QTableWidgetItem*> selected = ui->completedTable->selectedItems();
            QSet<int> rows;
            for (QTableWidgetItem *item : selected) rows.insert(item->row());
            QList<int> rowList = rows.values();
            std::sort(rowList.begin(), rowList.end(), std::greater<int>());
            // 批量删除选中任务
            for (int row : rowList) {
                QTableWidgetItem *item = ui->completedTable->item(row, 0); // 文件名列
                QTableWidgetItem *pathItem = ui->completedTable->item(row, 1); // 路径列
                if (item && pathItem) {
                    QString fileName = item->text();
                    QString savePath = pathItem->text();
                    // 遍历所有已完成任务，找到对应的 taskId
                    QList<DownloadTask*> completedTasks = m_downloadManager->getCompletedTasks();
                    for (DownloadTask *task : completedTasks) {
                        if (task->fileName() == fileName && task->savePath() == savePath) {
                            m_downloadManager->removeTask(task->id());
                            break;
                        }
                    }
                }
            }
            loadTasks();
        } else if (act == clearAction) {
            m_downloadManager->removeCompletedTasks();
            loadTasks();
        }
    });

    // 初始化失败任务表格
    ui->failedTable->setColumnCount(3);
    ui->failedTable->setHorizontalHeaderLabels({tr("文件名"), tr("文件路径"), tr("原因")});
    ui->failedTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->failedTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->failedTable->setAlternatingRowColors(true);
    ui->failedTable->horizontalHeader()->setStretchLastSection(false);
    ui->failedTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    ui->failedTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    ui->failedTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);

    ui->failedTable->setTextElideMode(Qt::ElideMiddle);
    ui->failedTable->setWordWrap(false);
    ui->failedTable->horizontalHeader()->setMaximumSectionSize(400);
    // 右键菜单
    ui->failedTable->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->failedTable, &QTableWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
        QMenu menu;
        QAction *removeAction = menu.addAction(tr("删除选中"));
        QAction *clearAction = menu.addAction(tr("全部清空"));
        QAction *act = menu.exec(ui->failedTable->viewport()->mapToGlobal(pos));
        if (act == removeAction) {
            QList<QTableWidgetItem*> selected = ui->failedTable->selectedItems();
            QSet<int> rows;
            for (QTableWidgetItem *item : selected) rows.insert(item->row());
            QList<int> rowList = rows.values();
            std::sort(rowList.begin(), rowList.end(), std::greater<int>());
            for (int row : rowList) {
                QTableWidgetItem *nameItem = ui->failedTable->item(row, 0);
                QTableWidgetItem *pathItem = ui->failedTable->item(row, 1);
                if (nameItem && pathItem) {
                    QString fileName = nameItem->text();
                    QString savePath = pathItem->text();
                    QList<DownloadTask*> failedTasks = m_downloadManager->getFailedTasks();
                    for (DownloadTask *task : failedTasks) {
                        if (task->fileName() == fileName && task->savePath() == savePath) {
                            m_downloadManager->removeTask(task->id());
                            break;
                        }
                    }
                }
            }
            loadTasks();
        } else if (act == clearAction) {
            QList<DownloadTask*> failedTasks = m_downloadManager->getFailedTasks();
            for (DownloadTask *task : failedTasks) {
                m_downloadManager->removeTask(task->id());
            }
            loadTasks();
        }
    });
}

void MainWindow::setupConnections()
{
    // 连接 UI 信号
    connect(ui->browseButton, &QPushButton::clicked, this, &MainWindow::onBrowseClicked);
    connect(ui->clearButton, &QPushButton::clicked, this, &MainWindow::onClearClicked);
    connect(ui->addTaskButton, &QPushButton::clicked, this, &MainWindow::onAddTaskButtonClicked);
    connect(ui->startAllButton, &QPushButton::clicked, this, &MainWindow::onStartAllClicked);
    connect(ui->pauseAllButton, &QPushButton::clicked, this, &MainWindow::onPauseAllClicked);
    connect(ui->browseSmbButton, &QPushButton::clicked, this, &MainWindow::onBrowseSmbButtonClicked);
    
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
    connect(m_downloadManager, &DownloadManager::taskProgress,
            this, &MainWindow::onTaskProgress);

    // 连接 TaskTableWidget 的操作信号到 DownloadManager
    connect(ui->taskTable, &TaskTableWidget::startTaskRequested, this, [this](DownloadTask *task) {
        if (task) m_downloadManager->startTask(task->id());
    });
    connect(ui->taskTable, &TaskTableWidget::pauseTaskRequested, this, [this](DownloadTask *task) {
        if (task) m_downloadManager->pauseTask(task->id());
    });
    connect(ui->taskTable, &TaskTableWidget::resumeTaskRequested, this, [this](DownloadTask *task) {
        if (task) m_downloadManager->resumeTask(task->id());
    });
    connect(ui->taskTable, &TaskTableWidget::cancelTaskRequested, this, [this](DownloadTask *task) {
        if (task) m_downloadManager->cancelTask(task->id());
    });
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
    ui->savePathEdit->setText(m_downloadManager->getDefaultSavePath());
    // 已移除 remoteFileTable 相关代码
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

void MainWindow::onTaskAdded(const QString &taskId)
{
    DownloadTask *task = m_downloadManager->getTask(taskId);
    if (task) {
        // ui->taskTable->addTask(task); // 已注释
        updateStatusBar();
        LOG_INFO(QString("任务已添加到界面 - ID: %1").arg(taskId));
    }
}

void MainWindow::onTaskRemoved(const QString &taskId)
{
    // 通过 taskId 查找并删除对应的行
    for (int row = 0; row < ui->taskTable->rowCount(); ++row) {
        // 文件名列位于第 0 列
        QTableWidgetItem *item = ui->taskTable->item(row, 0);
        if (item) {
            DownloadTask *task = static_cast<DownloadTask*>(item->data(Qt::UserRole).value<void*>());
            if (task && task->id() == taskId) {
                // ui->taskTable->removeTask(task); // 已注释
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

void MainWindow::onTaskCompleted(const QString &taskId)
{
    DownloadTask *task = m_downloadManager->getTask(taskId);
    if (task) {
        ui->taskTable->updateTask(task);
        LOG_INFO(QString("任务完成 - ID: %1").arg(taskId));
        showInfo(tr("下载完成：%1").arg(task->fileName()));
        loadTasks();
    }
}

void MainWindow::onTaskFailed(const QString &taskId, const QString &error)
{
    DownloadTask *task = m_downloadManager->getTask(taskId);
    if (task) {
        ui->taskTable->updateTask(task);
        LOG_WARNING(QString("任务失败 - ID: %1, 错误: %2").arg(taskId).arg(error));
        showError(tr("下载失败：%1 - %2").arg(task->fileName()).arg(error));
        loadTasks();
    }
}

void MainWindow::onTaskCancelled(const QString &taskId)
{
    DownloadTask *task = m_downloadManager->getTask(taskId);
    if (task) {
        ui->taskTable->updateTask(task);
        LOG_INFO(QString("任务取消 - ID: %1").arg(taskId));
        loadTasks();
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
    QList<DownloadTask*> failedTasks = m_downloadManager->getFailedTasks();

    QString status = tr("总任务：%1 | 活动：%2 | 已完成：%3 | 失败：%4")
                    .arg(allTasks.size())
                    .arg(activeTasks.size())
                    .arg(completedTasks.size())
                    .arg(failedTasks.size());
    
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

QString MainWindow::buildFinalSavePath(const QString &basePath) const
{
    QString dateDir = QDate::currentDate().toString("MM_dd");
    QString result = QDir(basePath).filePath(dateDir);

    QString sub = ui->subDirEdit->text().trimmed();
    if (!sub.isEmpty()) {
        result = QDir(result).filePath(sub);
    }

    QDir().mkpath(result);
    return result;
}

void MainWindow::fetchSmbFileList(const QString &url)
{
    // 这里原本有 remoteFileTable 的相关操作，已移除
    // 你可以根据新 UI 逻辑实现文件选择后的处理
}

void MainWindow::onDownloadFileClicked(const QString &fileUrl)
{
    LOG_INFO(QString("开始下载文件: %1").arg(fileUrl));

    QString savePath = ui->savePathEdit->text().trimmed();
    if (savePath.isEmpty())
        savePath = m_downloadManager->getDefaultSavePath();

    savePath = buildFinalSavePath(savePath);

    // 添加任务后立即启动下载，避免用户还需手动点击开始
    QString taskId = m_downloadManager->addTask(fileUrl, savePath);
    m_downloadManager->startTask(taskId);
}

void MainWindow::onDownloadDirectoryClicked(const QString &dirUrl)
{
    LOG_INFO(QString("开始下载目录: %1").arg(dirUrl));

    QString savePath = ui->savePathEdit->text().trimmed();
    if (savePath.isEmpty())
        savePath = m_downloadManager->getDefaultSavePath();

    savePath = buildFinalSavePath(savePath);

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
            QString taskId = m_downloadManager->addTask(childUrl, localPath);
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
    ui->completedTable->setRowCount(0);
    ui->failedTable->setRowCount(0);
    for (DownloadTask *task : tasks) {
        LOG_INFO(QString("遍历任务 - ID: %1, 状态: %2").arg(task->id()).arg(task->status()));
        if (task->status() == DownloadTask::Completed) {
            int row = ui->completedTable->rowCount();
            ui->completedTable->insertRow(row);
            QTableWidgetItem *nameItem = new QTableWidgetItem(task->fileName());
            nameItem->setToolTip(task->fileName());
            ui->completedTable->setItem(row, 0, nameItem);

            ui->completedTable->setItem(row, 1, new QTableWidgetItem(task->savePath()));
            ui->completedTable->setItem(row, 2, new QTableWidgetItem(formatBytes(task->downloadedSize())));
            LOG_INFO(QString("已插入到已完成任务表格 - ID: %1, 行: %2").arg(task->id()).arg(row));
        } else if (task->status() == DownloadTask::Failed || task->status() == DownloadTask::Cancelled) {
            int row = ui->failedTable->rowCount();
            ui->failedTable->insertRow(row);
            QTableWidgetItem *nameItem = new QTableWidgetItem(task->fileName());
            nameItem->setToolTip(task->fileName());
            ui->failedTable->setItem(row, 0, nameItem);

            ui->failedTable->setItem(row, 1, new QTableWidgetItem(task->savePath()));
            QString reason = task->status() == DownloadTask::Cancelled ? tr("用户取消") : task->errorMessage();
            ui->failedTable->setItem(row, 2, new QTableWidgetItem(reason));
            LOG_INFO(QString("已插入到失败任务表格 - ID: %1, 行: %2").arg(task->id()).arg(row));
        } else {
            ui->taskTable->addTask(task);
            LOG_INFO(QString("已插入到当前任务表格 - ID: %1").arg(task->id()));
        }
    }
    updateStatusBar();
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

void MainWindow::onBrowseSmbButtonClicked()
{
    // 这里只做本地目录选择示例，后续可集成远程SMB浏览
    QString file = QFileDialog::getOpenFileName(this, tr("选择远程文件"));
    if (!file.isEmpty()) {
        ui->urlEdit->setText(file);
    }
}

void MainWindow::onAddTaskButtonClicked()
{
    QString url = ui->urlEdit->text().trimmed();
    QString savePath = ui->savePathEdit->text().trimmed();
    if (url.isEmpty()) {
        showWarning(tr("请输入下载地址"));
        return;
    }
    if (savePath.isEmpty()) {
        savePath = m_downloadManager->getDefaultSavePath();
    }
    savePath = buildFinalSavePath(savePath);
    QString taskId = m_downloadManager->addTask(url, savePath);
    m_downloadManager->startTask(taskId);
    // 立即刷新表格
    loadTasks();
    updateStatusBar();
    showInfo(tr("已添加下载任务"));
}

void MainWindow::onTaskProgress(const QString &taskId, qint64 bytesReceived, qint64 bytesTotal)
{
    DownloadTask *task = m_downloadManager->getTask(taskId);
    if (task) {
        ui->taskTable->updateTask(task);
        updateStatusBar();
    }
}
