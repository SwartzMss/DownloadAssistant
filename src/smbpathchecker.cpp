#include "smbpathchecker.h"
#include <QDir>
#include <QUrl>
#include "pathutils.h"

SmbPathChecker::SmbPathChecker(QObject *parent)
    : QObject(parent)
{
}

void SmbPathChecker::check(const QString &path)
{
    QString uncPath = toUncPath(path);
    bool ok = QDir(uncPath).exists();
    emit finished(ok, uncPath);
}

