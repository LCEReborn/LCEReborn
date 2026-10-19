#include "Profiles.hpp"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QUuid>

#include "LauncherPaths.hpp"

namespace
{
const int kFormatVersion = 1;

QString AfterLaunchName(AfterLaunch value)
{
    switch (value)
    {
    case AfterLaunch::Hide:
        return "hide";
    case AfterLaunch::Close:
        return "close";
    default:
        return "keep";
    }
}

AfterLaunch AfterLaunchFromName(const QString& name)
{
    if (name == "hide")
        return AfterLaunch::Hide;
    if (name == "close")
        return AfterLaunch::Close;
    return AfterLaunch::KeepOpen;
}

bool IsHexUuid(const QString& text)
{
    static const QRegularExpression pattern("^[0-9a-f]{32}$");
    return pattern.match(text).hasMatch();
}

QString RandomPlayerUuid()
{
    return QUuid::createUuid().toString(QUuid::Id128);
}
}

ProfileStore::ProfileStore(const QString& path, QObject* parent) : QObject(parent), mPath(path)
{
}

bool ProfileStore::Load(QString* error)
{
    mProfiles.clear();
    mSelected.clear();

    QFile file(mPath);
    if (file.exists())
    {
        if (!file.open(QIODevice::ReadOnly))
        {
            *error = QString("%1 could not be read: %2").arg(QDir::toNativeSeparators(mPath), file.errorString());
            return false;
        }
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
        if (!document.isObject())
        {
            *error = QString("%1 is not valid: %2").arg(QDir::toNativeSeparators(mPath), parseError.errorString());
            return false;
        }
        const QJsonObject root = document.object();
        for (const QJsonValue& value : root.value("profiles").toArray())
        {
            const QJsonObject object = value.toObject();
            Profile profile;
            profile.id = object.value("id").toString();
            profile.name = object.value("name").toString().trimmed();
            profile.username = object.value("username").toString().trimmed();
            profile.uuid = object.value("uuid").toString().toLower();
            profile.gameDir = object.value("gameDir").toString();
            profile.language = object.value("language").toString();
            profile.afterLaunch = AfterLaunchFromName(object.value("afterLaunch").toString());
            profile.splitScreen = object.value("splitScreen").toBool(true);
            profile.joinPrompt = object.value("joinPrompt").toBool(false);
            if (profile.id.isEmpty() || profile.name.isEmpty() || NameInUse(profile.name, QString()))
                continue;
            if (!CheckUsername(profile.username, nullptr))
                profile.username = DefaultUsername();
            if (!IsHexUuid(profile.uuid))
                profile.uuid = RandomPlayerUuid();
            mProfiles.append(profile);
        }
        mSelected = root.value("selected").toString();
    }

    if (mProfiles.isEmpty())
    {
        Profile profile = MakeNew(DefaultUsername());
        profile.name = "Default";
        mProfiles.append(profile);
    }
    if (Find(mSelected) == nullptr)
        mSelected = mProfiles.first().id;
    emit Changed();
    return true;
}

bool ProfileStore::Save(QString* error) const
{
    QJsonArray profiles;
    for (const Profile& profile : mProfiles)
    {
        QJsonObject object;
        object.insert("id", profile.id);
        object.insert("name", profile.name);
        object.insert("username", profile.username);
        object.insert("uuid", profile.uuid);
        object.insert("gameDir", profile.gameDir);
        object.insert("language", profile.language);
        object.insert("afterLaunch", AfterLaunchName(profile.afterLaunch));
        object.insert("splitScreen", profile.splitScreen);
        object.insert("joinPrompt", profile.joinPrompt);
        profiles.append(object);
    }
    QJsonObject root;
    root.insert("version", kFormatVersion);
    root.insert("selected", mSelected);
    root.insert("profiles", profiles);

    QSaveFile file(mPath);
    if (!file.open(QIODevice::WriteOnly))
    {
        *error = QString("%1 could not be written: %2").arg(QDir::toNativeSeparators(mPath), file.errorString());
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!file.commit())
    {
        *error = QString("%1 could not be written: %2").arg(QDir::toNativeSeparators(mPath), file.errorString());
        return false;
    }
    return true;
}

const Profile* ProfileStore::Find(const QString& id) const
{
    for (const Profile& profile : mProfiles)
    {
        if (profile.id == id)
            return &profile;
    }
    return nullptr;
}

const Profile* ProfileStore::Selected() const
{
    return Find(mSelected);
}

void ProfileStore::Select(const QString& id)
{
    if (Find(id) != nullptr && id != mSelected)
    {
        mSelected = id;
        emit Changed();
    }
}

void ProfileStore::Put(const Profile& profile)
{
    for (Profile& existing : mProfiles)
    {
        if (existing.id == profile.id)
        {
            existing = profile;
            emit Changed();
            return;
        }
    }
    mProfiles.append(profile);
    emit Changed();
}

bool ProfileStore::Remove(const QString& id)
{
    if (mProfiles.size() <= 1)
        return false;
    for (int index = 0; index < mProfiles.size(); ++index)
    {
        if (mProfiles[index].id == id)
        {
            mProfiles.removeAt(index);
            if (mSelected == id)
                mSelected = mProfiles.first().id;
            emit Changed();
            return true;
        }
    }
    return false;
}

bool ProfileStore::NameInUse(const QString& name, const QString& exceptId) const
{
    for (const Profile& profile : mProfiles)
    {
        if (profile.id != exceptId && profile.name.compare(name.trimmed(), Qt::CaseInsensitive) == 0)
            return true;
    }
    return false;
}

Profile ProfileStore::MakeNew(const QString& username) const
{
    Profile profile;
    profile.id = QUuid::createUuid().toString(QUuid::Id128);
    profile.username = username;
    profile.uuid = RandomPlayerUuid();
    QString name = username;
    for (int number = 2; NameInUse(name, QString()); ++number)
        name = QString("%1 (%2)").arg(username).arg(number);
    profile.name = name;
    return profile;
}

bool ProfileStore::CheckUsername(const QString& username, QString* reason)
{
    static const QRegularExpression pattern("^[A-Za-z0-9_]{3,16}$");
    if (pattern.match(username).hasMatch())
        return true;
    if (reason != nullptr)
        *reason = "A username is 3 to 16 letters, digits or underscores.";
    return false;
}

QString ProfileStore::GameDirFor(const Profile& profile)
{
    return profile.gameDir.isEmpty() ? LauncherPaths::DataDir() : QDir(profile.gameDir).absolutePath();
}

QString ProfileStore::DefaultUsername()
{
    QString name = qEnvironmentVariable("USERNAME", qEnvironmentVariable("USER"));
    name.remove(QRegularExpression("[^A-Za-z0-9_]"));
    name.truncate(16);
    return CheckUsername(name, nullptr) ? name : QString("Player");
}
