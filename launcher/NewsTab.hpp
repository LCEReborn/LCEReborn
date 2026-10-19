#ifndef LAUNCHER_NEWSTAB_HPP
#define LAUNCHER_NEWSTAB_HPP

#include <QByteArray>
#include <QWidget>

class QLabel;
class QListWidget;
class QNetworkAccessManager;
class QNetworkReply;
class QTextBrowser;

class NewsTab : public QWidget
{
    Q_OBJECT

public:
    explicit NewsTab(QWidget* parent = nullptr);

    void Refresh();

signals:
    void UpdateChecked(const QString& newerName, const QString& newerUrl);

private:
    void OnReply(QNetworkReply* reply);
    bool ShowReleases(const QByteArray& json, const QString& note);
    void ShowOffline(const QString& note);
    void ShowSelected();

    QNetworkAccessManager* mNetwork;
    QListWidget* mReleases;
    QLabel* mTitle;
    QLabel* mDate;
    QTextBrowser* mBody;
    QLabel* mNote;
};

#endif
