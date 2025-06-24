#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "downloadmanager.h"
#include "downloadtask.h"
#include <QMessageBox>
#include <QFileDialog>
#include <QStandardPaths>
#include <QApplication>
#include <QCheckBox>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QCloseEvent>
#include <algorithm>
#include <QSet>
#include "logger.h"
#include "tasktablewidget.h"
#include <smb2/libsmb2.h>
#include <smb2/smb2.h>
#include <QPushButton>
#include <QTableWidgetItem>
#include <QHeaderView>
#include "smbutils.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_downloadManager(new DownloadManager(this))
    , m_updateTimer(new QTimer(this))
    , m_taskModel(new QStandardItemModel(this))
    , m_trayIcon(nullptr)
    , m_trayMenu(nullptr)
{
    LOG_INFO("MainWindow 初始化开始");
    
    ui->setupUi(this);

    ui->usernameEdit->setEnabled(false);
    ui->passwordEdit->setEnabled(false);
    ui->domainEdit->setEnabled(false);
    
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
    
    // 初始化协议选择下拉框
    ui->protocolComboBox->clear();
    ui->protocolComboBox->addItem(tr("SMB"), static_cast<int>(DownloadTask::SMB));
    
    // 设置下拉框的显示项数，有助于稳定弹出行为
    ui->protocolComboBox->setMaxVisibleItems(5);
    
    // 加载任务
    loadTasks();

    // 设置应用程序和窗口图标
    setWindowIcon(QIcon(":/images/icon.png"));
    
    // 托盘可用性判断
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        QMessageBox::critical(this, tr("错误"), tr("系统托盘不可用！"));
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
    ui->remoteFileTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    for (int i = 1; i < 4; ++i)
        ui->remoteFileTable->horizontalHeader()->setSectionResizeMode(i, QHeaderView::ResizeToContents);
}

void MainWindow::setupConnections()
{
    // 连接 UI 信号
    connect(ui->connectButton, &QPushButton::clicked, this, &MainWindow::onConnectClicked);
    connect(ui->browseButton, &QPushButton::clicked, this, &MainWindow::onBrowseClicked);
    connect(ui->clearButton, &QPushButton::clicked, this, &MainWindow::onClearClicked);
    connect(ui->authCheckBox, &QCheckBox::toggled, this, [this](bool checked){
        ui->usernameEdit->setEnabled(checked);
        ui->passwordEdit->setEnabled(checked);
        ui->domainEdit->setEnabled(checked);
    });
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
    QString url = normalizeSmbUrl(ui->urlEdit->text());
    if (url.isEmpty()) {
        QMessageBox::warning(this, tr("警告"), tr("请输入 SMB 地址"));
        return;
    }
    if (!validateAuthFields()) {
        return;
    }
    fetchSmbFileList(url);
}

void MainWindow::onBrowseClicked()
{
    QString dir = QFileDialog::getExistingDirectory(this, tr("选择保存目录"), 
                                                   ui->savePathEdit->text());
    if (!dir.isEmpty()) {
        ui->savePathEdit->setText(dir);
        m_downloadManager->setDefaultSavePath(dir);
    }
}

void MainWindow::onClearClicked()
{
    ui->urlEdit->clear();
    ui->savePathEdit->setText(m_downloadManager->getDefaultSavePath());
    ui->remoteFileTable->setRowCount(0);
}

void MainWindow::onStartAllClicked()
{
    m_downloadManager->startAllTasks();
    showInfo(tr("已开始所有任务"));
}

void MainWindow::onPauseAllClicked()
{
    m_downloadManager->pauseAllTasks();
    showInfo(tr("已暂停所有任务"));
}

void MainWindow::onRemoveClicked()
{
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
                m_downloadManager->removeTask(task->id());
            }
        }
    }
}


void MainWindow::onClearCompletedClicked()
{
    m_downloadManager->removeCompletedTasks();
    showInfo(tr("已清空已完成的任务"));
}

void MainWindow::onStartTaskClicked(DownloadTask *task)
{
    if (!task)
        return;
    m_downloadManager->startTask(task->id());
}

void MainWindow::onPauseTaskClicked(DownloadTask *task)
{
    if (!task)
        return;
    m_downloadManager->pauseTask(task->id());
}

void MainWindow::onResumeTaskClicked(DownloadTask *task)
{
    if (!task)
        return;
    m_downloadManager->resumeTask(task->id());
}

void MainWindow::onCancelTaskClicked(DownloadTask *task)
{
    if (!task)
        return;
    m_downloadManager->cancelTask(task->id());
}

void MainWindow::onTaskAdded(const QString &taskId)
{
    DownloadTask *task = m_downloadManager->getTask(taskId);
    if (task) {
        ui->taskTable->addTask(task);
        updateStatusBar();
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
    }
}

void MainWindow::onTaskPaused(const QString &taskId)
{
    DownloadTask *task = m_downloadManager->getTask(taskId);
    if (task) {
        ui->taskTable->updateTask(task);
    }
}

void MainWindow::onTaskResumed(const QString &taskId)
{
    DownloadTask *task = m_downloadManager->getTask(taskId);
    if (task) {
        ui->taskTable->updateTask(task);
    }
}

void MainWindow::onTaskCancelled(const QString &taskId)
{
    DownloadTask *task = m_downloadManager->getTask(taskId);
    if (task) {
        ui->taskTable->updateTask(task);
    }
}

void MainWindow::onTaskCompleted(const QString &taskId)
{
    DownloadTask *task = m_downloadManager->getTask(taskId);
    if (task) {
        ui->taskTable->updateTask(task);
        showInfo(tr("下载完成：%1").arg(task->fileName()));
    }
}

void MainWindow::onTaskFailed(const QString &taskId, const QString &error)
{
    DownloadTask *task = m_downloadManager->getTask(taskId);
    if (task) {
        ui->taskTable->updateTask(task);
        showError(tr("下载失败：%1 - %2").arg(task->fileName()).arg(error));
    }
}

void MainWindow::onAllTasksCompleted()
{
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
    QMessageBox::critical(this, tr("错误"), message);
}

void MainWindow::showInfo(const QString &message)
{
    QMessageBox::information(this, tr("信息"), message);
}

bool MainWindow::validateAuthFields()
{
    if (!ui->authCheckBox->isChecked())
        return true;

    QString username = ui->usernameEdit->text().trimmed();
    QString password = ui->passwordEdit->text().trimmed();

    if (username.isEmpty() || password.isEmpty()) {
        QMessageBox::warning(this, tr("警告"), tr("用户名和密码不能为空"));
        return false;
    }

    return true;
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
    QString normalizedUrl = normalizeSmbUrl(url);
    ui->remoteFileTable->setRowCount(0);

    struct smb2_context *smb2 = smb2_init_context();
    if (!smb2) {
        showError(tr("SMB2 上下文创建失败"));
        return;
    }

    struct smb2_url *smburl = smb2_parse_url(smb2, normalizedUrl.toUtf8().constData());
    if (!smburl) {
        showError(tr("SMB URL 解析失败: %1").arg(QString::fromUtf8(smb2_get_error(smb2))));
        smb2_destroy_context(smb2);
        return;
    }

    QString username = ui->authCheckBox->isChecked() ? ui->usernameEdit->text() : QString();
    QString password = ui->authCheckBox->isChecked() ? ui->passwordEdit->text() : QString();
    QString domain = ui->authCheckBox->isChecked() ? ui->domainEdit->text() : QString();
    if (domain.isEmpty())
        parseDomainUser(username, domain, username);
    if (!domain.isEmpty()) smb2_set_domain(smb2, domain.toUtf8().constData());
    if (!username.isEmpty()) smb2_set_user(smb2, username.toUtf8().constData());
    if (!password.isEmpty()) smb2_set_password(smb2, password.toUtf8().constData());

    if (smb2_connect_share(smb2, smburl->server, smburl->share,
                           username.isEmpty() ? nullptr : username.toUtf8().constData()) < 0) {
        showError(tr("连接失败: %1").arg(QString::fromUtf8(smb2_get_error(smb2))));
        smb2_destroy_url(smburl);
        smb2_destroy_context(smb2);
        return;
    }

    struct smb2dir *dir = smb2_opendir(smb2, smburl->path ? smburl->path : "/");
    if (!dir) {
        showError(tr("打开目录失败: %1").arg(QString::fromUtf8(smb2_get_error(smb2))));
        smb2_disconnect_share(smb2);
        smb2_destroy_url(smburl);
        smb2_destroy_context(smb2);
        return;
    }

    struct smb2dirent *ent;
    int row = 0;
    while ((ent = smb2_readdir(smb2, dir)) != nullptr) {
        QString name = QString::fromUtf8(ent->name);
        if (name == "." || name == "..")
            continue;

        bool isDir = ent->st.smb2_type == SMB2_TYPE_DIRECTORY;
        qint64 size = ent->st.smb2_size;

        ui->remoteFileTable->insertRow(row);
        ui->remoteFileTable->setItem(row, 0, new QTableWidgetItem(name));
        ui->remoteFileTable->setItem(row, 1, new QTableWidgetItem(isDir ? QString() : formatBytes(size)));
        ui->remoteFileTable->setItem(row, 2, new QTableWidgetItem(isDir ? tr("目录") : tr("文件")));

        if (!isDir) {
            QPushButton *btn = new QPushButton(tr("下载"));
            ui->remoteFileTable->setCellWidget(row, 3, btn);
            QString fullUrl = url;
            if (!fullUrl.endsWith('/'))
                fullUrl += '/';
            fullUrl += name;
            connect(btn, &QPushButton::clicked, this, [this, fullUrl]() {
                onDownloadFileClicked(fullUrl);
            });
        }
        ++row;
    }

    smb2_closedir(smb2, dir);
    smb2_disconnect_share(smb2);
    smb2_destroy_url(smburl);
    smb2_destroy_context(smb2);
}

void MainWindow::onDownloadFileClicked(const QString &fileUrl)
{
    if (!validateAuthFields())
        return;

    QString savePath = ui->savePathEdit->text().trimmed();
    if (savePath.isEmpty())
        savePath = m_downloadManager->getDefaultSavePath();

    QString username = ui->authCheckBox->isChecked() ? ui->usernameEdit->text() : QString();
    QString password = ui->authCheckBox->isChecked() ? ui->passwordEdit->text() : QString();
    QString domain = ui->authCheckBox->isChecked() ? ui->domainEdit->text() : QString();
    if (domain.isEmpty())
        parseDomainUser(username, domain, username);

    m_downloadManager->addTask(fileUrl, savePath, DownloadTask::SMB, username, password, domain);
}


void MainWindow::loadTasks()
{
    // 从下载管理器获取所有已加载的任务并插入到表格中
    QList<DownloadTask*> tasks = m_downloadManager->getAllTasks();

    ui->taskTable->setRowCount(0);
    for (DownloadTask *task : tasks) {
        ui->taskTable->addTask(task);
    }

    updateStatusBar();
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
    m_trayMenu = new QMenu(this);
    QAction *showAction = m_trayMenu->addAction(tr("显示窗口"));
    connect(showAction, &QAction::triggered, this, &QWidget::showNormal);
    QAction *quitAction = m_trayMenu->addAction(tr("退出"));
    connect(quitAction, &QAction::triggered, qApp, &QApplication::quit);
}

void MainWindow::createTrayIcon()
{
    m_trayIcon = new QSystemTrayIcon(this);
    m_trayIcon->setIcon(windowIcon());
    m_trayIcon->setContextMenu(m_trayMenu);
    connect(m_trayIcon, &QSystemTrayIcon::activated, this, &MainWindow::onTrayIconActivated);
    m_trayIcon->show();
}

void MainWindow::onTrayIconActivated(QSystemTrayIcon::ActivationReason reason)
{
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
        hide();
        event->ignore();
    } else {
        QMainWindow::closeEvent(event);
    }
}
