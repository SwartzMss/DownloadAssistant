#ifndef FILEBROWSERDIALOG_H
#define FILEBROWSERDIALOG_H

#include <QDialog>

class QFileSystemModel;
class QTreeView;
// SmbDirModelWorker was removed for now. It may be reintroduced
// to asynchronously populate directory models over SMB.

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
};

#endif // FILEBROWSERDIALOG_H
