#ifndef LAUNCHER_GAMELOG_HPP
#define LAUNCHER_GAMELOG_HPP

#include <QAbstractTableModel>
#include <QByteArray>
#include <QSortFilterProxyModel>
#include <QString>
#include <QVector>
#include <QWidget>

#include <deque>

class QCheckBox;
class QLineEdit;
class QTableView;
class QTimer;

enum class LogLevel
{
    Trace,
    Debug,
    Info,
    Warn,
    Error,
    Fatal
};

struct LogEntry
{
    QString time;
    QString thread;
    LogLevel level = LogLevel::Info;
    QString source;
    QString message;
};

class LogParser
{
public:
    explicit LogParser(LogLevel unformattedLevel) : mUnformattedLevel(unformattedLevel) {}

    QVector<LogEntry> Feed(const QByteArray& data);
    QVector<LogEntry> Finish();

private:
    LogEntry Parse(const QByteArray& line) const;

    LogLevel mUnformattedLevel;
    QByteArray mPending;
};

class LogModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column
    {
        TimeColumn,
        LevelColumn,
        ThreadColumn,
        SourceColumn,
        MessageColumn,
        ColumnCount
    };
    static const int kLevelRole = Qt::UserRole + 1;

    using QAbstractTableModel::QAbstractTableModel;

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    void Append(const QVector<LogEntry>& entries);
    void Clear();
    QString LineText(int row) const;

private:
    std::deque<LogEntry> mEntries;
};

class LogFilter : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    using QSortFilterProxyModel::QSortFilterProxyModel;

    void SetLevelShown(LogLevel level, bool shown);
    bool LevelShown(LogLevel level) const { return (mLevels & (1u << static_cast<int>(level))) != 0; }
    void SetText(const QString& text);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private:
    unsigned int mLevels = 0x3e;
    QString mText;
};

class GameLogTab : public QWidget
{
    Q_OBJECT

public:
    explicit GameLogTab(QWidget* parent = nullptr);

    void SetLogsFolder(const QString& folder);
    void Add(const QVector<LogEntry>& entries);
    void AddLauncherMessage(LogLevel level, const QString& message);
    void SaveSettings() const;

private:
    void Flush();
    void CopySelection() const;

    LogModel* mModel;
    LogFilter* mFilter;
    QTableView* mView;
    QLineEdit* mSearch;
    QVector<QCheckBox*> mLevelBoxes;
    QVector<LogEntry> mPending;
    QTimer* mFlushTimer;
    QString mLogsFolder;
};

QString LogLevelName(LogLevel level);

#endif
