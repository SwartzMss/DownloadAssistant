#include "filebrowserdialog.h"
#include <QFileSystemModel>
#include <QTreeView>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QHeaderView>

FileBrowserDialog::FileBrowserDialog(const QString &rootPath, QWidget *parent)
    : QDialog(parent), m_model(new QFileSystemModel(this)), m_view(new QTreeView(this))
{
    QModelIndex rootIndex = m_model->setRootPath(rootPath);
    m_view->setModel(m_model);
    if (rootIndex.isValid())
        m_view->setRootIndex(rootIndex);
    m_view->setSelectionMode(QAbstractItemView::ExtendedSelection);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_view);
    layout->addWidget(buttons);
    setLayout(layout);
    setWindowTitle(tr("选择远程文件或目录"));
    resize(800, 600);

    // Optimize column sizes so file names are readable
    QHeaderView *header = m_view->header();
    header->setSectionResizeMode(0, QHeaderView::Stretch);
    for (int i = 1; i < m_model->columnCount(); ++i)
        header->setSectionResizeMode(i, QHeaderView::ResizeToContents);
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
