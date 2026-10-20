#ifndef LAUNCHER_UPDATECHECK_HPP
#define LAUNCHER_UPDATECHECK_HPP

#include <QDate>
#include <QJsonArray>
#include <QString>

struct NewerRelease
{
    QString name;
    QString url;
};

QJsonArray NewestFirst(const QJsonArray& releases);
NewerRelease FindNewerRelease(const QJsonArray& releases, const QString& commit, const QDate& built);

#endif
