#ifndef FILEBROWSERDIALOG_H
#define FILEBROWSERDIALOG_H

#include <QDialog>

class QFileSystemModel;
class QTreeView;

class FileBrowserDialog : public QDialog
{
    Q_OBJECT
public:
    explicit FileBrowserDialog(QWidget *parent = nullptr);
    QStringList selectedPaths() const;

private:
    QFileSystemModel *m_model;
    QTreeView *m_view;
};

#endif // FILEBROWSERDIALOG_H
