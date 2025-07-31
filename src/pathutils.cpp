#include "pathutils.h"


QString toUncPath(QString path)
{
    path.remove('\r');
    path.remove('\n');
    path = path.trimmed();
    if (path.startsWith("smb://", Qt::CaseInsensitive)) {
        QString p = path.mid(6); // remove "smb://"
        int slash = p.indexOf('/');
        QString host;
        QString rest;
        if (slash >= 0) {
            host = p.left(slash);
            rest = p.mid(slash + 1);
        } else {
            host = p;
        }
        rest.replace('/', '\\');
        if (!rest.isEmpty())
            return QStringLiteral("\\\\") + host + QLatin1Char('\\') + rest;
        else
            return QStringLiteral("\\\\") + host + QLatin1Char('\\');
    }
    path.replace('/', '\\');
    return path;
}
