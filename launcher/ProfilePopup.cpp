#include "ProfilePopup.hpp"

#include <QHBoxLayout>
#include <QPushButton>
#include <QVBoxLayout>

#include "Profiles.hpp"
#include "Skins.hpp"
#include "Widgets.hpp"

ProfilePopup::ProfilePopup(const ProfileStore& store, QWidget* parent) : QFrame(parent, Qt::Popup)
{
    setAttribute(Qt::WA_DeleteOnClose);

    auto* panel = new GamePanel(this);
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(panel);

    auto* layout = new QVBoxLayout(panel);
    layout->setSpacing(6);
    for (const Profile& profile : store.Profiles())
    {
        auto* row = new GameButton(profile.username, panel);
        row->SetPixelSize(20);
        row->SetSubtitle(profile.name);
        row->SetIcon(Skins::FaceFor(profile, kSlotIconSize), true);
        row->SetPrimary(profile.id == store.SelectedId());
        row->setFixedHeight(62);
        row->setAccessibleName(profile.name);
        const QString id = profile.id;
        connect(row, &GameButton::clicked, this, [this, id] {
            emit ProfileChosen(id);
            close();
        });
        layout->addWidget(row);
    }
    layout->addSpacing(6);

    auto* actions = new QHBoxLayout;
    actions->setSpacing(6);
    const struct
    {
        const char* label;
        const char* tip;
        void (ProfilePopup::*signal)();
    } buttons[] = {
        {"New", "New profile", &ProfilePopup::NewRequested},
        {"Edit", "Edit this profile", &ProfilePopup::EditRequested},
        {"Manage", "Manage profiles", &ProfilePopup::ManageRequested},
    };
    for (const auto& entry : buttons)
    {
        auto* button = new QPushButton(entry.label, panel);
        button->setToolTip(entry.tip);
        button->setAccessibleName(entry.tip);
        button->setCursor(Qt::PointingHandCursor);
        const auto signal = entry.signal;
        connect(button, &QPushButton::clicked, this, [this, signal] {
            close();
            emit(this->*signal)();
        });
        actions->addWidget(button, 1);
    }
    layout->addLayout(actions);
}

void ProfilePopup::ShowAbove(const QWidget* anchor)
{
    setFixedWidth(anchor->width());
    adjustSize();
    move(anchor->mapToGlobal(QPoint(0, -height() - 8)));
    show();
}
