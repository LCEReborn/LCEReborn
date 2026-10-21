#ifndef LAUNCHER_LAUNCHERPATHS_HPP
#define LAUNCHER_LAUNCHERPATHS_HPP

#include <QString>

namespace LauncherPaths
{
QString InstallDir();
QString DataDir();
QString AssetsDir();
QString ResourcesDir();
QString LicenseFile();
QString GameExecutable();
QString ProfilesFile();
QString SettingsFile();
QString NewsCacheFile();
QString PreparedManifest();
QString ContentCheckFile();
bool RunsFromAppImage();
}

#endif
