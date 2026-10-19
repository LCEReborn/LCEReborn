#ifndef LAUNCHER_BLOCKICONS_HPP
#define LAUNCHER_BLOCKICONS_HPP

#include <QImage>
#include <QString>

namespace BlockIcons
{
QString BlockFor(const QString& commit);
QImage Render(const QString& commit, int size);
QImage RenderCube(const QImage& top, const QImage& front, const QImage& side, int size);
}

#endif
