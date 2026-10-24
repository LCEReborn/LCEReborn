#include "MainWindow.hpp"

#include <QApplication>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QStackedWidget>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include "BuildVersion.hpp"
#include "GameArt.hpp"
#include "GameRunner.hpp"
#include "LauncherPaths.hpp"
#include "NewsTab.hpp"
#include "ProfileDialog.hpp"
#include "ProfilePopup.hpp"
#include "Profiles.hpp"
#include "ProfilesTab.hpp"
#include "SetupPage.hpp"
#include "Skins.hpp"
#include "Widgets.hpp"

namespace
{
enum Page
{
    NewsPage,
    LogPage,
    ProfilesPage,
    PageCount
};

const char* const kPageNames[PageCount] = {"News", "Game Log", "Profiles"};
const int kBottomRowHeight = 62;
}

MainWindow::MainWindow(ProfileStore& store, QWidget* parent)
    : QMainWindow(parent), mStore(store), mRunner(new GameRunner(this)), mPanorama(new Panorama(this)),
      mRoot(new QStackedWidget(mPanorama)), mShellLayout(nullptr), mTabs(new TabbedPanel(this)),
      mPages(new QStackedWidget(this)), mSetup(new SetupPage(this)), mNews(new NewsTab(this)),
      mLog(new GameLogTab(this)), mProfileButton(new GameButton(QString(), this)), mPlay(new PlayButton(this)),
      mStatus(new ShadowLabel(QString(), 16, this))
{
    setWindowTitle(QString("LCEReborn Launcher - %1").arg(BuildVersion()));
    resize(1280, 720);
    setMinimumSize(960, 600);

    mPages->addWidget(mNews);
    mPages->addWidget(mLog);
    auto* profiles = new ProfilesTab(mStore, this);
    mPages->addWidget(profiles);
    for (const char* name : kPageNames)
        mTabs->AddTab(name);
    mTabs->SetContent(mPages);
    connect(mTabs, &TabbedPanel::CurrentChanged, mPages, &QStackedWidget::setCurrentIndex);

    auto* corner = new QWidget(this);
    auto* cornerLayout = new QHBoxLayout(corner);
    cornerLayout->setContentsMargins(0, 0, 0, 0);
    cornerLayout->setSpacing(12);
    auto* mark = new QLabel(corner);
    mark->setPixmap(QPixmap(":/launcher/mark.png").scaled(32, 32, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    mark->setFixedSize(32, 32);
    cornerLayout->addStretch(1);
    cornerLayout->addWidget(mark);
    cornerLayout->addWidget(new ShadowLabel("LCE Reborn", 24, corner));
    mTabs->SetCorner(corner);

    mProfileButton->SetPixelSize(20);
    mProfileButton->setFixedSize(360, kBottomRowHeight);
    mPlay->setFixedSize(280, kBottomRowHeight);
    auto* update = new GameButton("Update", this);
    update->SetPixelSize(20);
    update->setFixedSize(150, kBottomRowHeight);
    update->hide();
    auto* build = new ShadowLabel(QString("Build %1").arg(BuildVersion()), 16, this);
    auto* statusColumn = new QVBoxLayout;
    statusColumn->setSpacing(6);
    statusColumn->addStretch(1);
    statusColumn->addWidget(mStatus);
    statusColumn->addWidget(build);
    statusColumn->addStretch(1);

    auto* bottomRow = new QHBoxLayout;
    bottomRow->setSpacing(20);
    bottomRow->addWidget(mProfileButton);
    bottomRow->addLayout(statusColumn, 1);
    bottomRow->addWidget(update);
    bottomRow->addWidget(mPlay);

    auto* shell = new QWidget(this);
    mShellLayout = new QVBoxLayout(shell);
    mShellLayout->setSpacing(12);
    mShellLayout->addWidget(mTabs, 1);
    mShellLayout->addLayout(bottomRow);

    mRoot->addWidget(mSetup);
    mRoot->addWidget(shell);
    auto* rootLayout = new QVBoxLayout(mPanorama);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->addWidget(mRoot);
    setCentralWidget(mPanorama);

    connect(&mStore, &ProfileStore::Changed, this, &MainWindow::RefreshProfiles);
    connect(mProfileButton, &GameButton::clicked, this, &MainWindow::ShowProfileMenu);
    connect(mPlay, &PlayButton::clicked, this, &MainWindow::Play);
    connect(mRunner, &GameRunner::Output, mLog, &GameLogTab::Add);
    connect(mRunner, &GameRunner::Started, this, &MainWindow::OnGameStarted);
    connect(mRunner, &GameRunner::Finished, this, &MainWindow::OnGameFinished);
    connect(mSetup, &SetupPage::Message, mLog, &GameLogTab::AddLauncherMessage);
    connect(mSetup, &SetupPage::Imported, this, &MainWindow::OnImported);
    connect(mSetup, &SetupPage::BackRequested, this, [this] { mRoot->setCurrentIndex(1); });
    connect(mNews, &NewsTab::UpdateChecked, this, [update](const QString& newerName, const QString& newerUrl) {
        update->setVisible(!newerName.isEmpty());
        update->setToolTip(QString("%1 is out. Open its download page.").arg(newerName));
        update->setProperty("url", newerUrl);
    });
    connect(update, &GameButton::clicked, this,
            [update] { QDesktopServices::openUrl(QUrl(update->property("url").toString())); });
    connect(profiles, &ProfilesTab::ReimportRequested, this, [this] {
        if (mRunner->IsRunning())
        {
            QMessageBox::information(this, "Re-import Game Files", "Close the game before importing its files again.");
            return;
        }
        mSetup->SetCanGoBack(true);
        mRoot->setCurrentIndex(0);
    });

    QSettings settings(LauncherPaths::SettingsFile(), QSettings::IniFormat);
    restoreGeometry(settings.value("window/geometry").toByteArray());
    const int page = settings.value("window/page", NewsPage).toInt();
    ShowPage(page >= 0 && page < PageCount ? page : NewsPage);
    const bool imported = QFileInfo(LauncherPaths::ContentCheckFile()).isFile();
    mRoot->setCurrentIndex(imported ? 1 : 0);
    if (imported && !settings.value("notices/packsFolder", false).toBool())
        QTimer::singleShot(0, this, &MainWindow::ShowPacksNotice);

    RefreshProfiles();
}

void MainWindow::ShowPage(int index)
{
    mTabs->SetCurrent(index);
    mPages->setCurrentIndex(index);
}

void MainWindow::resizeEvent(QResizeEvent* event)
{
    QMainWindow::resizeEvent(event);
    if (mShellLayout == nullptr)
        return;
    const int sides = qMax(16, width() * 60 / 1280);
    mShellLayout->setContentsMargins(sides, qMax(12, height() * 22 / 720), sides, qMax(12, height() * 16 / 720));
}

void MainWindow::ShowProfileMenu()
{
    auto* popup = new ProfilePopup(mStore, this);
    connect(popup, &ProfilePopup::ProfileChosen, this, [this](const QString& id) {
        mStore.Select(id);
        SaveProfiles();
    });
    connect(popup, &ProfilePopup::NewRequested, this,
            [this] { ProfileDialog::Run(this, mStore, mStore.MakeNew(ProfileStore::DefaultUsername()), true); });
    connect(popup, &ProfilePopup::EditRequested, this, [this] {
        if (const Profile* profile = mStore.Selected())
            ProfileDialog::Run(this, mStore, *profile, false);
    });
    connect(popup, &ProfilePopup::ManageRequested, this, [this] { ShowPage(ProfilesPage); });
    popup->ShowAbove(mProfileButton);
}

void MainWindow::RefreshProfiles()
{
    if (const Profile* profile = mStore.Selected())
    {
        mProfileButton->setText(profile->username);
        mProfileButton->SetSubtitle(profile->name);
        mProfileButton->SetIcon(Skins::FaceFor(*profile, kSlotIconSize), true);
        mProfileButton->setToolTip(QString("Playing as %1 with the profile %2").arg(profile->username, profile->name));
        mLog->SetLogsFolder(ProfileStore::GameDirFor(*profile) + "/logs");
    }
    RefreshStatus();
}

void MainWindow::SetStatus(const QString& text, const QColor& colour)
{
    mStatus->SetText(text);
    mStatus->SetColour(colour);
    mStatus->setToolTip(text);
}

void MainWindow::RefreshStatus()
{
    const Profile* profile = mStore.Selected();
    const QString who = profile != nullptr ? QString("Play as %1").arg(profile->username) : QString();
    mTabs->SetMarked(LogPage, mRunner->IsRunning());
    if (mRunner->IsRunning())
    {
        mPlay->setEnabled(false);
        mPlay->SetLabels("Playing", "The game is running");
        SetStatus("The game is running. Its output is in the Game Log.", GameColours::kText);
        return;
    }
    const QString problem = GameRunner::CheckGameReady();
    if (!problem.isEmpty())
    {
        mPlay->setEnabled(false);
        mPlay->SetLabels("Play", problem);
        SetStatus(problem, GameColours::kWarning);
        return;
    }
    mPlay->setEnabled(true);
    mPlay->SetLabels("Play", who);
    SetStatus("Ready to play", GameColours::kReady);
}

void MainWindow::Play()
{
    const QString problem = GameRunner::CheckGameReady();
    if (!problem.isEmpty())
    {
        mLog->AddLauncherMessage(LogLevel::Warn, problem);
        RefreshStatus();
        return;
    }
    const Profile* profile = mStore.Selected();
    if (profile == nullptr)
        return;

    mLog->AddLauncherMessage(LogLevel::Info, QString("Starting the game as %1 with the profile %2.")
                                                 .arg(profile->username, profile->name));
    QString error;
    // An AppImage's files go away when the launcher exits, so there the launcher waits hidden for the game instead.
    if (profile->afterLaunch == AfterLaunch::Close && !LauncherPaths::RunsFromAppImage())
    {
        if (!mRunner->StartDetached(*profile, &error))
        {
            mLog->AddLauncherMessage(LogLevel::Error, error);
            QMessageBox::warning(this, "Play", error);
            return;
        }
        close();
        return;
    }
    ShowPage(LogPage);
    if (!mRunner->Start(*profile, &error))
    {
        mLog->AddLauncherMessage(LogLevel::Error, error);
        QMessageBox::warning(this, "Play", error);
        RefreshStatus();
    }
}

void MainWindow::OnGameStarted()
{
    RefreshStatus();
    const Profile* profile = mStore.Selected();
    if (profile != nullptr && (profile->afterLaunch == AfterLaunch::Hide || profile->afterLaunch == AfterLaunch::Close))
    {
        mHiddenForGame = true;
        mCloseAfterGame = profile->afterLaunch == AfterLaunch::Close;
        hide();
    }
}

void MainWindow::OnGameFinished(int exitCode, bool crashed)
{
    if (crashed || exitCode != 0)
        mLog->AddLauncherMessage(LogLevel::Error, crashed ? QString("The game crashed.")
                                                          : QString("The game exited with code %1.").arg(exitCode));
    else
        mLog->AddLauncherMessage(LogLevel::Info, "The game closed.");
    if (mCloseAfterGame && !crashed && exitCode == 0)
    {
        QApplication::quit();
        return;
    }
    mCloseAfterGame = false;
    if (mHiddenForGame)
    {
        mHiddenForGame = false;
        show();
        raise();
        activateWindow();
    }
    if (crashed || exitCode != 0)
        ShowPage(LogPage);
    mStore.NotifySkinsChanged();
}

void MainWindow::OnImported()
{
    GameArt::Reload();
    QApplication::setFont(GameFont(16));
    for (QWidget* widget : QApplication::allWidgets())
    {
        QEvent fontChange(QEvent::ApplicationFontChange);
        QCoreApplication::sendEvent(widget, &fontChange);
        widget->updateGeometry();
    }
    mPanorama->Reload();
    mNews->Refresh();
    mStore.NotifySkinsChanged();
    mRoot->setCurrentIndex(1);
    QSettings(LauncherPaths::SettingsFile(), QSettings::IniFormat).setValue("notices/packsFolder", true);
    update();
}

void MainWindow::ShowPacksNotice()
{
    QSettings(LauncherPaths::SettingsFile(), QSettings::IniFormat).setValue("notices/packsFolder", true);
    QMessageBox notice(QMessageBox::Information, "Packs",
                       "DLC and skin packs no longer load from the game files. The packs that came free with the "
                       "game still load on their own, but packs you bought need adding again.\n\n"
                       "Copy the folders of the packs you own from content/WiiU/DLC in your update 688 dump into "
                       "the packs folder, and they load the next time you play.",
                       QMessageBox::NoButton, this);
    QPushButton* open = notice.addButton("Open Packs Folder", QMessageBox::ActionRole);
    notice.addButton(QMessageBox::Ok);
    notice.exec();
    const Profile* profile = mStore.Selected();
    if (notice.clickedButton() != open || profile == nullptr)
        return;
    const QString folder = ProfileStore::GameDirFor(*profile) + "/packs";
    QDir().mkpath(folder);
    QDesktopServices::openUrl(QUrl::fromLocalFile(folder));
}

void MainWindow::SaveProfiles()
{
    QString error;
    if (!mStore.Save(&error))
        mLog->AddLauncherMessage(LogLevel::Error, error);
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (mRunner->IsRunning())
    {
        const auto answer = QMessageBox::question(
            this, "Close the Launcher", "The game is still running. Closing the launcher closes the game too. Close both?");
        if (answer != QMessageBox::Yes)
        {
            event->ignore();
            return;
        }
        mRunner->Stop();
    }
    if (mSetup->IsBusy())
    {
        const auto answer = QMessageBox::question(
            this, "Close the Launcher", "The game files are still being imported. Stop and close the launcher?");
        if (answer != QMessageBox::Yes)
        {
            event->ignore();
            return;
        }
    }
    QSettings settings(LauncherPaths::SettingsFile(), QSettings::IniFormat);
    settings.setValue("window/geometry", saveGeometry());
    settings.setValue("window/page", mPages->currentIndex());
    mLog->SaveSettings();
    SaveProfiles();
    event->accept();
}
