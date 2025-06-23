#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "downloadmanager.h"
#include "downloadtask.h"
#include <QMessageBox>
#include <QFileDialog>
#include <QStandardPaths>
#include <QHeaderView>
#include <QApplication>
#include <QCheckBox>
#include <QProgressBar>
#include "logger.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_downloadManager(new DownloadManager(this))
    , m_updateTimer(new QTimer(this))
    , m_taskModel(new QStandardItemModel(this))
{
    LOG_INFO("MainWindow 初始化开始");
    
    ui->setupUi(this);

    ui->usernameEdit->setEnabled(false);
    ui->passwordEdit->setEnabled(false);
    
    setupUI();
    setupConnections();
    setupTable();
    
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
    ui->protocolComboBox->addItem(tr("HTTP/HTTPS"), static_cast<int>(DownloadTask::HTTP_HTTPS));
    
    // 设置下拉框的显示项数，有助于稳定弹出行为
    ui->protocolComboBox->setMaxVisibleItems(5);
    
    // 加载任务
    loadTasks();
    
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
}

void MainWindow::setupConnections()
{
    // 连接 UI 信号
    connect(ui->addTaskButton, &QPushButton::clicked, this, &MainWindow::onAddTaskClicked);
    connect(ui->browseButton, &QPushButton::clicked, this, &MainWindow::onBrowseClicked);
    connect(ui->clearButton, &QPushButton::clicked, this, &MainWindow::onClearClicked);
    connect(ui->authCheckBox, &QCheckBox::toggled, this, [this](bool checked){
        ui->usernameEdit->setEnabled(checked);
        ui->passwordEdit->setEnabled(checked);
    });
    connect(ui->startAllButton, &QPushButton::clicked, this, &MainWindow::onStartAllClicked);
    connect(ui->pauseAllButton, &QPushButton::clicked, this, &MainWindow::onPauseAllClicked);
    connect(ui->removeButton, &QPushButton::clicked, this, &MainWindow::onRemoveClicked);
    connect(ui->clearCompletedButton, &QPushButton::clicked, this, &MainWindow::onClearCompletedClicked);
    
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

void MainWindow::setupTable()
{
    // 设置表格属性
    ui->taskTable->setColumnCount(8);
    ui->taskTable->setHorizontalHeaderLabels({
        tr("协议"),
        tr("文件名"),
        tr("状态"),
        tr("进度"),
        tr("速度"),
        tr("大小"),
        tr("剩余时间"),
        tr("操作")
    });
    
    // 设置表格样式
    ui->taskTable->setAlternatingRowColors(true);
    ui->taskTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->taskTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    
    // 设置列宽
    ui->taskTable->horizontalHeader()->setStretchLastSection(false);
    ui->taskTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    ui->taskTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    ui->taskTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    ui->taskTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    ui->taskTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    ui->taskTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    ui->taskTable->horizontalHeader()->setSectionResizeMode(6, QHeaderView::ResizeToContents);
    ui->taskTable->horizontalHeader()->setSectionResizeMode(7, QHeaderView::ResizeToContents);
}

void MainWindow::onAddTaskClicked()
{
    QString url = ui->urlEdit->text().trimmed();
    if (url.isEmpty()) {
        QMessageBox::warning(this, tr("警告"), tr("请输入下载地址"));
        return;
    }
    
    QString savePath = ui->savePathEdit->text().trimmed();
    if (savePath.isEmpty()) {
        savePath = m_downloadManager->getDefaultSavePath();
    }
    
    DownloadTask::ProtocolType protocol = static_cast<DownloadTask::ProtocolType>(ui->protocolComboBox->currentData().toInt());
    
    QString username;
    QString password;
    if (ui->authCheckBox->isChecked()) {
        username = ui->usernameEdit->text();
        password = ui->passwordEdit->text();
    }

    QString taskId = m_downloadManager->addTask(url, savePath, protocol, username, password);
    
    // 清空输入框
    ui->urlEdit->clear();
    ui->savePathEdit->clear();
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

void MainWindow::onStartTaskClicked()
{
    QModelIndex currentIndex = ui->taskTable->currentIndex();
    if (!currentIndex.isValid()) {
        QMessageBox::warning(this, tr("警告"), tr("请选择要开始的任务"));
        return;
    }
    
    QString taskId = m_taskModel->data(m_taskModel->index(currentIndex.row(), 0), Qt::UserRole).toString();
    m_downloadManager->startTask(taskId);
}

void MainWindow::onPauseTaskClicked()
{
    QModelIndex currentIndex = ui->taskTable->currentIndex();
    if (!currentIndex.isValid()) {
        QMessageBox::warning(this, tr("警告"), tr("请选择要暂停的任务"));
        return;
    }
    
    QString taskId = m_taskModel->data(m_taskModel->index(currentIndex.row(), 0), Qt::UserRole).toString();
    m_downloadManager->pauseTask(taskId);
}

void MainWindow::onResumeTaskClicked()
{
    QModelIndex currentIndex = ui->taskTable->currentIndex();
    if (!currentIndex.isValid()) {
        QMessageBox::warning(this, tr("警告"), tr("请选择要恢复的任务"));
        return;
    }
    
    QString taskId = m_taskModel->data(m_taskModel->index(currentIndex.row(), 0), Qt::UserRole).toString();
    m_downloadManager->resumeTask(taskId);
}

void MainWindow::onCancelTaskClicked()
{
    QModelIndex currentIndex = ui->taskTable->currentIndex();
    if (!currentIndex.isValid()) {
        QMessageBox::warning(this, tr("警告"), tr("请选择要取消的任务"));
        return;
    }
    
    QString taskId = m_taskModel->data(m_taskModel->index(currentIndex.row(), 0), Qt::UserRole).toString();
    m_downloadManager->cancelTask(taskId);
}

void MainWindow::onTaskAdded(const QString &taskId)
{
    DownloadTask *task = m_downloadManager->getTask(taskId);
    if (task) {
        addTableRow(task);
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
                removeTableRow(task);
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
        int row = findTableRow(task);
        if (row >= 0) {
            updateTableRow(row, task);
        }
    }
}

void MainWindow::onTaskPaused(const QString &taskId)
{
    DownloadTask *task = m_downloadManager->getTask(taskId);
    if (task) {
        int row = findTableRow(task);
        if (row >= 0) {
            updateTableRow(row, task);
        }
    }
}

void MainWindow::onTaskResumed(const QString &taskId)
{
    DownloadTask *task = m_downloadManager->getTask(taskId);
    if (task) {
        int row = findTableRow(task);
        if (row >= 0) {
            updateTableRow(row, task);
        }
    }
}

void MainWindow::onTaskCancelled(const QString &taskId)
{
    DownloadTask *task = m_downloadManager->getTask(taskId);
    if (task) {
        int row = findTableRow(task);
        if (row >= 0) {
            updateTableRow(row, task);
        }
    }
}

void MainWindow::onTaskCompleted(const QString &taskId)
{
    DownloadTask *task = m_downloadManager->getTask(taskId);
    if (task) {
        int row = findTableRow(task);
        if (row >= 0) {
            updateTableRow(row, task);
        }
        showInfo(tr("下载完成：%1").arg(task->fileName()));
    }
}

void MainWindow::onTaskFailed(const QString &taskId, const QString &error)
{
    DownloadTask *task = m_downloadManager->getTask(taskId);
    if (task) {
        int row = findTableRow(task);
        if (row >= 0) {
            updateTableRow(row, task);
        }
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
                updateTableRow(row, task);
            }
        }
    }
}

void MainWindow::updateTableRow(int row, DownloadTask *task)
{
    if (!task || row < 0 || row >= ui->taskTable->rowCount()) {
        return;
    }
    
    // 协议
    ui->taskTable->item(row, 0)->setText(task->protocolText());
    
    // 文件名
    ui->taskTable->item(row, 1)->setText(task->fileName());
    
    // 状态
    ui->taskTable->item(row, 2)->setText(task->statusText());
    
    // 进度
    QProgressBar *progressBar = qobject_cast<QProgressBar*>(ui->taskTable->cellWidget(row, 3));
    if (progressBar) {
        progressBar->setValue(static_cast<int>(task->progress() * 100));
    }
    
    // 速度
    ui->taskTable->item(row, 4)->setText(task->speedText());
    
    // 大小
    QString sizeText = formatBytes(task->downloadedSize());
    if (task->fileSize() > 0) {
        sizeText += " / " + formatBytes(task->fileSize());
    }
    ui->taskTable->item(row, 5)->setText(sizeText);
    
    // 剩余时间
    ui->taskTable->item(row, 6)->setText(task->timeRemainingText());
    
    // 操作按钮
    updateOperationButtons(row, task);
}

void MainWindow::addTableRow(DownloadTask *task)
{
    int row = ui->taskTable->rowCount();
    ui->taskTable->insertRow(row);
    
    // 协议
    ui->taskTable->setItem(row, 0, new QTableWidgetItem(task->protocolText()));
    
    // 文件名
    QTableWidgetItem *fileNameItem = new QTableWidgetItem(task->fileName());
    fileNameItem->setData(Qt::UserRole, QVariant::fromValue(static_cast<void*>(task)));
    ui->taskTable->setItem(row, 1, fileNameItem);
    
    // 状态
    ui->taskTable->setItem(row, 2, new QTableWidgetItem(task->statusText()));
    
    // 进度条
    QProgressBar *progressBar = new QProgressBar();
    progressBar->setRange(0, 100);
    progressBar->setValue(0);
    ui->taskTable->setCellWidget(row, 3, progressBar);
    
    // 速度
    ui->taskTable->setItem(row, 4, new QTableWidgetItem(task->speedText()));
    
    // 大小
    ui->taskTable->setItem(row, 5, new QTableWidgetItem(formatBytes(task->downloadedSize())));
    
    // 剩余时间
    ui->taskTable->setItem(row, 6, new QTableWidgetItem(task->timeRemainingText()));
    
    // 操作按钮
    createOperationButtons(row, task);
    
    updateTableRow(row, task);
}

void MainWindow::removeTableRow(DownloadTask *task)
{
    int row = findTableRow(task);
    if (row >= 0) {
        ui->taskTable->removeRow(row);
    }
}

int MainWindow::findTableRow(DownloadTask *task)
{
    for (int row = 0; row < ui->taskTable->rowCount(); ++row) {
        QTableWidgetItem *item = ui->taskTable->item(row, 0);
        if (item) {
            DownloadTask *rowTask = static_cast<DownloadTask*>(item->data(Qt::UserRole).value<void*>());
            if (rowTask == task) {
                return row;
            }
        }
    }
    return -1;
}

void MainWindow::createOperationButtons(int row, DownloadTask *task)
{
    QWidget *widget = new QWidget();
    QHBoxLayout *layout = new QHBoxLayout(widget);
    layout->setContentsMargins(2, 2, 2, 2);
    layout->setSpacing(2);
    
    QPushButton *startButton = new QPushButton(tr("开始"));
    QPushButton *pauseButton = new QPushButton(tr("暂停"));
    QPushButton *resumeButton = new QPushButton(tr("继续"));
    QPushButton *cancelButton = new QPushButton(tr("取消"));
    
    startButton->setProperty("row", row);
    pauseButton->setProperty("row", row);
    resumeButton->setProperty("row", row);
    cancelButton->setProperty("row", row);
    
    connect(startButton, &QPushButton::clicked, this, &MainWindow::onStartTaskClicked);
    connect(pauseButton, &QPushButton::clicked, this, &MainWindow::onPauseTaskClicked);
    connect(resumeButton, &QPushButton::clicked, this, &MainWindow::onResumeTaskClicked);
    connect(cancelButton, &QPushButton::clicked, this, &MainWindow::onCancelTaskClicked);
    
    layout->addWidget(startButton);
    layout->addWidget(pauseButton);
    layout->addWidget(resumeButton);
    layout->addWidget(cancelButton);
    
    ui->taskTable->setCellWidget(row, 7, widget);
    
    updateOperationButtons(row, task);
}

void MainWindow::updateOperationButtons(int row, DownloadTask *task)
{
    QWidget *widget = ui->taskTable->cellWidget(row, 7);
    if (!widget) return;
    
    QList<QPushButton*> buttons = widget->findChildren<QPushButton*>();
    if (buttons.size() < 4) return;
    
    QPushButton *startButton = buttons[0];
    QPushButton *pauseButton = buttons[1];
    QPushButton *resumeButton = buttons[2];
    QPushButton *cancelButton = buttons[3];
    
    // 根据任务状态设置按钮可见性
    switch (task->status()) {
    case DownloadTask::Pending:
        startButton->setVisible(true);
        pauseButton->setVisible(false);
        resumeButton->setVisible(false);
        cancelButton->setVisible(true);
        break;
    case DownloadTask::Downloading:
        startButton->setVisible(false);
        pauseButton->setVisible(true);
        resumeButton->setVisible(false);
        cancelButton->setVisible(true);
        break;
    case DownloadTask::Paused:
        startButton->setVisible(false);
        pauseButton->setVisible(false);
        resumeButton->setVisible(true);
        cancelButton->setVisible(true);
        break;
    case DownloadTask::Completed:
    case DownloadTask::Failed:
    case DownloadTask::Cancelled:
        startButton->setVisible(false);
        pauseButton->setVisible(false);
        resumeButton->setVisible(false);
        cancelButton->setVisible(false);
        break;
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


void MainWindow::loadTasks()
{
    // 实现加载任务的逻辑
}

void MainWindow::onTaskProgress(const QString &taskId, qint64 bytesReceived, qint64 bytesTotal)
{
    DownloadTask *task = m_downloadManager->getTask(taskId);
    if (task) {
        int row = findTableRow(task);
        if (row >= 0) {
            updateTableRow(row, task);
        }
    }
} 