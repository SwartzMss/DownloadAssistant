#include "smbpathchecker.h"
#include <QDir>
#include <QUrl>
#include "logger.h"

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
    LOG_INFO(QString("检查远程路径: %1").arg(path));
    QString uncPath = toUncPath(path);
    bool ok = QDir(uncPath).exists();
    LOG_INFO(QString("路径 %1 %2").arg(uncPath).arg(ok ? "存在" : "不存在"));
    emit finished(ok, uncPath);
}

