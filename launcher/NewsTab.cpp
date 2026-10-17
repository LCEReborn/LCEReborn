#include "NewsTab.hpp"

#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLocale>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPushButton>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTextBrowser>
#include <QVBoxLayout>

#include "LauncherPaths.hpp"

namespace
{
const char* const kReleasesUrl = "https://api.github.com/repos/LCEReborn/LCEReborn/releases?per_page=10";
const char* const kRepositoryUrl = "https://github.com/LCEReborn/LCEReborn";

QString Links()
{
    const QString repository = kRepositoryUrl;
    return QString("<p style='line-height:170%'><a href='%1'>GitHub</a><br>"
                   "<a href='%1/releases'>Releases</a><br>"
                   "<a href='%1/issues'>Report a bug</a><br>"
                   "<a href='%1/tree/main/docs'>Documentation</a></p>")
        .arg(repository);
}
}

NewsTab::NewsTab(QWidget* parent)
    : QWidget(parent), mNetwork(new QNetworkAccessManager(this)), mBrowser(new QTextBrowser(this)),
      mNote(new QLabel(this))
{
    mBrowser->setObjectName("NewsBody");
    mBrowser->setOpenExternalLinks(true);
    mBrowser->document()->setDocumentMargin(4);
    mNote->setObjectName("Muted");
    mNote->setWordWrap(true);

    auto* side = new QVBoxLayout;
    side->setSpacing(8);
    auto* heading = new QLabel("LINKS", this);
    heading->setObjectName("Tagline");
    auto* links = new QLabel(Links(), this);
    links->setOpenExternalLinks(true);
    links->setTextFormat(Qt::RichText);
    auto* refresh = new QPushButton("Refresh", this);
    refresh->setObjectName("RailLink");
    refresh->setCursor(Qt::PointingHandCursor);
    side->addWidget(heading);
    side->addWidget(links);
    side->addStretch(1);
    side->addWidget(refresh, 0, Qt::AlignLeft);
    connect(refresh, &QPushButton::clicked, this, &NewsTab::Refresh);

    auto* left = new QVBoxLayout;
    left->addWidget(mBrowser, 1);
    left->addWidget(mNote);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(40);
    layout->addLayout(left, 1);
    auto* sideWidget = new QWidget(this);
    sideWidget->setLayout(side);
    sideWidget->setFixedWidth(170);
    layout->addWidget(sideWidget);

    connect(mNetwork, &QNetworkAccessManager::finished, this, &NewsTab::OnReply);
    Refresh();
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
        const QString saved =
            QLocale().toString(QFileInfo(cache).lastModified(), QLocale::ShortFormat);
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

    static const QRegularExpression images("!\\[[^\\]]*\\]\\([^)]*\\)");
    QString markdown;
    for (const QJsonValue& value : document.array())
    {
        const QJsonObject release = value.toObject();
        if (release.value("draft").toBool())
            continue;
        QString title = release.value("name").toString().trimmed();
        if (title.isEmpty())
            title = release.value("tag_name").toString();
        const QDateTime published = QDateTime::fromString(release.value("published_at").toString(), Qt::ISODate);
        QString body = release.value("body").toString();
        body.remove(images);
        markdown += QString("## [%1](%2)\n\n*%3%4*\n\n%5\n\n---\n\n")
                        .arg(title, release.value("html_url").toString(),
                             QLocale().toString(published.toLocalTime().date(), QLocale::LongFormat),
                             release.value("prerelease").toBool() ? " · pre-release" : "", body.trimmed());
    }
    mBrowser->setMarkdown(markdown);
    mNote->setText(note);
    mNote->setVisible(!note.isEmpty());
    return true;
}

void NewsTab::ShowOffline(const QString& note)
{
    mBrowser->setHtml("<p style='font-size:22pt; font-weight:700; color:#5f5850'>No news yet</p>");
    mNote->setText(note);
    mNote->setVisible(!note.isEmpty());
}
