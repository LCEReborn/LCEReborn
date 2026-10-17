#include "LauncherPaths.hpp"

#include <QCoreApplication>
#include <QDir>

namespace LauncherPaths
{
QString DataDir()
{
    return QDir(QCoreApplication::applicationDirPath()).absolutePath();
}

QString AssetsDir()
{
    return DataDir() + "/assets";
}

QString ResourcesDir()
{
    return DataDir() + "/resources";
}

QString GameExecutable()
{
#if defined(Q_OS_WIN)
    return DataDir() + "/LCEReborn.exe";
#else
    return DataDir() + "/LCEReborn";
#endif
}

QString ProfilesFile()
{
    return DataDir() + "/launcher_profiles.json";
}

QString SettingsFile()
{
    return DataDir() + "/launcher.ini";
}

QString NewsCacheFile()
{
    return DataDir() + "/launcher_news.json";
}

QString PreparedManifest()
{
    return AssetsDir() + "/.prepared.json";
}

QString ContentCheckFile()
{
    return AssetsDir() + "/Common/Media/MediaWiiU.arc";
}
}
