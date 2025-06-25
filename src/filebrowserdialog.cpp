#include "filebrowserdialog.h"
#include <QFileSystemModel>
#include <QTreeView>
#include <QVBoxLayout>
#include <QDialogButtonBox>

FileBrowserDialog::FileBrowserDialog(QWidget *parent)
    : QDialog(parent), m_model(new QFileSystemModel(this)), m_view(new QTreeView(this))
{
    m_model->setRootPath(QString());
    m_view->setModel(m_model);
    m_view->setSelectionMode(QAbstractItemView::ExtendedSelection);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_view);
    layout->addWidget(buttons);
    setLayout(layout);
    setWindowTitle(tr("选择远程文件或目录"));
}

QStringList FileBrowserDialog::selectedPaths() const
{
    QStringList paths;
    QModelIndexList indexes = m_view->selectionModel()->selectedRows();
    for (const QModelIndex &index : indexes) {
        paths << m_model->filePath(index);
    }
    return paths;
}
