#ifndef LAUNCHER_SKINS_HPP
#define LAUNCHER_SKINS_HPP

#include <QImage>

struct Profile;

namespace Skins
{
QImage SkinFor(const Profile& profile);
QImage Face(const QImage& skin);
}

#endif
