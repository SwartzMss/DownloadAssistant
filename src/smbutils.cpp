#include "smbutils.h"

QString normalizeSmbUrl(const QString &input)
{
    QString url = input;
    url.remove('\r');
    url.remove('\n');
    url = url.trimmed();

    // Replace backslashes with forward slashes for UNC style paths
    url.replace('\\', "/");

    // If URL already starts with smb:// or smb2://, just return after replacement
    if (url.startsWith("smb://", Qt::CaseInsensitive) ||
        url.startsWith("smb2://", Qt::CaseInsensitive)) {
        return url;
    }

    // Remove leading slashes if any
    while (url.startsWith('/')) {
        url.remove(0, 1);
    }

    // Prepend smb:// scheme
    url.prepend("smb://");
    return url;
}

void parseDomainUser(const QString &input, QString &domain, QString &user)
{
    user = input;
    domain.clear();
    int idx = input.indexOf('\x5c'); // backslash
    if (idx < 0)
        idx = input.indexOf(';');
    if (idx > 0) {
        domain = input.left(idx);
        user = input.mid(idx + 1);
    }
}
