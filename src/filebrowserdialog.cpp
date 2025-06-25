#include "filebrowserdialog.h"
#include <QFileSystemModel>
#include <QTreeView>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QTimer>
#include <QMessageBox>
#include <QApplication>
#include "smbdirmodelworker.h"

FileBrowserDialog::FileBrowserDialog(const QString &rootPath, QWidget *parent)
    : QDialog(parent)
    , m_model(nullptr)
    , m_view(new QTreeView(this))
    , m_thread(new QThread(this))
{
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->setEnabled(false);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    // 在后台线程加载模型，避免阻塞主线程
    auto *worker = new SmbDirModelWorker;
    worker->moveToThread(m_thread);
    connect(m_thread, &QThread::finished, worker, &QObject::deleteLater);
    connect(worker, &SmbDirModelWorker::loaded, this,
            [this, buttons](QFileSystemModel *model, const QString &path) {
                model->moveToThread(QApplication::instance()->thread());
                m_model = model;
                m_view->setModel(m_model);
                QModelIndex rootIndex = m_model->index(path);
                if (rootIndex.isValid()) {
                    m_view->setRootIndex(rootIndex);
                } else {
                    QMessageBox::critical(this, tr("错误"),
                                          tr("目录不存在: %1").arg(path));
                    reject();
                }
                // 调整列宽
                QHeaderView *header = m_view->header();
                header->setSectionResizeMode(0, QHeaderView::Stretch);
                for (int i = 1; i < m_model->columnCount(); ++i)
                    header->setSectionResizeMode(i, QHeaderView::ResizeToContents);
                buttons->setEnabled(true);
                m_thread->quit();
            });
    m_thread->start();
    QMetaObject::invokeMethod(worker, "load", Qt::QueuedConnection, Q_ARG(QString, rootPath));

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_view);
    layout->addWidget(buttons);
    setLayout(layout);
    setWindowTitle(tr("选择远程文件或目录"));
    resize(800, 600);
}

FileBrowserDialog::~FileBrowserDialog()
{
    if (m_thread->isRunning()) {
        m_thread->quit();
        m_thread->wait();
    }
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
