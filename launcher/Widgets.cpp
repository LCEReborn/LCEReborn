#include "Widgets.hpp"

#include <QEnterEvent>
#include <QFontMetrics>
#include <QFontDatabase>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QRandomGenerator>
#include <QStyleOptionButton>
#include <QStylePainter>

#include <cmath>


namespace
{
const QColor kAccent(232, 165, 75);
const QColor kAccentDeep(176, 112, 40);
const QColor kText(236, 230, 221);
const QColor kMuted(154, 145, 136);

QFont Display(int pointSize, int weight)
{
    return DisplayFont(pointSize, weight);
}
}

QFont DisplayFont(int pointSize, int weight)
{
    QFont font(DisplayFamily());
    font.setPointSize(pointSize);
    font.setWeight(static_cast<QFont::Weight>(weight));
    return font;
}

QString DisplayFamily()
{
    static const QString family = [] {
        const QStringList families = QFontDatabase::families();
        for (const char* candidate : {"Bahnschrift", "DIN Alternate", "Roboto Condensed", "Arial Narrow"})
        {
            if (families.contains(candidate))
                return QString(candidate);
        }
        return QFont().family();
    }();
    return family;
}

IconButton::IconButton(const QIcon& icon, const QString& text, QWidget* parent) : QPushButton(icon, text, parent)
{
}

void IconButton::paintEvent(QPaintEvent*)
{
    QStylePainter painter(this);
    QStyleOptionButton option;
    initStyleOption(&option);
    const QString label = option.text;
    const QIcon icon = option.icon;
    option.text.clear();
    option.icon = QIcon();
    painter.drawControl(QStyle::CE_PushButton, option);

    const QFontMetricsF metrics(font());
    const QSize iconSize = icon.isNull() ? QSize() : option.iconSize;
    const qreal spacing = icon.isNull() || label.isEmpty() ? 0.0 : 6.0;
    const qreal inset = icon.isNull() ? 0.0 : 2.0;
    const qreal inkWidth = icon.isNull() ? 0.0 : iconSize.width() - inset * 2.0;
    const qreal x = (width() - inkWidth - spacing - metrics.horizontalAdvance(label)) / 2.0;
    const int baseline = qRound(height() / 2.0 + metrics.capHeight() / 2.0);

    if (!icon.isNull())
    {
        const QIcon::Mode mode = !isEnabled() ? QIcon::Disabled : (underMouse() ? QIcon::Active : QIcon::Normal);
        const QPixmap pixmap = icon.pixmap(iconSize, devicePixelRatio(), mode);
        const qreal top = baseline - metrics.capHeight() / 2.0 - iconSize.height() / 2.0;
        painter.drawPixmap(QPointF(qRound(x - inset), std::floor(top)), pixmap);
    }

    painter.setFont(font());
    painter.setPen(palette().color(isEnabled() ? QPalette::Active : QPalette::Disabled, QPalette::ButtonText));
    painter.drawText(QPointF(qRound(x + inkWidth + spacing), baseline), label);
}

Backdrop::Backdrop(QWidget* parent) : QWidget(parent), mMark(":/launcher/mark.png")
{
    QImage grain(160, 160, QImage::Format_ARGB32);
    QRandomGenerator random(20261017);
    for (int y = 0; y < grain.height(); ++y)
    {
        for (int x = 0; x < grain.width(); ++x)
        {
            const int shade = 23 + static_cast<int>(random.bounded(5));
            grain.setPixelColor(x, y, QColor(shade + 2, shade, shade - 2));
        }
    }
    mGrain = QPixmap::fromImage(grain);
}

void Backdrop::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.drawTiledPixmap(rect(), mGrain);

    if (!mMark.isNull())
    {
        const int size = qMax(320, height() * 3 / 4);
        painter.setOpacity(0.05);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
        painter.drawPixmap(width() - size * 2 / 3, height() - size * 2 / 3, size, size, mMark);
        painter.setOpacity(1.0);
    }
}

NavItem::NavItem(const QString& text, QWidget* parent) : QAbstractButton(parent)
{
    setText(text);
    setCheckable(true);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::TabFocus);
}

QSize NavItem::sizeHint() const
{
    const QFontMetrics label(Display(14, QFont::Bold));
    int width = 22 + label.horizontalAdvance(text()) + 22;
    if (!mBadge.isEmpty())
        width += QFontMetrics(Display(9, QFont::Bold)).horizontalAdvance(mBadge) + 22;
    return QSize(width, 64);
}

void NavItem::SetBadge(const QString& badge)
{
    mBadge = badge;
    updateGeometry();
    update();
}

void NavItem::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    if (isChecked())
        painter.fillRect(QRect(12, height() - 3, width() - 24, 3), kAccent);

    painter.setFont(Display(14, isChecked() ? QFont::Bold : QFont::Medium));
    painter.setPen(isChecked() || mHover ? kText : QColor(186, 178, 168));
    const int labelWidth = painter.fontMetrics().horizontalAdvance(text());
    painter.drawText(QRect(22, 0, labelWidth + 2, height()), Qt::AlignVCenter | Qt::AlignLeft, text());

    if (!mBadge.isEmpty())
    {
        painter.setFont(Display(9, QFont::Bold));
        const int badgeWidth = painter.fontMetrics().horizontalAdvance(mBadge) + 12;
        const QRect badge(22 + labelWidth + 8, height() / 2 - 9, badgeWidth, 18);
        painter.setPen(Qt::NoPen);
        painter.setBrush(kAccent);
        painter.drawRoundedRect(badge, 3, 3);
        painter.setPen(QColor(40, 26, 10));
        painter.drawText(badge, Qt::AlignCenter, mBadge);
    }
}

void NavItem::enterEvent(QEnterEvent*)
{
    mHover = true;
    update();
}

void NavItem::leaveEvent(QEvent*)
{
    mHover = false;
    update();
}

ProfileChip::ProfileChip(QWidget* parent) : QAbstractButton(parent)
{
    setCursor(Qt::PointingHandCursor);
}

QSize ProfileChip::sizeHint() const
{
    return QSize(240, 64);
}

void ProfileChip::SetProfile(const QPixmap& face, const QString& username, const QString& profileName)
{
    mFace = face;
    mUsername = username;
    mProfileName = profileName;
    update();
}

void ProfileChip::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QRectF frame = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    painter.setPen(QColor(255, 255, 255, mHover ? 40 : 22));
    painter.setBrush(QColor(255, 255, 255, mHover ? 10 : 4));
    painter.drawRoundedRect(frame, 6, 6);

    int textLeft = 16;
    if (!mFace.isNull())
    {
        const QRect face(10, (height() - 44) / 2, 44, 44);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
        painter.drawPixmap(face, mFace);
        textLeft = 66;
    }

    painter.setFont(Display(13, QFont::Bold));
    painter.setPen(kText);
    painter.drawText(QRect(textLeft, 10, width() - textLeft - 30, 24), Qt::AlignLeft | Qt::AlignVCenter, mUsername);
    painter.setFont(Display(9, QFont::Medium));
    painter.setPen(kMuted);
    painter.drawText(QRect(textLeft, 32, width() - textLeft - 30, 20), Qt::AlignLeft | Qt::AlignVCenter,
                     QString("PROFILE  ·  %1").arg(mProfileName.toUpper()));

    painter.setPen(QPen(kMuted, 1.6));
    const QPointF centre(width() - 18, height() / 2.0);
    painter.drawLine(centre + QPointF(-4, -2), centre + QPointF(0, 2));
    painter.drawLine(centre + QPointF(0, 2), centre + QPointF(4, -2));
}

void ProfileChip::enterEvent(QEnterEvent*)
{
    mHover = true;
    update();
}

void ProfileChip::leaveEvent(QEvent*)
{
    mHover = false;
    update();
}

PlayButton::PlayButton(QWidget* parent) : QAbstractButton(parent)
{
    setCursor(Qt::PointingHandCursor);
}

QSize PlayButton::sizeHint() const
{
    return QSize(240, 78);
}

void PlayButton::SetLabels(const QString& title, const QString& detail, bool attention)
{
    setText(title);
    mDetail = detail;
    mAttention = attention;
    update();
}

void PlayButton::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    QColor face = mAttention ? QColor(84, 136, 196) : kAccent;
    QColor edge = mAttention ? QColor(46, 82, 128) : kAccentDeep.darker(130);
    if (!isEnabled())
    {
        face = QColor(66, 60, 54);
        edge = QColor(44, 40, 36);
    }
    else if (isDown())
    {
        face = face.darker(110);
    }
    else if (mHover)
    {
        face = face.lighter(108);
    }

    const QRectF slab = QRectF(rect()).adjusted(0, 0, 0, -5);
    painter.setPen(Qt::NoPen);
    painter.setBrush(edge);
    painter.drawRoundedRect(slab.translated(0, isDown() ? 1 : 5), 6, 6);
    painter.setBrush(face);
    painter.drawRoundedRect(slab.translated(0, isDown() ? 4 : 0), 6, 6);

    const QRectF content = slab.translated(0, isDown() ? 4 : 0).adjusted(22, 0, -22, 0);
    const QColor ink = isEnabled() ? QColor(36, 22, 8) : QColor(140, 130, 120);
    painter.setPen(mAttention && isEnabled() ? QColor(250, 250, 255) : ink);
    const QRectF label = content.adjusted(0, 0, isEnabled() ? -28 : 0, 0);
    QFont titleFont = Display(22, QFont::Bold);
    painter.setFont(titleFont);
    while (titleFont.pointSize() > 12 && painter.fontMetrics().horizontalAdvance(text().toUpper()) > label.width())
    {
        titleFont.setPointSize(titleFont.pointSize() - 1);
        painter.setFont(titleFont);
    }
    painter.drawText(label.adjusted(0, 10, 0, -26), Qt::AlignLeft | Qt::AlignVCenter, text().toUpper());
    painter.setFont(Display(9, QFont::DemiBold));
    QColor detail = painter.pen().color();
    detail.setAlpha(190);
    painter.setPen(detail);
    painter.drawText(label.adjusted(0, 44, 0, -8), Qt::AlignLeft | Qt::AlignVCenter,
                     painter.fontMetrics().elidedText(mDetail.toUpper(), Qt::ElideRight, static_cast<int>(label.width())));

    if (isEnabled())
    {
        painter.setPen(QPen(painter.pen().color(), 3, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
        const QPointF tip(content.right(), content.center().y());
        painter.drawLine(tip + QPointF(-14, -10), tip);
        painter.drawLine(tip, tip + QPointF(-14, 10));
    }
}

void PlayButton::enterEvent(QEnterEvent*)
{
    mHover = true;
    update();
}

void PlayButton::leaveEvent(QEvent*)
{
    mHover = false;
    update();
}
