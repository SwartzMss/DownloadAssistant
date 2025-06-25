#ifndef SMBDIRMODELWORKER_H
#define SMBDIRMODELWORKER_H

#include <QObject>

class QFileSystemModel;

class SmbDirModelWorker : public QObject
{
    Q_OBJECT
public:
    explicit SmbDirModelWorker(QObject *parent = nullptr);

public slots:
    void load(const QString &rootPath);

signals:
    void loaded(QFileSystemModel *model, const QString &rootPath);
};

#endif // SMBDIRMODELWORKER_H
