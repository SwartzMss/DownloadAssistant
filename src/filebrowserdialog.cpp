#include "filebrowserdialog.h"
#include <QFileSystemModel>
#include <QTreeView>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QTimer>
#include <QMessageBox>
#include <QApplication>

FileBrowserDialog::FileBrowserDialog(const QString &rootPath, QWidget *parent)
    : QDialog(parent)
    , m_model(new QFileSystemModel(this))
    , m_view(new QTreeView(this))
{
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->setEnabled(false);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    m_model->setRootPath(rootPath);
    m_view->setModel(m_model);
    QModelIndex rootIndex = m_model->index(rootPath);
    if (rootIndex.isValid()) {
        m_view->setRootIndex(rootIndex);
        QHeaderView *header = m_view->header();
        header->setSectionResizeMode(0, QHeaderView::Stretch);
        for (int i = 1; i < m_model->columnCount(); ++i)
            header->setSectionResizeMode(i, QHeaderView::ResizeToContents);
        buttons->setEnabled(true);
    } else {
        QMessageBox::critical(this, tr("错误"), tr("目录不存在: %1").arg(rootPath));
        buttons->setEnabled(false);
    }

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_view);
    layout->addWidget(buttons);
    setLayout(layout);
    setWindowTitle(tr("选择远程文件或目录"));
    resize(800, 600);
}

FileBrowserDialog::~FileBrowserDialog()
{
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
