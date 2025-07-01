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
    // Remove the "状态" column, leaving only six columns
    setColumnCount(6);
    setHorizontalHeaderLabels({tr("文件名"), tr("进度"),
                               tr("速度"), tr("大小"), tr("剩余时间"), tr("操作")});

    setAlternatingRowColors(true);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setEditTriggers(QAbstractItemView::NoEditTriggers);

    horizontalHeader()->setStretchLastSection(false);
    horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);          // 文件名
    horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed); // 进度
    horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed); // 速度
    horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed); // 大小
    horizontalHeader()->setSectionResizeMode(4, QHeaderView::Fixed); // 剩余时间
    horizontalHeader()->setSectionResizeMode(5, QHeaderView::Fixed); // 操作

    setColumnWidth(1, 90); // 进度
    setColumnWidth(2, 90); // 速度
    setColumnWidth(3, 90); // 大小
    setColumnWidth(4, 100); // 剩余时间
    setColumnWidth(5, 120); // 操作

    // Display long file names with ellipsis in the middle
    setTextElideMode(Qt::ElideMiddle);
    setWordWrap(false);

    // Limit the maximum width of the file name column
    horizontalHeader()->setMaximumSectionSize(400);
}

void TaskTableWidget::addTask(DownloadTask *task)
{
    int row = rowCount();
    insertRow(row);

    QTableWidgetItem *fileNameItem = new QTableWidgetItem(task->fileName());
    fileNameItem->setToolTip(task->fileName());
    fileNameItem->setData(Qt::UserRole, QVariant::fromValue(static_cast<void*>(task)));
    setItem(row, 0, fileNameItem);

    QProgressBar *progressBar = new QProgressBar();
    progressBar->setRange(0, 100);
    progressBar->setValue(0);
    setCellWidget(row, 1, progressBar);

    setItem(row, 2, new QTableWidgetItem(task->speedText()));
    setItem(row, 3, new QTableWidgetItem(formatBytes(task->fileSize())));
    setItem(row, 4, new QTableWidgetItem(task->timeRemainingText()));

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
    item(row,0)->setToolTip(task->fileName());

    QProgressBar *progressBar = qobject_cast<QProgressBar*>(cellWidget(row,1));
    if (progressBar)
        progressBar->setValue(static_cast<int>(task->progress()));

    item(row,2)->setText(task->speedText());

    item(row,3)->setText(formatBytes(task->fileSize()));

    item(row,4)->setText(task->timeRemainingText());

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

    // Operation column index changed to 5 after removing status column
    setCellWidget(row, 5, widget);
}

void TaskTableWidget::updateOperationButtons(int row, DownloadTask *task)
{
    QWidget *widget = cellWidget(row, 5);
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
    case DownloadTask::Queued:
        startButton->setVisible(false);
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
