#ifndef LAUNCHER_NEWSTAB_HPP
#define LAUNCHER_NEWSTAB_HPP

#include <QByteArray>
#include <QWidget>

class QLabel;
class QNetworkAccessManager;
class QNetworkReply;
class QTextBrowser;

class NewsTab : public QWidget
{
    Q_OBJECT

public:
    explicit NewsTab(QWidget* parent = nullptr);

    void Refresh();

private:
    void OnReply(QNetworkReply* reply);
    bool ShowReleases(const QByteArray& json, const QString& note);
    void ShowOffline(const QString& note);

    QNetworkAccessManager* mNetwork;
    QTextBrowser* mBrowser;
    QLabel* mNote;
};

#endif
