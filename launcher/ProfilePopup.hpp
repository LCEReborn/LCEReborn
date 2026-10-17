#ifndef LAUNCHER_PROFILEPOPUP_HPP
#define LAUNCHER_PROFILEPOPUP_HPP

#include <QAbstractButton>
#include <QFrame>
#include <QPixmap>

class ProfileStore;

class ProfileRow : public QAbstractButton
{
    Q_OBJECT

public:
    ProfileRow(const QPixmap& face, const QString& name, const QString& username, bool selected,
               QWidget* parent = nullptr);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    QPixmap mFace;
    QString mUsername;
    bool mSelected;
    bool mHover = false;
};

class ProfilePopup : public QFrame
{
    Q_OBJECT

public:
    ProfilePopup(const ProfileStore& store, QWidget* parent = nullptr);

    void ShowAbove(const QWidget* anchor);

signals:
    void ProfileChosen(const QString& id);
    void NewRequested();
    void EditRequested();
    void ManageRequested();
};

#endif
