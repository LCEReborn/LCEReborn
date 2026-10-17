#include "ProfileDialog.hpp"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

#include "LauncherPaths.hpp"

namespace
{
struct Language
{
    const char* code;
    const char* name;
};

const Language kLanguages[] = {
    {"", "System language"},    {"en", "English"},       {"fr", "Français"},           {"de", "Deutsch"},
    {"it", "Italiano"},         {"es", "Español"},       {"pt", "Português"},          {"nl", "Nederlands"},
    {"ru", "Русский"},          {"ja", "日本語"},        {"ko", "한국어"},             {"zh-Hans", "简体中文"},
    {"zh-Hant", "繁體中文"},
};
}

ProfileDialog::ProfileDialog(const ProfileStore& store, const Profile& profile, bool isNew, QWidget* parent)
    : QDialog(parent), mStore(store), mProfile(profile), mName(new QLineEdit(profile.name, this)),
      mUsername(new QLineEdit(profile.username, this)), mUuid(new QLabel(this)),
      mGameDir(new QLineEdit(QDir::toNativeSeparators(profile.gameDir), this)), mLanguage(new QComboBox(this)),
      mAfterLaunch(new QComboBox(this))
{
    setWindowTitle(isNew ? "New Profile" : "Edit Profile");
    setMinimumWidth(520);

    mUsername->setMaxLength(16);
    mUuid->setObjectName("Muted");
    mUuid->setTextInteractionFlags(Qt::TextSelectableByMouse);
    mGameDir->setPlaceholderText(QDir::toNativeSeparators(LauncherPaths::DataDir()));
    for (const Language& language : kLanguages)
    {
        mLanguage->addItem(QString::fromUtf8(language.name), QString(language.code));
        if (profile.language == language.code)
            mLanguage->setCurrentIndex(mLanguage->count() - 1);
    }
    mAfterLaunch->addItem("Keep the launcher open", static_cast<int>(AfterLaunch::KeepOpen));
    mAfterLaunch->addItem("Hide the launcher until the game closes", static_cast<int>(AfterLaunch::Hide));
    mAfterLaunch->addItem("Close the launcher", static_cast<int>(AfterLaunch::Close));
    mAfterLaunch->setCurrentIndex(mAfterLaunch->findData(static_cast<int>(profile.afterLaunch)));

    auto* browse = new QPushButton("Browse…", this);
    auto* gameDirRow = new QHBoxLayout;
    gameDirRow->addWidget(mGameDir, 1);
    gameDirRow->addWidget(browse);
    connect(browse, &QPushButton::clicked, this, [this] {
        const QString folder = QFileDialog::getExistingDirectory(this, "Choose the game folder",
                                                                 mGameDir->text().isEmpty() ? LauncherPaths::DataDir()
                                                                                            : mGameDir->text());
        if (!folder.isEmpty())
            mGameDir->setText(QDir::toNativeSeparators(folder));
    });

    auto* form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignRight);
    form->setVerticalSpacing(10);
    form->addRow("Profile name", mName);
    form->addRow("Username", mUsername);
    form->addRow("Player identity", mUuid);
    form->addRow("Game folder", gameDirRow);
    form->addRow("Language", mLanguage);
    form->addRow("When the game starts", mAfterLaunch);


    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &ProfileDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &ProfileDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 18, 20, 16);
    layout->setSpacing(14);
    layout->addLayout(form);
    layout->addWidget(buttons);

    mUuid->setText(mProfile.uuid);
}

void ProfileDialog::accept()
{
    const QString name = mName->text().trimmed();
    const QString username = mUsername->text().trimmed();
    QString reason;
    if (name.isEmpty())
    {
        QMessageBox::warning(this, windowTitle(), "Give the profile a name.");
        return;
    }
    if (mStore.NameInUse(name, mProfile.id))
    {
        QMessageBox::warning(this, windowTitle(), QString("There's already a profile called %1.").arg(name));
        return;
    }
    if (!ProfileStore::CheckUsername(username, &reason))
    {
        QMessageBox::warning(this, windowTitle(), reason);
        return;
    }
    const QString gameDir = QDir::fromNativeSeparators(mGameDir->text().trimmed());
    if (!gameDir.isEmpty() && !QDir::isAbsolutePath(gameDir))
    {
        QMessageBox::warning(this, windowTitle(), "Choose a full path for the game folder, or leave it empty.");
        return;
    }

    mProfile.name = name;
    mProfile.username = username;
    mProfile.gameDir = gameDir;
    mProfile.language = mLanguage->currentData().toString();
    mProfile.afterLaunch = static_cast<AfterLaunch>(mAfterLaunch->currentData().toInt());
    QDialog::accept();
}

bool ProfileDialog::Run(QWidget* parent, ProfileStore& store, const Profile& profile, bool isNew)
{
    ProfileDialog dialog(store, profile, isNew, parent);
    if (dialog.exec() != QDialog::Accepted)
        return false;
    store.Put(dialog.Result());
    store.Select(dialog.Result().id);
    QString error;
    if (!store.Save(&error))
    {
        QMessageBox::warning(parent, "Profiles", error);
        return false;
    }
    return true;
}
