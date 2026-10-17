#ifndef LAUNCHER_LAUNCHERPATHS_HPP
#define LAUNCHER_LAUNCHERPATHS_HPP

#include <QString>

namespace LauncherPaths
{
QString DataDir();
QString AssetsDir();
QString ResourcesDir();
QString GameExecutable();
QString ProfilesFile();
QString SettingsFile();
QString NewsCacheFile();
QString PreparedManifest();
QString ContentCheckFile();
}

#endif
