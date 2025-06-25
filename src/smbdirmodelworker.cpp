#include "smbdirmodelworker.h"
#include <QFileSystemModel>

SmbDirModelWorker::SmbDirModelWorker(QObject *parent)
    : QObject(parent)
{
}

void SmbDirModelWorker::load(const QString &rootPath)
{
    QFileSystemModel *model = new QFileSystemModel;
    model->setRootPath(rootPath);
    emit loaded(model, rootPath);
}
