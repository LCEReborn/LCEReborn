#include "Icons.hpp"

#include <QPainter>
#include <QPainterPath>
#include <QPixmap>

#include <functional>

namespace
{
const int kSize = 16;

QPixmap Draw(const QColor& colour, qreal scale, const std::function<void(QPainter&)>& paint)
{
    QPixmap pixmap(static_cast<int>(kSize * scale), static_cast<int>(kSize * scale));
    pixmap.setDevicePixelRatio(scale);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(colour, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);
    paint(painter);
    return pixmap;
}

QIcon Make(const std::function<void(QPainter&)>& paint)
{
    QIcon icon;
    for (qreal scale : {1.0, 2.0})
    {
        icon.addPixmap(Draw(QColor(236, 230, 221), scale, paint), QIcon::Normal);
        icon.addPixmap(Draw(QColor(232, 165, 75), scale, paint), QIcon::Active);
        icon.addPixmap(Draw(QColor(111, 103, 95), scale, paint), QIcon::Disabled);
    }
    return icon;
}
}

namespace Icons
{
QIcon Plus()
{
    return Make([](QPainter& painter) {
        painter.drawLine(QPointF(8, 3), QPointF(8, 13));
        painter.drawLine(QPointF(3, 8), QPointF(13, 8));
    });
}

QIcon Pencil()
{
    return Make([](QPainter& painter) {
        QPainterPath body;
        body.moveTo(3, 13);
        body.lineTo(3.6, 10.2);
        body.lineTo(10.8, 3);
        body.lineTo(13, 5.2);
        body.lineTo(5.8, 12.4);
        body.closeSubpath();
        painter.drawPath(body);
        painter.drawLine(QPointF(9.4, 4.4), QPointF(11.6, 6.6));
    });
}

QIcon Bin()
{
    return Make([](QPainter& painter) {
        painter.drawLine(QPointF(2.5, 4.5), QPointF(13.5, 4.5));
        painter.drawLine(QPointF(6.5, 4.5), QPointF(6.5, 2.5));
        painter.drawLine(QPointF(6.5, 2.5), QPointF(9.5, 2.5));
        painter.drawLine(QPointF(9.5, 2.5), QPointF(9.5, 4.5));
        QPainterPath can;
        can.moveTo(4, 4.5);
        can.lineTo(4.8, 13.5);
        can.lineTo(11.2, 13.5);
        can.lineTo(12, 4.5);
        painter.drawPath(can);
        painter.drawLine(QPointF(6.8, 7), QPointF(7, 11));
        painter.drawLine(QPointF(9.2, 7), QPointF(9, 11));
    });
}

QIcon List()
{
    return Make([](QPainter& painter) {
        for (qreal y : {4.0, 8.0, 12.0})
        {
            painter.drawPoint(QPointF(3, y));
            painter.drawLine(QPointF(6, y), QPointF(13.5, y));
        }
    });
}

QIcon Folder()
{
    return Make([](QPainter& painter) {
        QPainterPath folder;
        folder.moveTo(2, 4);
        folder.lineTo(6.2, 4);
        folder.lineTo(7.6, 5.6);
        folder.lineTo(14, 5.6);
        folder.lineTo(14, 13);
        folder.lineTo(2, 13);
        folder.closeSubpath();
        painter.drawPath(folder);
        painter.drawLine(QPointF(2, 7.6), QPointF(14, 7.6));
    });
}
}
