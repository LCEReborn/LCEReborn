#include "MainWindow.hpp"

#include <QApplication>
#include <QButtonGroup>
#include <QCloseEvent>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QStackedWidget>
#include <QStyle>
#include <QVBoxLayout>

#include "BuildVersion.hpp"
#include "GameRunner.hpp"
#include "Icons.hpp"
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
}

MainWindow::MainWindow(ProfileStore& store, QWidget* parent)
    : QMainWindow(parent), mStore(store), mRunner(new GameRunner(this)), mPages(new QStackedWidget(this)),
      mRoot(new QStackedWidget(this)), mSetup(new SetupPage(this)), mNews(new NewsTab(this)), mLog(new GameLogTab(this)),
      mProfileChip(new ProfileChip(this)), mPlay(new PlayButton(this)), mStatus(new QLabel(this))
{
    setWindowTitle(QString("LCEReborn Launcher  ·  %1").arg(BuildVersion()));
    resize(1080, 680);
    setMinimumSize(880, 560);

    mPages->addWidget(mNews);
    mPages->addWidget(mLog);
    mPages->addWidget(new ProfilesTab(mStore, this));

    auto* topBar = new QFrame(this);
    topBar->setObjectName("TopBar");
    auto* topLayout = new QHBoxLayout(topBar);
    topLayout->setContentsMargins(22, 0, 22, 0);
    topLayout->setSpacing(0);
    auto* mark = new QLabel(topBar);
    mark->setPixmap(QPixmap(":/launcher/mark.png").scaled(38, 38, Qt::KeepAspectRatio, Qt::FastTransformation));
    mark->setFixedSize(38, 38);
    auto* wordmark = new QLabel("LCE REBORN", topBar);
    wordmark->setObjectName("Wordmark");
    topLayout->addWidget(mark);
    topLayout->addSpacing(12);
    topLayout->addWidget(wordmark);
    topLayout->addSpacing(44);

    auto* navGroup = new QButtonGroup(this);
    navGroup->setExclusive(true);
    for (int index = 0; index < PageCount; ++index)
    {
        if (index > 0)
        {
            auto* separator = new QFrame(topBar);
            separator->setObjectName("NavSeparator");
            separator->setFixedSize(1, 22);
            topLayout->addWidget(separator);
        }
        auto* item = new NavItem(kPageNames[index], topBar);
        navGroup->addButton(item, index);
        topLayout->addWidget(item);
        mNav.append(item);
    }
    connect(navGroup, &QButtonGroup::idClicked, this, &MainWindow::ShowPage);
    topLayout->addStretch(1);
    auto* reimport = new QPushButton("Re-import game files", topBar);
    reimport->setObjectName("RailLink");
    reimport->setCursor(Qt::PointingHandCursor);
    topLayout->addWidget(reimport);

    auto* bottomBar = new QFrame(this);
    bottomBar->setObjectName("BottomBar");
    auto* bottomLayout = new QHBoxLayout(bottomBar);
    bottomLayout->setContentsMargins(22, 14, 22, 16);
    bottomLayout->setSpacing(18);
    mProfileChip->setFixedWidth(280);
    mStatus->setObjectName("Status");
    mStatus->setWordWrap(true);
    mPlay->setFixedWidth(300);
    auto* build = new QLabel(BuildVersion().toUpper(), bottomBar);
    build->setObjectName("Tagline");
    auto* statusColumn = new QVBoxLayout;
    statusColumn->setSpacing(4);
    statusColumn->addStretch(1);
    statusColumn->addWidget(mStatus);
    statusColumn->addWidget(build);
    statusColumn->addStretch(1);
    bottomLayout->addWidget(mProfileChip);
    bottomLayout->addLayout(statusColumn, 1);
    bottomLayout->addWidget(mPlay);

    auto* content = new QVBoxLayout;
    content->setContentsMargins(28, 22, 28, 18);
    content->addWidget(mPages, 1);

    auto* shell = new QWidget(this);
    auto* layout = new QVBoxLayout(shell);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(topBar);
    layout->addLayout(content, 1);
    layout->addWidget(bottomBar);

    mRoot->addWidget(mSetup);
    mRoot->addWidget(shell);
    auto* backdrop = new Backdrop(this);
    auto* backdropLayout = new QVBoxLayout(backdrop);
    backdropLayout->setContentsMargins(0, 0, 0, 0);
    backdropLayout->addWidget(mRoot);
    setCentralWidget(backdrop);

    connect(&mStore, &ProfileStore::Changed, this, &MainWindow::RefreshProfiles);
    connect(mProfileChip, &ProfileChip::clicked, this, &MainWindow::ShowProfileMenu);
    connect(mPlay, &PlayButton::clicked, this, &MainWindow::Play);
    connect(mRunner, &GameRunner::Output, mLog, &GameLogTab::Add);
    connect(mRunner, &GameRunner::Started, this, &MainWindow::OnGameStarted);
    connect(mRunner, &GameRunner::Finished, this, &MainWindow::OnGameFinished);
    connect(mSetup, &SetupPage::Message, mLog, &GameLogTab::AddLauncherMessage);
    connect(mSetup, &SetupPage::Imported, this, [this] {
        mStore.NotifySkinsChanged();
        mRoot->setCurrentIndex(1);
    });
    connect(mSetup, &SetupPage::BackRequested, this, [this] { mRoot->setCurrentIndex(1); });
    connect(reimport, &QPushButton::clicked, this, [this] {
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
    mRoot->setCurrentIndex(QFileInfo(LauncherPaths::ContentCheckFile()).isFile() ? 1 : 0);

    RefreshProfiles();
}

void MainWindow::ShowPage(int index)
{
    mPages->setCurrentIndex(index);
    mNav[index]->setChecked(true);
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
    popup->ShowAbove(mProfileChip);
}

void MainWindow::RefreshProfiles()
{
    if (const Profile* profile = mStore.Selected())
    {
        const QImage face = Skins::Face(Skins::SkinFor(*profile));
        mProfileChip->SetProfile(face.isNull() ? QPixmap() : QPixmap::fromImage(face), profile->username,
                                 profile->name);
        mLog->SetLogsFolder(ProfileStore::GameDirFor(*profile) + "/logs");
    }
    RefreshStatus();
}

void MainWindow::SetStatus(const QString& text, const char* style)
{
    mStatus->setText(text);
    mStatus->setObjectName(style);
    mStatus->style()->unpolish(mStatus);
    mStatus->style()->polish(mStatus);
}

void MainWindow::RefreshStatus()
{
    const Profile* profile = mStore.Selected();
    const QString who = profile != nullptr ? QString("as %1").arg(profile->username) : QString();
    mNav[LogPage]->SetBadge(mRunner->IsRunning() ? "LIVE" : QString());
    if (mRunner->IsRunning())
    {
        mPlay->setEnabled(false);
        mPlay->SetLabels("Playing", who, false);
        SetStatus("The game is running. Its output is in the Game Log.", "Status");
        return;
    }
    const QString problem = GameRunner::CheckGameReady();
    if (!problem.isEmpty())
    {
        mPlay->setEnabled(false);
        mPlay->SetLabels("Play", "Not ready", false);
        SetStatus(problem, "StatusWarning");
        return;
    }
    mPlay->setEnabled(true);
    mPlay->SetLabels("Play", who, false);
    SetStatus("Ready to play", "StatusReady");
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
    if (profile->afterLaunch == AfterLaunch::Close)
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
    if (profile != nullptr && profile->afterLaunch == AfterLaunch::Hide)
    {
        mHiddenForGame = true;
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
