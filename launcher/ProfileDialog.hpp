#ifndef LAUNCHER_PROFILEDIALOG_HPP
#define LAUNCHER_PROFILEDIALOG_HPP

#include <QDialog>

#include "Profiles.hpp"

class QComboBox;
class QLabel;
class QLineEdit;

class ProfileDialog : public QDialog
{
    Q_OBJECT

public:
    ProfileDialog(const ProfileStore& store, const Profile& profile, bool isNew, QWidget* parent = nullptr);

    Profile Result() const { return mProfile; }

    static bool Run(QWidget* parent, ProfileStore& store, const Profile& profile, bool isNew);

private:
    void accept() override;

    const ProfileStore& mStore;
    Profile mProfile;
    QLineEdit* mName;
    QLineEdit* mUsername;
    QLabel* mUuid;
    QLineEdit* mGameDir;
    QComboBox* mLanguage;
    QComboBox* mAfterLaunch;
};

#endif
