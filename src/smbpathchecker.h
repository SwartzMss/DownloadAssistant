#ifndef SMBPATHCHECKER_H
#define SMBPATHCHECKER_H

#include <QObject>

class SmbPathChecker : public QObject
{
    Q_OBJECT
public:
    explicit SmbPathChecker(QObject *parent = nullptr);

public slots:
    void check(const QString &path);

signals:
    void finished(bool exists, const QString &uncPath);
};

#endif // SMBPATHCHECKER_H
