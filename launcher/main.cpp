#include <QApplication>
#include <QIcon>
#include <QMessageBox>

#include "BuildVersion.hpp"
#include "LauncherPaths.hpp"
#include "MainWindow.hpp"
#include "Profiles.hpp"
#include "Theme.hpp"

int main(int argc, char** argv)
{
    QApplication application(argc, argv);
    QApplication::setApplicationName("LCEReborn Launcher");
    QApplication::setDesktopFileName("org.lcereborn.LCERebornLauncher");
    QApplication::setApplicationVersion(BuildVersion());
    QApplication::setWindowIcon(QIcon(":/launcher/icon.png"));
    ApplyTheme(application);

    ProfileStore store(LauncherPaths::ProfilesFile());
    QString error;
    if (!store.Load(&error))
    {
        QMessageBox::critical(nullptr, "LCEReborn Launcher",
                              error + "\n\nFix or remove the file and start the launcher again.");
        return 1;
    }
    if (!store.Save(&error))
        QMessageBox::warning(nullptr, "LCEReborn Launcher", error);

    MainWindow window(store);
    window.show();
    return application.exec();
}
