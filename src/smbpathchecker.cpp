#include "smbpathchecker.h"
#include <QDir>
#include <QUrl>

static QString toUncPath(QString path)
{
    path.remove('\r');
    path.remove('\n');
    path = path.trimmed();
    if (path.startsWith("smb://", Qt::CaseInsensitive)) {
        QUrl u(path);
        QString p = u.path();
        if (p.startsWith('/'))
            p.remove(0, 1);
        p.replace('/', '\\');
        return QStringLiteral("\\\\") + u.host() + QLatin1Char('\\') + p;
    }
    path.replace('/', '\\');
    return path;
}

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

