#include "ProfilesTab.hpp"

#include <QDesktopServices>
#include <QDir>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QUrl>
#include <QVBoxLayout>

#include "Icons.hpp"
#include "ProfileDialog.hpp"
#include "Profiles.hpp"
#include "Skins.hpp"
#include "Widgets.hpp"

ProfilesTab::ProfilesTab(ProfileStore& store, QWidget* parent)
    : QWidget(parent), mStore(store), mTable(new QTableWidget(this)), mDelete(new IconButton(Icons::Bin(), "Delete", this))
{
    mTable->setColumnCount(3);
    mTable->setHorizontalHeaderLabels({"Profile", "Username", "Game folder"});
    mTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    mTable->setSelectionMode(QAbstractItemView::SingleSelection);
    mTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    mTable->setShowGrid(false);
    mTable->verticalHeader()->hide();
    mTable->verticalHeader()->setDefaultSectionSize(52);
    mTable->setObjectName("ProfileList");
    mTable->setFocusPolicy(Qt::NoFocus);
    mTable->setIconSize(QSize(32, 32));
    mTable->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    mTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    mTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);

    auto* newButton = new IconButton(Icons::Plus(), "New profile", this);
    auto* editButton = new IconButton(Icons::Pencil(), "Edit", this);
    auto* openButton = new IconButton(Icons::Folder(), "Open game folder", this);
    openButton->setObjectName("RailLink");
    auto* buttons = new QHBoxLayout;
    buttons->addWidget(newButton);
    buttons->addWidget(editButton);
    buttons->addWidget(mDelete);
    buttons->addStretch(1);
    buttons->addWidget(openButton);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);
    layout->addLayout(buttons);
    layout->addWidget(mTable, 1);

    connect(newButton, &QPushButton::clicked, this, &ProfilesTab::NewProfile);
    connect(editButton, &QPushButton::clicked, this, &ProfilesTab::EditProfile);
    connect(mDelete, &QPushButton::clicked, this, &ProfilesTab::DeleteProfile);
    connect(openButton, &QPushButton::clicked, this, &ProfilesTab::OpenGameFolder);
    connect(mTable, &QTableWidget::cellDoubleClicked, this, &ProfilesTab::EditProfile);
    connect(mTable, &QTableWidget::itemSelectionChanged, this, [this] {
        if (!mReloading && !CurrentId().isEmpty())
            mStore.Select(CurrentId());
    });
    connect(&mStore, &ProfileStore::Changed, this, &ProfilesTab::Reload);
    Reload();
}

void ProfilesTab::Reload()
{
    mReloading = true;
    const QVector<Profile>& profiles = mStore.Profiles();
    mTable->setRowCount(profiles.size());
    for (int row = 0; row < profiles.size(); ++row)
    {
        const Profile& profile = profiles[row];
        auto* name = new QTableWidgetItem(profile.name);
        name->setData(Qt::UserRole, profile.id);
        const QImage face = Skins::FaceFor(profile);
        if (!face.isNull())
            name->setIcon(QIcon(QPixmap::fromImage(face.scaled(64, 64, Qt::IgnoreAspectRatio, Qt::SmoothTransformation))));
        mTable->setItem(row, 0, name);
        mTable->setItem(row, 1, new QTableWidgetItem(profile.username));
        mTable->setItem(row, 2, new QTableWidgetItem(QDir::toNativeSeparators(ProfileStore::GameDirFor(profile))));
        if (profile.id == mStore.SelectedId())
            mTable->selectRow(row);
    }
    mDelete->setEnabled(profiles.size() > 1);
    mReloading = false;
}

QString ProfilesTab::CurrentId() const
{
    const QList<QTableWidgetItem*> selected = mTable->selectedItems();
    if (selected.isEmpty())
        return QString();
    QTableWidgetItem* name = mTable->item(selected.first()->row(), 0);
    return name != nullptr ? name->data(Qt::UserRole).toString() : QString();
}

void ProfilesTab::NewProfile()
{
    ProfileDialog::Run(this, mStore, mStore.MakeNew(ProfileStore::DefaultUsername()), true);
}

void ProfilesTab::EditProfile()
{
    const Profile* profile = mStore.Find(CurrentId());
    if (profile != nullptr)
        ProfileDialog::Run(this, mStore, *profile, false);
}

void ProfilesTab::DeleteProfile()
{
    const Profile* profile = mStore.Find(CurrentId());
    if (profile == nullptr || mStore.Profiles().size() <= 1)
        return;
    const QString question = QString("Delete the profile %1? Its game folder and saves are kept.").arg(profile->name);
    if (QMessageBox::question(this, "Delete Profile", question) != QMessageBox::Yes)
        return;
    mStore.Remove(profile->id);
    QString error;
    if (!mStore.Save(&error))
        QMessageBox::warning(this, "Profiles", error);
}

void ProfilesTab::OpenGameFolder()
{
    const Profile* profile = mStore.Find(CurrentId());
    if (profile == nullptr)
        return;
    const QString folder = ProfileStore::GameDirFor(*profile);
    QDir().mkpath(folder);
    QDesktopServices::openUrl(QUrl::fromLocalFile(folder));
}
