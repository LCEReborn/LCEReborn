#ifndef LAUNCHER_MAINWINDOW_HPP
#define LAUNCHER_MAINWINDOW_HPP

#include <QMainWindow>

#include "GameLog.hpp"

class GameButton;
class GameLogTab;
class GameRunner;
class NewsTab;
class Panorama;
class PlayButton;
class ProfileStore;
class QBoxLayout;
class QStackedWidget;
class SetupPage;
class ShadowLabel;
class TabbedPanel;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(ProfileStore& store, QWidget* parent = nullptr);

protected:
    void closeEvent(QCloseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void ShowPage(int index);
    void ShowProfileMenu();
    void RefreshProfiles();
    void RefreshStatus();
    void Play();
    void OnGameStarted();
    void OnGameFinished(int exitCode, bool crashed);
    void OnImported();
    void SaveProfiles();
    void SetStatus(const QString& text, const QColor& colour);

    ProfileStore& mStore;
    GameRunner* mRunner;
    Panorama* mPanorama;
    QStackedWidget* mRoot;
    QBoxLayout* mShellLayout;
    TabbedPanel* mTabs;
    QStackedWidget* mPages;
    SetupPage* mSetup;
    NewsTab* mNews;
    GameLogTab* mLog;
    GameButton* mProfileButton;
    PlayButton* mPlay;
    ShadowLabel* mStatus;
    bool mHiddenForGame = false;
    bool mCloseAfterGame = false;
};

#endif
