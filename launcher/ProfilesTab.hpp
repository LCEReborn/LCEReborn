#ifndef LAUNCHER_PROFILESTAB_HPP
#define LAUNCHER_PROFILESTAB_HPP

#include <QWidget>

class ProfileStore;
class QPushButton;
class QTableWidget;

class ProfilesTab : public QWidget
{
    Q_OBJECT

public:
    ProfilesTab(ProfileStore& store, QWidget* parent = nullptr);

private:
    void Reload();
    QString CurrentId() const;
    void NewProfile();
    void EditProfile();
    void DeleteProfile();
    void OpenGameFolder();

    ProfileStore& mStore;
    QTableWidget* mTable;
    QPushButton* mDelete;
    bool mReloading = false;
};

#endif
