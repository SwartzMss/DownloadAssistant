#include "tasktablewidget.h"
#include <QHeaderView>
#include <QProgressBar>
#include <QHBoxLayout>
#include <QPushButton>

TaskTableWidget::TaskTableWidget(QWidget *parent)
    : QTableWidget(parent)
{
    setupTable();
}

void TaskTableWidget::setupTable()
{
    setColumnCount(7);
    setHorizontalHeaderLabels({tr("文件名"), tr("状态"), tr("进度"),
                               tr("速度"), tr("大小"), tr("剩余时间"), tr("操作")});

    setAlternatingRowColors(true);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setEditTriggers(QAbstractItemView::NoEditTriggers);

    horizontalHeader()->setStretchLastSection(false);
    horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    horizontalHeader()->setSectionResizeMode(6, QHeaderView::ResizeToContents);
}

void TaskTableWidget::addTask(DownloadTask *task)
{
    int row = rowCount();
    insertRow(row);

    QTableWidgetItem *fileNameItem = new QTableWidgetItem(task->fileName());
    fileNameItem->setData(Qt::UserRole, QVariant::fromValue(static_cast<void*>(task)));
    setItem(row, 0, fileNameItem);

    setItem(row, 1, new QTableWidgetItem(task->statusText()));

    QProgressBar *progressBar = new QProgressBar();
    progressBar->setRange(0, 100);
    progressBar->setValue(0);
    setCellWidget(row, 2, progressBar);

    setItem(row, 3, new QTableWidgetItem(task->speedText()));
    setItem(row, 4, new QTableWidgetItem(formatBytes(task->downloadedSize())));
    setItem(row, 5, new QTableWidgetItem(task->timeRemainingText()));

    createOperationButtons(row, task);

    updateTask(task);
}

void TaskTableWidget::removeTask(DownloadTask *task)
{
    int row = findTaskRow(task);
    if (row >= 0)
        removeRow(row);
}

int TaskTableWidget::findTaskRow(DownloadTask *task)
{
    for (int row = 0; row < rowCount(); ++row) {
        QTableWidgetItem *it = item(row, 0);
        if (it) {
            DownloadTask *rowTask = static_cast<DownloadTask*>(it->data(Qt::UserRole).value<void*>());
            if (rowTask == task)
                return row;
        }
    }
    return -1;
}

void TaskTableWidget::updateTask(DownloadTask *task)
{
    int row = findTaskRow(task);
    if (row < 0)
        return;

    item(row,0)->setText(task->fileName());
    item(row,1)->setText(task->statusText());

    QProgressBar *progressBar = qobject_cast<QProgressBar*>(cellWidget(row,2));
    if (progressBar)
        progressBar->setValue(static_cast<int>(task->progress()));

    item(row,3)->setText(task->speedText());

    QString sizeText = formatBytes(task->downloadedSize());
    if (task->fileSize() > 0)
        sizeText += " / " + formatBytes(task->fileSize());
    item(row,4)->setText(sizeText);

    item(row,5)->setText(task->timeRemainingText());

    updateOperationButtons(row, task);
}

void TaskTableWidget::createOperationButtons(int row, DownloadTask *task)
{
    QWidget *widget = new QWidget();
    QHBoxLayout *layout = new QHBoxLayout(widget);
    layout->setContentsMargins(2,2,2,2);
    layout->setSpacing(2);

    QPushButton *startButton = new QPushButton(tr("开始"));
    QPushButton *pauseButton = new QPushButton(tr("暂停"));
    QPushButton *resumeButton = new QPushButton(tr("继续"));
    QPushButton *cancelButton = new QPushButton(tr("取消"));

    connect(startButton, &QPushButton::clicked, this, [this, task]() { emit startTaskRequested(task); });
    connect(pauseButton, &QPushButton::clicked, this, [this, task]() { emit pauseTaskRequested(task); });
    connect(resumeButton, &QPushButton::clicked, this, [this, task]() { emit resumeTaskRequested(task); });
    connect(cancelButton, &QPushButton::clicked, this, [this, task]() { emit cancelTaskRequested(task); });

    layout->addWidget(startButton);
    layout->addWidget(pauseButton);
    layout->addWidget(resumeButton);
    layout->addWidget(cancelButton);

    setCellWidget(row, 6, widget);
}

void TaskTableWidget::updateOperationButtons(int row, DownloadTask *task)
{
    QWidget *widget = cellWidget(row, 6);
    if (!widget) return;

    QList<QPushButton*> buttons = widget->findChildren<QPushButton*>();
    if (buttons.size() < 4) return;

    QPushButton *startButton = buttons[0];
    QPushButton *pauseButton = buttons[1];
    QPushButton *resumeButton = buttons[2];
    QPushButton *cancelButton = buttons[3];

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

QString TaskTableWidget::formatBytes(qint64 bytes) const
{
    const qint64 KB = 1024;
    const qint64 MB = 1024 * KB;
    const qint64 GB = 1024 * MB;

    if (bytes >= GB)
        return QString("%1 GB").arg(static_cast<double>(bytes) / GB, 0, 'f', 2);
    else if (bytes >= MB)
        return QString("%1 MB").arg(static_cast<double>(bytes) / MB, 0, 'f', 2);
    else if (bytes >= KB)
        return QString("%1 KB").arg(static_cast<double>(bytes) / KB, 0, 'f', 2);
    else
        return QString("%1 B").arg(bytes);
}
