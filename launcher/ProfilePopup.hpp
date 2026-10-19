#ifndef LAUNCHER_PROFILEPOPUP_HPP
#define LAUNCHER_PROFILEPOPUP_HPP

#include <QFrame>

class ProfileStore;

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
