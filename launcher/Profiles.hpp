#ifndef LAUNCHER_PROFILES_HPP
#define LAUNCHER_PROFILES_HPP

#include <QObject>
#include <QString>
#include <QVector>

enum class AfterLaunch
{
    KeepOpen,
    Hide,
    Close
};

struct Profile
{
    QString id;
    QString name;
    QString username;
    QString uuid;
    QString gameDir;
    QString language;
    AfterLaunch afterLaunch = AfterLaunch::KeepOpen;
    bool splitScreen = true;
    bool joinPrompt = false;
};

class ProfileStore : public QObject
{
    Q_OBJECT

public:
    explicit ProfileStore(const QString& path, QObject* parent = nullptr);

    bool Load(QString* error);
    bool Save(QString* error) const;

    const QVector<Profile>& Profiles() const { return mProfiles; }
    const Profile* Find(const QString& id) const;
    const Profile* Selected() const;
    QString SelectedId() const { return mSelected; }

    void Select(const QString& id);
    void Put(const Profile& profile);
    bool Remove(const QString& id);
    bool NameInUse(const QString& name, const QString& exceptId) const;
    Profile MakeNew(const QString& username) const;
    void NotifySkinsChanged() { emit Changed(); }

    static bool CheckUsername(const QString& username, QString* reason);
    static QString GameDirFor(const Profile& profile);
    static QString DefaultUsername();

signals:
    void Changed();

private:
    QString mPath;
    QVector<Profile> mProfiles;
    QString mSelected;
};

#endif
