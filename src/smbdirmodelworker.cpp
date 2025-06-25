#include "smbdirmodelworker.h"
#include <QFileSystemModel>
#include "logger.h"

SmbDirModelWorker::SmbDirModelWorker(QObject *parent)
    : QObject(parent)
{
}

void SmbDirModelWorker::load(const QString &rootPath)
{
    LOG_INFO(QString("加载目录模型: %1").arg(rootPath));
    QFileSystemModel *model = new QFileSystemModel;
    model->setRootPath(rootPath);
    emit loaded(model, rootPath);
    LOG_INFO(QString("目录模型加载完成: %1").arg(rootPath));
}
