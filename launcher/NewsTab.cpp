#include "NewsTab.hpp"

#include <QDateTime>
#include <QDesktopServices>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QListWidget>
#include <QLocale>
#include <QMenu>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPushButton>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTextBlock>
#include <QTextBrowser>
#include <QTextCursor>
#include <QUrl>
#include <QVBoxLayout>

#include "BlockIcons.hpp"
#include "BuildVersion.hpp"
#include "LauncherPaths.hpp"
#include "UpdateCheck.hpp"
#include "Widgets.hpp"

namespace
{
const char* const kReleasesUrl = "https://api.github.com/repos/LCEReborn/LCEReborn/releases?per_page=10";
const char* const kRepositoryUrl = "https://github.com/LCEReborn/LCEReborn";
const int kTagRole = Qt::UserRole + 1;
const int kTitleRole = Qt::UserRole + 2;
const int kUrlRole = Qt::UserRole + 3;
const int kPublishedRole = Qt::UserRole + 4;
const int kBodyRole = Qt::UserRole + 5;
const int kPrereleaseRole = Qt::UserRole + 6;
const int kBlockSize = 40;

QString CommitOf(const QString& tag, const QString& title)
{
    static const QRegularExpression buildTag("^build-([0-9a-fA-F]{7,40})$");
    static const QRegularExpression leadingCommit("^([0-9a-fA-F]{8})\\b");
    QRegularExpressionMatch match = buildTag.match(tag);
    if (match.hasMatch())
        return match.captured(1).left(8).toLower();
    match = leadingCommit.match(title);
    if (match.hasMatch())
        return match.captured(1).toLower();
    return QString();
}

QString HowLongAgo(const QDateTime& published)
{
    const qint64 days = published.toLocalTime().date().daysTo(QDate::currentDate());
    if (days <= 0)
        return "Today";
    if (days == 1)
        return "Yesterday";
    if (days < 31)
        return QString("%1 days ago").arg(days);
    return QLocale().toString(published.toLocalTime().date(), "d MMMM yyyy");
}

void UseGameText(QTextDocument* document)
{
    for (QTextBlock block = document->begin(); block.isValid(); block = block.next())
    {
        QTextCursor cursor(block);
        cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
        QTextCharFormat format;
        format.setFontWeight(QFont::Normal);
        format.setProperty(QTextFormat::FontSizeAdjustment, 0);
        format.setProperty(QTextFormat::FontPixelSize, block.blockFormat().headingLevel() > 0 ? 20 : 18);
        cursor.mergeCharFormat(format);
        if (block.blockFormat().headingLevel() > 0)
        {
            QTextBlockFormat spacing = block.blockFormat();
            spacing.setTopMargin(10);
            spacing.setBottomMargin(4);
            cursor.setBlockFormat(spacing);
        }
    }
}
}

NewsTab::NewsTab(QWidget* parent)
    : QWidget(parent), mNetwork(new QNetworkAccessManager(this)), mReleases(new QListWidget(this)),
      mTitle(new QLabel(this)), mDate(new QLabel(this)), mBody(new QTextBrowser(this)), mNote(new QLabel(this))
{
    mReleases->setObjectName("GameRows");
    mReleases->setItemDelegate(new GameRowDelegate(mReleases));
    mReleases->setFrameShape(QFrame::NoFrame);
    mReleases->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    mReleases->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    mReleases->setMouseTracking(true);
    mReleases->setUniformItemSizes(true);
    mNote->setObjectName("Muted");
    mNote->setWordWrap(true);
    mNote->hide();

    auto* refresh = new QPushButton("Refresh", this);
    auto* links = new QPushButton("Links", this);
    auto* linkMenu = new QMenu(links);
    const QString repository = kRepositoryUrl;
    const struct
    {
        const char* label;
        QString url;
    } entries[] = {
        {"GitHub", repository},
        {"All releases", repository + "/releases"},
        {"Report a bug", repository + "/issues"},
        {"Documentation", repository + "/tree/main/docs"},
    };
    for (const auto& entry : entries)
    {
        const QUrl url(entry.url);
        linkMenu->addAction(entry.label, this, [url] { QDesktopServices::openUrl(url); });
    }
    links->setMenu(linkMenu);
    connect(refresh, &QPushButton::clicked, this, &NewsTab::Refresh);

    auto* listWell = new GameWell(this);
    listWell->setFixedWidth(400);
    auto* listLayout = new QVBoxLayout(listWell);
    listLayout->setContentsMargins(10, 10, 10, 10);
    listLayout->setSpacing(10);
    listLayout->addWidget(mReleases, 1);
    listLayout->addWidget(mNote);
    auto* buttons = new QHBoxLayout;
    buttons->setSpacing(8);
    buttons->addWidget(refresh, 1);
    buttons->addWidget(links, 1);
    listLayout->addLayout(buttons);

    mTitle->setObjectName("NewsTitle");
    mTitle->setTextFormat(Qt::RichText);
    mTitle->setOpenExternalLinks(true);
    mTitle->setWordWrap(true);
    mDate->setObjectName("NewsDate");
    mBody->setObjectName("NewsBody");
    mBody->setOpenExternalLinks(true);
    mBody->setFrameShape(QFrame::NoFrame);
    mBody->document()->setDocumentMargin(0);

    auto* notesWell = new GameWell(this);
    auto* notesLayout = new QVBoxLayout(notesWell);
    notesLayout->setContentsMargins(22, 18, 14, 14);
    notesLayout->setSpacing(6);
    notesLayout->addWidget(mTitle);
    notesLayout->addWidget(mDate);
    notesLayout->addSpacing(8);
    notesLayout->addWidget(mBody, 1);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);
    layout->addWidget(listWell);
    layout->addWidget(notesWell, 1);

    connect(mReleases, &QListWidget::currentRowChanged, this, &NewsTab::ShowSelected);
    connect(mNetwork, &QNetworkAccessManager::finished, this, &NewsTab::OnReply);
    QMetaObject::invokeMethod(this, &NewsTab::Refresh, Qt::QueuedConnection);
}

void NewsTab::Refresh()
{
    QFile cache(LauncherPaths::NewsCacheFile());
    if (!cache.open(QIODevice::ReadOnly) || !ShowReleases(cache.readAll(), QString()))
        ShowOffline(QString());

    QNetworkRequest request{QUrl(kReleasesUrl)};
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("User-Agent", "LCEReborn-Launcher");
    request.setTransferTimeout(10000);
    mNetwork->get(request);
}

void NewsTab::OnReply(QNetworkReply* reply)
{
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError)
    {
        const QString reason = QString("Couldn't reach GitHub (%1).").arg(reply->errorString());
        QFile cache(LauncherPaths::NewsCacheFile());
        const QString saved = QLocale().toString(QFileInfo(cache).lastModified(), QLocale::ShortFormat);
        if (!cache.open(QIODevice::ReadOnly) ||
            !ShowReleases(cache.readAll(), reason + " Showing the news saved " + saved + "."))
            ShowOffline(reason);
        return;
    }
    const QByteArray json = reply->readAll();
    if (!ShowReleases(json, QString()))
    {
        ShowOffline(QString());
        return;
    }
    QSaveFile cache(LauncherPaths::NewsCacheFile());
    if (cache.open(QIODevice::WriteOnly))
    {
        cache.write(json);
        cache.commit();
    }
}

bool NewsTab::ShowReleases(const QByteArray& json, const QString& note)
{
    const QJsonDocument document = QJsonDocument::fromJson(json);
    if (!document.isArray() || document.array().isEmpty())
        return false;

    const QString selectedTag = mReleases->currentItem() != nullptr
                                    ? mReleases->currentItem()->data(kTagRole).toString()
                                    : QString();
    const QString installed = BuildCommit().left(8).toLower();
    mReleases->blockSignals(true);
    mReleases->clear();
    int selectRow = 0;
    for (const QJsonValue& value : document.array())
    {
        const QJsonObject release = value.toObject();
        if (release.value("draft").toBool())
            continue;
        const QString tag = release.value("tag_name").toString();
        QString title = release.value("name").toString().trimmed();
        if (title.isEmpty())
            title = tag;
        const QDateTime published = QDateTime::fromString(release.value("published_at").toString(), Qt::ISODate);
        const QString commit = CommitOf(tag, title);
        const bool prerelease = release.value("prerelease").toBool();

        const QString label = commit.isEmpty() || !published.isValid()
                                  ? title
                                  : QString("%1 - %2").arg(commit, QLocale().toString(published.toLocalTime().date(), "d MMM"));
        auto* item = new QListWidgetItem(label, mReleases);
        item->setData(Qt::DecorationRole, BlockIcons::Render(commit.isEmpty() ? tag : commit, kBlockSize));
        QStringList notes;
        if (prerelease)
            notes.append("Pre-release");
        if (!commit.isEmpty() && commit == installed)
            notes.append("This build");
        item->setData(GameRowDelegate::kSubtitleRole, notes.join(", "));
        item->setData(kTagRole, tag);
        item->setData(kTitleRole, title);
        item->setData(kUrlRole, release.value("html_url").toString());
        item->setData(kPublishedRole, published);
        item->setData(kBodyRole, release.value("body").toString());
        item->setData(kPrereleaseRole, prerelease);
        item->setToolTip(title);
        if (tag == selectedTag)
            selectRow = mReleases->count() - 1;
    }
    mReleases->blockSignals(false);
    if (mReleases->count() == 0)
        return false;
    mReleases->setCurrentRow(selectRow);
    ShowSelected();

    mNote->setText(note);
    mNote->setVisible(!note.isEmpty());
    const NewerRelease newer = FindNewerRelease(document.array(), BuildCommit(), BuildDate());
    emit UpdateChecked(newer.name, newer.url);
    return true;
}

void NewsTab::ShowSelected()
{
    const QListWidgetItem* item = mReleases->currentItem();
    if (item == nullptr)
        return;
    static const QRegularExpression images("!\\[[^\\]]*\\]\\([^)]*\\)");
    const QString url = item->data(kUrlRole).toString();
    const QString title = item->data(kTitleRole).toString().toHtmlEscaped();
    mTitle->setText(url.isEmpty() ? title
                                  : QString("<a href='%1' style='color:#404040; text-decoration:none'>%2</a>")
                                        .arg(url.toHtmlEscaped(), title));
    mTitle->setToolTip(url.isEmpty() ? QString() : "Open the release on GitHub");

    const QDateTime published = item->data(kPublishedRole).toDateTime();
    QString when = published.isValid() ? HowLongAgo(published) : QString();
    if (item->data(kPrereleaseRole).toBool())
        when += when.isEmpty() ? "Pre-release" : ", pre-release";
    mDate->setText(when);
    mDate->setVisible(!when.isEmpty());

    QString body = item->data(kBodyRole).toString();
    body.remove(images);
    body = body.trimmed();
    mBody->setMarkdown(body.isEmpty() ? QString("No notes for this release.") : body);
    UseGameText(mBody->document());
}

void NewsTab::ShowOffline(const QString& note)
{
    mReleases->clear();
    mTitle->setText("No news yet");
    mDate->setText("The releases appear here once the launcher reaches GitHub.");
    mDate->show();
    mBody->clear();
    mNote->setText(note);
    mNote->setVisible(!note.isEmpty());
    emit UpdateChecked(QString(), QString());
}
