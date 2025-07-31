#include "pathutils.h"


QString toUncPath(QString path)
{
    path.remove('\r');
    path.remove('\n');
    path = path.trimmed();
    path.replace('/', '\\');
    return path;
}
