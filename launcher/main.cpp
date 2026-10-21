#include <QApplication>
#include <QDir>
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
    QApplication::setApplicationVersion(BuildVersion());
    QApplication::setWindowIcon(QIcon(":/launcher/icon.png"));
    ApplyTheme(application);

    QString error;
    if (!QDir().mkpath(LauncherPaths::DataDir()))
    {
        QMessageBox::critical(nullptr, "LCEReborn Launcher",
                              QString("The launcher's folder %1 could not be created.")
                                  .arg(QDir::toNativeSeparators(LauncherPaths::DataDir())));
        return 1;
    }
    ProfileStore store(LauncherPaths::ProfilesFile());
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
