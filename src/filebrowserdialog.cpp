#include "filebrowserdialog.h"
#include <QFileSystemModel>
#include <QTreeView>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QTimer>
#include <QMessageBox>

FileBrowserDialog::FileBrowserDialog(const QString &rootPath, QWidget *parent)
    : QDialog(parent)
    , m_model(new QFileSystemModel(this))
    , m_view(new QTreeView(this))
{
    // 先设置一个空的根路径，避免阻塞对话框显示
    m_model->setRootPath(QString());
    m_view->setModel(m_model);

    // 延迟加载实际的远程根目录，以减少界面卡顿
    QTimer::singleShot(0, this, [this, rootPath] {
        QModelIndex rootIndex = m_model->setRootPath(rootPath);
        if (rootIndex.isValid()) {
            m_view->setRootIndex(rootIndex);
        } else {
            QMessageBox::critical(this, tr("错误"),
                                  tr("目录不存在: %1").arg(rootPath));
            reject();
        }
    });

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
