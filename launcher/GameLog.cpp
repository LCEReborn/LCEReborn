#include "GameLog.hpp"

#include <QApplication>
#include <QClipboard>
#include <QColor>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeySequence>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollBar>
#include <QSettings>
#include <QShortcut>
#include <QTableView>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>

#include "LauncherPaths.hpp"

namespace
{
const int kMaxEntries = 200000;
const int kTrimBatch = 20000;
const char* const kLevelNames[] = {"TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"};

QColor LevelColour(LogLevel level)
{
    switch (level)
    {
    case LogLevel::Trace:
        return QColor(110, 114, 120);
    case LogLevel::Debug:
        return QColor(140, 144, 150);
    case LogLevel::Warn:
        return QColor(224, 176, 80);
    case LogLevel::Error:
        return QColor(239, 106, 94);
    case LogLevel::Fatal:
        return QColor(255, 80, 80);
    default:
        return QColor(225, 225, 225);
    }
}
}

QString LogLevelName(LogLevel level)
{
    return kLevelNames[static_cast<int>(level)];
}

QVector<LogEntry> LogParser::Feed(const QByteArray& data)
{
    mPending += data;
    QVector<LogEntry> entries;
    int start = 0;
    for (;;)
    {
        const int end = mPending.indexOf('\n', start);
        if (end < 0)
            break;
        QByteArray line = mPending.mid(start, end - start);
        if (line.endsWith('\r'))
            line.chop(1);
        entries.append(Parse(line));
        start = end + 1;
    }
    mPending.remove(0, start);
    return entries;
}

QVector<LogEntry> LogParser::Finish()
{
    QVector<LogEntry> entries;
    if (!mPending.isEmpty())
        entries.append(Parse(mPending));
    mPending.clear();
    return entries;
}

LogEntry LogParser::Parse(const QByteArray& line) const
{
    static const QRegularExpression pattern(
        "^\\[(\\d\\d:\\d\\d:\\d\\d\\.\\d{3})\\] \\[(.*)/(TRACE|DEBUG|INFO|WARN|ERROR|FATAL)\\] \\[([^\\]]*)\\]: (.*)$");
    const QString text = QString::fromUtf8(line);
    LogEntry entry;
    const QRegularExpressionMatch match = pattern.match(text);
    if (match.hasMatch())
    {
        entry.time = match.captured(1);
        entry.thread = match.captured(2);
        const QString level = match.captured(3);
        for (int index = 0; index < 6; ++index)
        {
            if (level == QLatin1String(kLevelNames[index]))
                entry.level = static_cast<LogLevel>(index);
        }
        entry.source = match.captured(4);
        entry.message = match.captured(5);
        return entry;
    }
    entry.time = QTime::currentTime().toString("HH:mm:ss.zzz");
    entry.level = mUnformattedLevel;
    entry.source = "Game";
    entry.message = text;
    return entry;
}

int LogModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(mEntries.size());
}

int LogModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant LogModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= static_cast<int>(mEntries.size()))
        return QVariant();
    const LogEntry& entry = mEntries[static_cast<std::size_t>(index.row())];
    if (role == kLevelRole)
        return static_cast<int>(entry.level);
    if (role == Qt::ForegroundRole)
        return LevelColour(entry.level);
    if (role == Qt::ToolTipRole && index.column() == MessageColumn)
        return entry.message;
    if (role != Qt::DisplayRole)
        return QVariant();
    switch (index.column())
    {
    case TimeColumn:
        return entry.time;
    case LevelColumn:
        return LogLevelName(entry.level);
    case ThreadColumn:
        return entry.thread;
    case SourceColumn:
        return entry.source;
    default:
        return entry.message;
    }
}

QVariant LogModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QVariant();
    static const char* const names[] = {"Time", "Level", "Thread", "Source", "Message"};
    return section >= 0 && section < ColumnCount ? QString(names[section]) : QVariant();
}

void LogModel::Append(const QVector<LogEntry>& entries)
{
    if (entries.isEmpty())
        return;
    const int existing = static_cast<int>(mEntries.size());
    const int incoming = static_cast<int>(entries.size());
    if (existing > 0 && existing + incoming > kMaxEntries)
    {
        const int remove = std::min(existing, std::max(kTrimBatch, existing + incoming - kMaxEntries));
        beginRemoveRows(QModelIndex(), 0, remove - 1);
        mEntries.erase(mEntries.begin(), mEntries.begin() + remove);
        endRemoveRows();
    }
    const int first = static_cast<int>(mEntries.size());
    beginInsertRows(QModelIndex(), first, first + incoming - 1);
    mEntries.insert(mEntries.end(), entries.begin(), entries.end());
    endInsertRows();
}

void LogModel::Clear()
{
    beginResetModel();
    mEntries.clear();
    endResetModel();
}

QString LogModel::LineText(int row) const
{
    const LogEntry& entry = mEntries[static_cast<std::size_t>(row)];
    return QString("[%1] [%2/%3] [%4]: %5")
        .arg(entry.time, entry.thread, LogLevelName(entry.level), entry.source, entry.message);
}

void LogFilter::SetLevelShown(LogLevel level, bool shown)
{
    const unsigned int bit = 1u << static_cast<int>(level);
    beginFilterChange();
    mLevels = shown ? (mLevels | bit) : (mLevels & ~bit);
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
}

void LogFilter::SetText(const QString& text)
{
    beginFilterChange();
    mText = text;
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
}

bool LogFilter::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    const QModelIndex level = sourceModel()->index(sourceRow, LogModel::LevelColumn, sourceParent);
    if (!LevelShown(static_cast<LogLevel>(level.data(LogModel::kLevelRole).toInt())))
        return false;
    if (mText.isEmpty())
        return true;
    for (int column : {LogModel::MessageColumn, LogModel::SourceColumn, LogModel::ThreadColumn})
    {
        if (sourceModel()->index(sourceRow, column, sourceParent).data().toString().contains(mText, Qt::CaseInsensitive))
            return true;
    }
    return false;
}

GameLogTab::GameLogTab(QWidget* parent)
    : QWidget(parent), mModel(new LogModel(this)), mFilter(new LogFilter(this)), mView(new QTableView(this)),
      mSearch(new QLineEdit(this)), mFlushTimer(new QTimer(this))
{
    mFilter->setSourceModel(mModel);
    mFilter->setDynamicSortFilter(true);

    QSettings settings(LauncherPaths::SettingsFile(), QSettings::IniFormat);
    auto* toolbar = new QHBoxLayout;
    for (int index = 0; index < 6; ++index)
    {
        const LogLevel level = static_cast<LogLevel>(index);
        auto* box = new QPushButton(LogLevelName(level), this);
        box->setCheckable(true);
        box->setCursor(Qt::PointingHandCursor);
        const bool shown = settings.value(QString("log/show%1").arg(LogLevelName(level)), mFilter->LevelShown(level)).toBool();
        box->setChecked(shown);
        mFilter->SetLevelShown(level, shown);
        const QColor colour = LevelColour(level);
        box->setStyleSheet(QString("QPushButton { background: transparent; border: 1px solid rgba(255,255,255,30); "
                                   "border-radius: 11px; padding: 3px 11px; color: #6f675f; font-weight: 700; "
                                   "font-size: 8.5pt; } "
                                   "QPushButton:checked { background: rgba(%1,%2,%3,40); border-color: %4; color: %4; }")
                               .arg(colour.red())
                               .arg(colour.green())
                               .arg(colour.blue())
                               .arg(colour.name()));
        connect(box, &QPushButton::toggled, this, [this, level](bool checked) { mFilter->SetLevelShown(level, checked); });
        toolbar->addWidget(box);
        mLevelBoxes.append(box);
    }
    toolbar->addSpacing(16);
    mSearch->setPlaceholderText("Search the log");
    mSearch->setClearButtonEnabled(true);
    connect(mSearch, &QLineEdit::textChanged, mFilter, &LogFilter::SetText);
    toolbar->addWidget(mSearch, 1);

    auto* clear = new QPushButton("Clear", this);
    clear->setObjectName("RailLink");
    connect(clear, &QPushButton::clicked, mModel, &LogModel::Clear);
    toolbar->addWidget(clear);
    auto* openFolder = new QPushButton("Open logs folder", this);
    openFolder->setObjectName("RailLink");
    connect(openFolder, &QPushButton::clicked, this, [this] {
        QDir().mkpath(mLogsFolder);
        QDesktopServices::openUrl(QUrl::fromLocalFile(mLogsFolder));
    });
    toolbar->addWidget(openFolder);

    mView->setObjectName("LogView");
    mView->setModel(mFilter);
    mView->setSelectionBehavior(QAbstractItemView::SelectRows);
    mView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    mView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    mView->setWordWrap(false);
    mView->setShowGrid(false);
    mView->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    mView->verticalHeader()->hide();
    mView->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    mView->verticalHeader()->setDefaultSectionSize(mView->fontMetrics().height() + 6);
    QFont mono("Consolas");
    mono.setStyleHint(QFont::Monospace);
    mView->setFont(mono);
    QHeaderView* header = mView->horizontalHeader();
    header->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    header->setSectionResizeMode(QHeaderView::Interactive);
    header->setStretchLastSection(true);
    header->resizeSection(LogModel::TimeColumn, 100);
    header->resizeSection(LogModel::LevelColumn, 64);
    header->resizeSection(LogModel::ThreadColumn, 150);
    header->resizeSection(LogModel::SourceColumn, 110);

    auto* copy = new QShortcut(QKeySequence::Copy, mView);
    connect(copy, &QShortcut::activated, this, &GameLogTab::CopySelection);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);
    layout->addLayout(toolbar);
    layout->addWidget(mView, 1);

    mFlushTimer->setInterval(100);
    connect(mFlushTimer, &QTimer::timeout, this, &GameLogTab::Flush);
    mFlushTimer->start();
}

void GameLogTab::SetLogsFolder(const QString& folder)
{
    mLogsFolder = folder;
}

void GameLogTab::Add(const QVector<LogEntry>& entries)
{
    mPending += entries;
}

void GameLogTab::AddLauncherMessage(LogLevel level, const QString& message)
{
    LogEntry entry;
    entry.time = QTime::currentTime().toString("HH:mm:ss.zzz");
    entry.thread = "Launcher";
    entry.level = level;
    entry.source = "Launcher";
    entry.message = message;
    mPending.append(entry);
}

void GameLogTab::Flush()
{
    if (mPending.isEmpty())
        return;
    QScrollBar* scroll = mView->verticalScrollBar();
    const bool atBottom = scroll->value() >= scroll->maximum() - 2;
    mModel->Append(mPending);
    mPending.clear();
    if (atBottom)
        mView->scrollToBottom();
}

void GameLogTab::CopySelection() const
{
    QModelIndexList rows = mView->selectionModel()->selectedRows();
    std::sort(rows.begin(), rows.end(), [](const QModelIndex& a, const QModelIndex& b) { return a.row() < b.row(); });
    QStringList lines;
    for (const QModelIndex& row : rows)
        lines.append(mModel->LineText(mFilter->mapToSource(row).row()));
    if (!lines.isEmpty())
        QApplication::clipboard()->setText(lines.join('\n'));
}

void GameLogTab::SaveSettings() const
{
    QSettings settings(LauncherPaths::SettingsFile(), QSettings::IniFormat);
    for (int index = 0; index < mLevelBoxes.size(); ++index)
        settings.setValue(QString("log/show%1").arg(LogLevelName(static_cast<LogLevel>(index))),
                          mLevelBoxes[index]->isChecked());
}
