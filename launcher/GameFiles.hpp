#ifndef LAUNCHER_GAMEFILES_HPP
#define LAUNCHER_GAMEFILES_HPP

#include <QObject>
#include <QString>

#include <atomic>

#include "GameLog.hpp"

class AssetPreparer : public QObject
{
    Q_OBJECT

public:
    AssetPreparer(const QString& baseDump, const QString& updateDump, const QString& assetsDir);

    void Cancel() { mCancelled = true; }

    static bool CheckDump(const QString& path, bool isUpdate, int* version, QString* error);

public slots:
    void Run();

signals:
    void Progress(qint64 filesDone, qint64 filesTotal, qint64 bytesDone, qint64 bytesTotal);
    void Message(LogLevel level, const QString& message);
    void Finished(bool succeeded, const QString& error);

private:
    bool Prepare(QString* error);
    bool CopyFile(const QString& source, const QString& target, qint64* bytesDone, qint64 bytesTotal,
                  qint64 filesDone, qint64 filesTotal, QString* error);

    QString mBaseDump;
    QString mUpdateDump;
    QString mAssetsDir;
    std::atomic<bool> mCancelled;
    qint64 mLastProgress = 0;
};

#endif
