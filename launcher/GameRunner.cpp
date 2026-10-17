#include "GameRunner.hpp"

#include <QDir>
#include <QFileInfo>

#include "LauncherPaths.hpp"

GameRunner::GameRunner(QObject* parent) : QObject(parent), mOutput(LogLevel::Info), mErrors(LogLevel::Warn)
{
    mProcess.setProcessChannelMode(QProcess::SeparateChannels);
    connect(&mProcess, &QProcess::readyReadStandardOutput, this,
            [this] { emit Output(mOutput.Feed(mProcess.readAllStandardOutput())); });
    connect(&mProcess, &QProcess::readyReadStandardError, this,
            [this] { emit Output(mErrors.Feed(mProcess.readAllStandardError())); });
    connect(&mProcess, &QProcess::started, this, &GameRunner::Started);
    connect(&mProcess, &QProcess::finished, this, [this](int exitCode, QProcess::ExitStatus status) {
        emit Output(mOutput.Feed(mProcess.readAllStandardOutput()) + mOutput.Finish());
        emit Output(mErrors.Feed(mProcess.readAllStandardError()) + mErrors.Finish());
        emit Finished(exitCode, status == QProcess::CrashExit);
    });
}

GameRunner::~GameRunner()
{
    disconnect(&mProcess, nullptr, this, nullptr);
    Stop();
}

QString GameRunner::CheckGameReady()
{
    if (!QFileInfo(LauncherPaths::GameExecutable()).isFile())
        return QString("The game executable is missing: %1")
            .arg(QDir::toNativeSeparators(LauncherPaths::GameExecutable()));
    if (!QFileInfo(LauncherPaths::ContentCheckFile()).isFile())
        return QString("The game files aren't imported yet.");
    if (!QFileInfo(LauncherPaths::ResourcesDir() + "/ui").isDir())
        return QString("The launcher's resources folder is missing: %1")
            .arg(QDir::toNativeSeparators(LauncherPaths::ResourcesDir()));
    return QString();
}

QStringList GameRunner::Arguments(const Profile& profile)
{
    const QString gameDir = ProfileStore::GameDirFor(profile);
    QStringList arguments;
    arguments << "--username" << profile.username << "--uuid" << profile.uuid << "--content-dir"
              << LauncherPaths::AssetsDir() << "--resource-dir" << LauncherPaths::ResourcesDir() << "--save-dir"
              << gameDir + "/saves" << "--log-dir" << gameDir + "/logs";
    if (!profile.language.isEmpty())
        arguments << "--language" << profile.language;
    return arguments;
}

bool GameRunner::Prepare(const Profile& profile, QString* error)
{
    if (IsRunning())
    {
        *error = "The game is already running.";
        return false;
    }
    const QString problem = CheckGameReady();
    if (!problem.isEmpty())
    {
        *error = problem;
        return false;
    }
    const QString gameDir = ProfileStore::GameDirFor(profile);
    if (!QDir().mkpath(gameDir))
    {
        *error = QString("The game folder %1 could not be created.").arg(QDir::toNativeSeparators(gameDir));
        return false;
    }
    return true;
}

bool GameRunner::Start(const Profile& profile, QString* error)
{
    if (!Prepare(profile, error))
        return false;
    mOutput = LogParser(LogLevel::Info);
    mErrors = LogParser(LogLevel::Warn);
    mProcess.setProgram(LauncherPaths::GameExecutable());
    mProcess.setArguments(Arguments(profile));
    mProcess.setWorkingDirectory(ProfileStore::GameDirFor(profile));
    mProcess.start();
    if (!mProcess.waitForStarted(10000))
    {
        *error = QString("The game could not be started: %1").arg(mProcess.errorString());
        return false;
    }
    return true;
}

bool GameRunner::StartDetached(const Profile& profile, QString* error)
{
    if (!Prepare(profile, error))
        return false;
    if (!QProcess::startDetached(LauncherPaths::GameExecutable(), Arguments(profile), ProfileStore::GameDirFor(profile)))
    {
        *error = "The game could not be started.";
        return false;
    }
    return true;
}

void GameRunner::Stop()
{
    if (!IsRunning())
        return;
    mProcess.terminate();
    if (!mProcess.waitForFinished(3000))
    {
        mProcess.kill();
        mProcess.waitForFinished(3000);
    }
}
