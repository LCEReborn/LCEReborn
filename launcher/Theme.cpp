#include "Theme.hpp"

#include <QAbstractButton>
#include <QApplication>
#include <QComboBox>
#include <QPainter>
#include <QPalette>
#include <QProxyStyle>
#include <QStyleFactory>
#include <QStyleOption>

#include "Widgets.hpp"

namespace
{
const int kControlHeight = 40;
const int kIndicatorSize = 20;

FaceState StateOf(const QStyleOption* option)
{
    if (!(option->state & QStyle::State_Enabled))
        return FaceState::Disabled;
    if (option->state & QStyle::State_Sunken)
        return FaceState::Pressed;
    if (option->state & (QStyle::State_On | QStyle::State_MouseOver | QStyle::State_HasFocus))
        return FaceState::Highlighted;
    return FaceState::Normal;
}

QColor InkFor(FaceState state)
{
    switch (state)
    {
    case FaceState::Normal:
        return GameColours::kText;
    case FaceState::Disabled:
        return QColor(160, 160, 160);
    default:
        return GameColours::kHighlight;
    }
}

void PaintCells(QPainter& painter, const QRect& area, int grid, const QVector<QPoint>& cells, const QColor& colour)
{
    const int unit = qMax(1, area.width() / grid);
    const QPoint origin(area.left() + (area.width() - unit * grid) / 2, area.top() + (area.height() - unit * grid) / 2);
    for (const QPoint& cell : cells)
        painter.fillRect(QRect(origin + cell * unit, QSize(unit, unit)), colour);
}

class GameStyle : public QProxyStyle
{
public:
    GameStyle() : QProxyStyle(QStyleFactory::create("Fusion")) {}

    using QProxyStyle::polish;
    void polish(QWidget* widget) override
    {
        QProxyStyle::polish(widget);
        if (qobject_cast<QAbstractButton*>(widget) != nullptr || qobject_cast<QComboBox*>(widget) != nullptr)
            widget->setAttribute(Qt::WA_Hover);
    }

    void drawPrimitive(PrimitiveElement element, const QStyleOption* option, QPainter* painter,
                       const QWidget* widget) const override
    {
        switch (element)
        {
        case PE_PanelButtonCommand:
        case PE_PanelButtonBevel:
            PaintButtonFace(*painter, option->rect, StateOf(option));
            return;
        case PE_FrameFocusRect:
        case PE_FrameDefaultButton:
        case PE_FrameLineEdit:
            return;
        case PE_PanelLineEdit:
        {
            const bool enabled = option->state & State_Enabled;
            const bool focused = option->state & State_HasFocus;
            painter->fillRect(option->rect, enabled ? QColor(160, 160, 160) : QColor(96, 96, 96));
            if (focused)
                painter->fillRect(option->rect, Qt::white);
            painter->fillRect(option->rect.adjusted(2, 2, -2, -2), enabled ? Qt::black : QColor(32, 32, 32));
            return;
        }
        case PE_IndicatorCheckBox:
        {
            const QRect box = option->rect;
            painter->fillRect(box, Qt::black);
            const QRect inner = box.adjusted(2, 2, -2, -2);
            const bool enabled = option->state & State_Enabled;
            const bool hovered = enabled && (option->state & (State_MouseOver | State_HasFocus));
            painter->fillRect(inner, enabled ? (hovered ? QColor(145, 154, 202) : QColor(111, 111, 111)) : QColor(80, 80, 80));
            painter->fillRect(QRect(inner.left(), inner.top(), inner.width(), 2), QColor(55, 55, 55));
            painter->fillRect(QRect(inner.left(), inner.top(), 2, inner.height()), QColor(55, 55, 55));
            if (option->state & State_On)
            {
                const QVector<QPoint> tick = {{6, 1}, {6, 2}, {5, 2}, {5, 3}, {4, 3}, {4, 4}, {3, 4}, {3, 5}, {2, 5},
                                              {2, 4}, {1, 4}, {1, 3}, {0, 3}};
                PaintCells(*painter, inner.adjusted(1, 1, -1, -1), 8, tick,
                           enabled ? (hovered ? GameColours::kHighlight : GameColours::kText) : QColor(150, 150, 150));
            }
            return;
        }
        default:
            QProxyStyle::drawPrimitive(element, option, painter, widget);
        }
    }

    void drawControl(ControlElement element, const QStyleOption* option, QPainter* painter,
                     const QWidget* widget) const override
    {
        if (element == CE_PushButtonLabel)
        {
            const auto* button = qstyleoption_cast<const QStyleOptionButton*>(option);
            if (button == nullptr)
                return;
            const FaceState state = StateOf(option);
            QRect area = button->rect.adjusted(6, 0, -6, -2);
            if (!button->icon.isNull())
            {
                const QSize size = button->iconSize;
                const int textWidth = button->fontMetrics.horizontalAdvance(button->text);
                const int total = size.width() + (button->text.isEmpty() ? 0 : 8 + textWidth);
                const QRect iconRect(area.center().x() - total / 2, area.center().y() - size.height() / 2,
                                     size.width(), size.height());
                button->icon.paint(painter, iconRect, Qt::AlignCenter,
                                   state == FaceState::Disabled ? QIcon::Disabled : QIcon::Normal);
                area.setLeft(iconRect.right() + 8);
                area.setWidth(textWidth + 4);
            }
            painter->setFont(widget != nullptr ? widget->font() : QApplication::font());
            DrawGameText(*painter, area, Qt::AlignCenter | Qt::TextShowMnemonic, button->text, InkFor(state),
                         state != FaceState::Disabled);
            return;
        }
        if (element == CE_ComboBoxLabel)
        {
            const auto* combo = qstyleoption_cast<const QStyleOptionComboBox*>(option);
            if (combo == nullptr)
                return;
            const QRect field = subControlRect(CC_ComboBox, combo, SC_ComboBoxEditField, widget).adjusted(4, 0, 0, -2);
            const FaceState state = StateOf(option);
            painter->setFont(widget != nullptr ? widget->font() : QApplication::font());
            DrawGameText(*painter, field, Qt::AlignLeft | Qt::AlignVCenter,
                         painter->fontMetrics().elidedText(combo->currentText, Qt::ElideRight, field.width()),
                         InkFor(state), state != FaceState::Disabled);
            return;
        }
        QProxyStyle::drawControl(element, option, painter, widget);
    }

    void drawComplexControl(ComplexControl control, const QStyleOptionComplex* option, QPainter* painter,
                            const QWidget* widget) const override
    {
        if (control != CC_ComboBox)
        {
            QProxyStyle::drawComplexControl(control, option, painter, widget);
            return;
        }
        const FaceState state = StateOf(option);
        PaintButtonFace(*painter, option->rect, state);
        const QRect arrow = subControlRect(CC_ComboBox, option, SC_ComboBoxArrow, widget).adjusted(0, 0, -6, -2);
        const QVector<QPoint> cells = {{0, 2}, {1, 2}, {2, 2}, {3, 2}, {4, 2}, {5, 2}, {6, 2},
                                       {1, 3}, {2, 3}, {3, 3}, {4, 3}, {5, 3}, {2, 4}, {3, 4}, {4, 4}, {3, 5}};
        PaintCells(*painter, QRect(arrow.center().x() - 7, arrow.center().y() - 7, 14, 14), 7, cells, InkFor(state));
    }

    int pixelMetric(PixelMetric metric, const QStyleOption* option, const QWidget* widget) const override
    {
        switch (metric)
        {
        case PM_IndicatorWidth:
        case PM_IndicatorHeight:
            return kIndicatorSize;
        case PM_ButtonShiftHorizontal:
            return 0;
        case PM_ButtonShiftVertical:
            return 1;
        case PM_DefaultFrameWidth:
            return 4;
        case PM_ButtonMargin:
            return 18;
        case PM_CheckBoxLabelSpacing:
            return 10;
        default:
            return QProxyStyle::pixelMetric(metric, option, widget);
        }
    }

    QSize sizeFromContents(ContentsType type, const QStyleOption* option, const QSize& size,
                           const QWidget* widget) const override
    {
        QSize result = QProxyStyle::sizeFromContents(type, option, size, widget);
        if (type == CT_PushButton || type == CT_ComboBox)
            result = QSize(result.width() + 12, qMax(result.height(), kControlHeight));
        else if (type == CT_LineEdit)
            result.setHeight(qMax(result.height(), kControlHeight - 4));
        return result;
    }

    int styleHint(StyleHint hint, const QStyleOption* option, const QWidget* widget,
                  QStyleHintReturn* returnData) const override
    {
        if (hint == SH_DialogButtonBox_ButtonsHaveIcons || hint == SH_UnderlineShortcut)
            return 0;
        if (hint == SH_ComboBox_Popup)
            return 0;
        return QProxyStyle::styleHint(hint, option, widget, returnData);
    }
};
}

void ApplyTheme(QApplication& application)
{
    application.setStyle(new GameStyle);
    application.setFont(GameFont(16));

    const QColor panel(208, 208, 208);
    const QColor dark = GameColours::kDark;
    QPalette palette;
    palette.setColor(QPalette::Window, panel);
    palette.setColor(QPalette::WindowText, dark);
    palette.setColor(QPalette::Base, Qt::black);
    palette.setColor(QPalette::AlternateBase, QColor(24, 24, 24));
    palette.setColor(QPalette::ToolTipBase, QColor(16, 0, 16));
    palette.setColor(QPalette::ToolTipText, GameColours::kText);
    palette.setColor(QPalette::Text, QColor(224, 224, 224));
    palette.setColor(QPalette::PlaceholderText, QColor(112, 112, 112));
    palette.setColor(QPalette::Button, QColor(131, 131, 131));
    palette.setColor(QPalette::ButtonText, GameColours::kText);
    palette.setColor(QPalette::BrightText, Qt::white);
    palette.setColor(QPalette::Highlight, QColor(145, 154, 202));
    palette.setColor(QPalette::HighlightedText, GameColours::kHighlight);
    palette.setColor(QPalette::Link, QColor(32, 48, 160));
    palette.setColor(QPalette::LinkVisited, QColor(80, 32, 140));
    palette.setColor(QPalette::Disabled, QPalette::WindowText, GameColours::kMuted);
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor(112, 112, 112));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(160, 160, 160));
    application.setPalette(palette);

    application.setStyleSheet(R"(
        QLabel#Heading { font-size: 24px; color: #404040; }
        QLabel#Muted { color: #5a5a5a; }
        QLabel#NewsTitle { font-size: 24px; color: #404040; }
        QLabel#NewsDate { color: #5a5a5a; }
        QLabel#SetupStatement { font-size: 28px; color: #404040; }
        QLabel#StepTitle { font-size: 20px; color: #404040; }
        QLabel#StepHint { color: #5a5a5a; }
        QLabel#StepGood { color: #1f6f1f; }
        QLabel#StepBad { color: #9a2a1a; }
        QTextBrowser#NewsBody { background: transparent; border: none; color: #404040; }
        QListWidget#GameRows { background: transparent; border: none; outline: none; }
        QTableView#LogView { background: #000000; border: none; color: #e1e1e1; outline: none;
                             selection-background-color: #4a4f78; selection-color: #efef00; }
        QTableView#LogView QHeaderView::section { background: #000000; color: #a0a0a0; border: none;
                                                  border-bottom: 2px solid #3c3c3c; padding: 6px 10px; }
        QTableView#LogView QTableCornerButton::section { background: #000000; border: none; }
        QComboBox QAbstractItemView { background: #000000; color: #e0e0e0; border: 2px solid #a0a0a0; outline: none;
                                      selection-background-color: #919aca; selection-color: #efef00; }
        QMenu { background: #100010; border: 2px solid #2d0a63; padding: 4px; color: #efefef; }
        QMenu::item { padding: 8px 24px 8px 16px; }
        QMenu::item:selected { background: #919aca; color: #efef00; }
        QMenu::separator { height: 2px; background: #2d0a63; margin: 4px 6px; }
        QToolTip { background: #100010; border: 2px solid #2d0a63; color: #efefef; padding: 4px; }
        QProgressBar { background: #000000; border: 2px solid #a0a0a0; min-height: 16px; max-height: 16px; }
        QProgressBar::chunk { background: #80ff20; }
        QScrollBar:vertical { background: #3c3c3c; width: 14px; margin: 0; border: none; }
        QScrollBar::handle:vertical { background: #c6c6c6; min-height: 32px; border-top: 2px solid #ffffff;
                                      border-left: 2px solid #ffffff; border-bottom: 2px solid #555555;
                                      border-right: 2px solid #555555; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }
        QScrollBar:horizontal { background: #3c3c3c; height: 14px; margin: 0; border: none; }
        QScrollBar::handle:horizontal { background: #c6c6c6; min-width: 32px; border-top: 2px solid #ffffff;
                                        border-left: 2px solid #ffffff; border-bottom: 2px solid #555555;
                                        border-right: 2px solid #555555; }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }
        QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { background: none; }
    )");
}
