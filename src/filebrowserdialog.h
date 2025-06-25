#ifndef FILEBROWSERDIALOG_H
#define FILEBROWSERDIALOG_H

#include <QDialog>
#include <QThread>

class SmbDirModelWorker;

class QFileSystemModel;
class QTreeView;

class FileBrowserDialog : public QDialog
{
    Q_OBJECT
public:
    explicit FileBrowserDialog(const QString &rootPath = QString(), QWidget *parent = nullptr);
    ~FileBrowserDialog() override;
    QStringList selectedPaths() const;

private:
    QFileSystemModel *m_model;
    QTreeView *m_view;
    QThread *m_thread;
};

#endif // FILEBROWSERDIALOG_H
