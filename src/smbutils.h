#ifndef SMBUTILS_H
#define SMBUTILS_H

#include <QString>

QString normalizeSmbUrl(const QString &input);
void parseDomainUser(const QString &input, QString &domain, QString &user);

#endif // SMBUTILS_H
