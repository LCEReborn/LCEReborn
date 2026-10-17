#ifndef LAUNCHER_MAINWINDOW_HPP
#define LAUNCHER_MAINWINDOW_HPP

#include <QMainWindow>
#include <QVector>

#include "GameLog.hpp"

class GameLogTab;
class GameRunner;
class NavItem;
class NewsTab;
class PlayButton;
class ProfileChip;
class ProfileStore;
class QLabel;
class QStackedWidget;
class SetupPage;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(ProfileStore& store, QWidget* parent = nullptr);

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void ShowPage(int index);
    void ShowProfileMenu();
    void RefreshProfiles();
    void RefreshStatus();
    void Play();
    void OnGameStarted();
    void OnGameFinished(int exitCode, bool crashed);
    void SaveProfiles();
    void SetStatus(const QString& text, const char* style);

    ProfileStore& mStore;
    GameRunner* mRunner;
    QStackedWidget* mPages;
    QStackedWidget* mRoot;
    SetupPage* mSetup;
    NewsTab* mNews;
    GameLogTab* mLog;
    QVector<NavItem*> mNav;
    ProfileChip* mProfileChip;
    PlayButton* mPlay;
    QLabel* mStatus;
    bool mHiddenForGame = false;
};

#endif
