#ifndef TASKTABLEWIDGET_H
#define TASKTABLEWIDGET_H

#include <QTableWidget>
#include "downloadtask.h"

class TaskTableWidget : public QTableWidget
{
    Q_OBJECT
public:
    explicit TaskTableWidget(QWidget *parent = nullptr);

    void setupTable();
    void addTask(DownloadTask *task);
    void removeTask(DownloadTask *task);
    void updateTask(DownloadTask *task);
    int findTaskRow(DownloadTask *task);

signals:
    void startTaskRequested(DownloadTask *task);
    void pauseTaskRequested(DownloadTask *task);
    void resumeTaskRequested(DownloadTask *task);
    void cancelTaskRequested(DownloadTask *task);

private:
    void createOperationButtons(int row, DownloadTask *task);
    void updateOperationButtons(int row, DownloadTask *task);
    QString formatBytes(qint64 bytes) const;
};

#endif // TASKTABLEWIDGET_H
