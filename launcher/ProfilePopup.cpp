#include "ProfilePopup.hpp"

#include <QEnterEvent>
#include <QHBoxLayout>
#include <QPainter>
#include <QPainterPath>
#include <QToolButton>
#include <QVBoxLayout>

#include "Icons.hpp"
#include "Profiles.hpp"
#include "Skins.hpp"
#include "Widgets.hpp"

namespace
{
const int kFaceSize = 34;

QToolButton* ActionButton(const QIcon& icon, const QString& label, const QString& tip, QWidget* parent)
{
    auto* button = new QToolButton(parent);
    button->setObjectName("PopupAction");
    button->setIcon(icon);
    button->setText(label);
    button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    button->setIconSize(QSize(16, 16));
    button->setToolTip(tip);
    button->setAccessibleName(tip);
    button->setAutoRaise(true);
    button->setCursor(Qt::PointingHandCursor);
    button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    button->setFixedHeight(46);
    return button;
}
}

ProfileRow::ProfileRow(const QPixmap& face, const QString& name, const QString& username, bool selected,
                       QWidget* parent)
    : QAbstractButton(parent), mFace(face), mUsername(username), mSelected(selected)
{
    setText(name);
    setAccessibleName(name);
    setCursor(Qt::PointingHandCursor);
}

QSize ProfileRow::sizeHint() const
{
    return QSize(220, 48);
}

void ProfileRow::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    if (mHover)
    {
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(255, 255, 255, 12));
        painter.drawRoundedRect(QRectF(rect()), 5, 5);
    }

    const QRect face(8, (height() - kFaceSize) / 2, kFaceSize, kFaceSize);
    if (!mFace.isNull())
    {
        painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
        painter.drawPixmap(face, mFace);
    }
    else
    {
        painter.fillRect(face, QColor(255, 255, 255, 16));
    }
    if (mSelected)
    {
        painter.fillRect(face, QColor(0, 0, 0, 150));
        QPainterPath check;
        check.moveTo(face.left() + kFaceSize * 0.26, face.top() + kFaceSize * 0.52);
        check.lineTo(face.left() + kFaceSize * 0.43, face.top() + kFaceSize * 0.68);
        check.lineTo(face.left() + kFaceSize * 0.75, face.top() + kFaceSize * 0.34);
        painter.setPen(QPen(QColor(255, 255, 255), 2.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(check);
    }

    const int left = face.right() + 12;
    painter.setPen(mSelected ? QColor(246, 194, 124) : QColor(236, 230, 221));
    painter.setFont(DisplayFont(12, QFont::Bold));
    painter.drawText(QRect(left, 6, width() - left - 8, 20), Qt::AlignLeft | Qt::AlignVCenter,
                     painter.fontMetrics().elidedText(text(), Qt::ElideRight, width() - left - 8));
    painter.setPen(QColor(154, 145, 136));
    painter.setFont(DisplayFont(9, QFont::Medium));
    painter.drawText(QRect(left, 26, width() - left - 8, 16), Qt::AlignLeft | Qt::AlignVCenter,
                     painter.fontMetrics().elidedText(mUsername, Qt::ElideRight, width() - left - 8));
}

void ProfileRow::enterEvent(QEnterEvent*)
{
    mHover = true;
    update();
}

void ProfileRow::leaveEvent(QEvent*)
{
    mHover = false;
    update();
}

ProfilePopup::ProfilePopup(const ProfileStore& store, QWidget* parent) : QFrame(parent, Qt::Popup)
{
    setObjectName("ProfilePopup");
    setAttribute(Qt::WA_DeleteOnClose);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(2);
    for (const Profile& profile : store.Profiles())
    {
        const QImage face = Skins::Face(Skins::SkinFor(profile));
        auto* row = new ProfileRow(face.isNull() ? QPixmap() : QPixmap::fromImage(face), profile.name, profile.username,
                                   profile.id == store.SelectedId(), this);
        const QString id = profile.id;
        connect(row, &ProfileRow::clicked, this, [this, id] {
            emit ProfileChosen(id);
            close();
        });
        layout->addWidget(row);
    }

    auto* separator = new QFrame(this);
    separator->setObjectName("PopupSeparator");
    separator->setFixedHeight(1);
    layout->addSpacing(4);
    layout->addWidget(separator);
    layout->addSpacing(4);

    auto* actions = new QHBoxLayout;
    actions->setSpacing(4);
    const struct
    {
        QIcon icon;
        const char* label;
        const char* tip;
        void (ProfilePopup::*signal)();
    } buttons[] = {
        {Icons::Plus(), "New", "New profile", &ProfilePopup::NewRequested},
        {Icons::Pencil(), "Edit", "Edit this profile", &ProfilePopup::EditRequested},
        {Icons::List(), "Manage", "Manage profiles", &ProfilePopup::ManageRequested},
    };
    for (const auto& entry : buttons)
    {
        QToolButton* button = ActionButton(entry.icon, entry.label, entry.tip, this);
        const auto signal = entry.signal;
        connect(button, &QToolButton::clicked, this, [this, signal] {
            close();
            emit(this->*signal)();
        });
        actions->addWidget(button);
    }
    layout->addLayout(actions);
}

void ProfilePopup::ShowAbove(const QWidget* anchor)
{
    setFixedWidth(anchor->width());
    adjustSize();
    move(anchor->mapToGlobal(QPoint(0, -height() - 6)));
    show();
}
