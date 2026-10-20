#include "UpdateCheck.hpp"

#include <QDateTime>
#include <QJsonObject>
#include <QJsonValue>

#include <algorithm>
#include <vector>

QJsonArray NewestFirst(const QJsonArray& releases)
{
    std::vector<QJsonValue> sorted(releases.begin(), releases.end());
    const auto published = [](const QJsonValue& release) {
        return QDateTime::fromString(release.toObject().value("published_at").toString(), Qt::ISODate);
    };
    std::stable_sort(sorted.begin(), sorted.end(),
                     [&](const QJsonValue& a, const QJsonValue& b) { return published(a) > published(b); });
    QJsonArray result;
    for (const QJsonValue& release : sorted)
        result.append(release);
    return result;
}

NewerRelease FindNewerRelease(const QJsonArray& releases, const QString& commit, const QDate& built)
{
    const QString ownTag = "build-" + commit;
    QJsonObject latest;
    bool ownIsRelease = false;
    for (const QJsonValue& value : NewestFirst(releases))
    {
        const QJsonObject release = value.toObject();
        if (release.value("draft").toBool())
            continue;
        if (release.value("tag_name").toString() == ownTag)
            ownIsRelease = true;
        if (latest.isEmpty() && !release.value("prerelease").toBool())
            latest = release;
    }
    if (latest.isEmpty() || latest.value("tag_name").toString() == ownTag)
        return NewerRelease();

    if (!ownIsRelease)
    {
        const QDate published =
            QDateTime::fromString(latest.value("published_at").toString(), Qt::ISODate).toUTC().date();
        if (!built.isValid() || !published.isValid() || published <= built)
            return NewerRelease();
    }

    NewerRelease newer;
    newer.name = latest.value("name").toString().trimmed();
    if (newer.name.isEmpty())
        newer.name = latest.value("tag_name").toString();
    newer.url = latest.value("html_url").toString();
    return newer;
}
