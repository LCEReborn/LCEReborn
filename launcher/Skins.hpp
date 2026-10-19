#ifndef LAUNCHER_SKINS_HPP
#define LAUNCHER_SKINS_HPP

#include <QImage>

struct Profile;

namespace Skins
{
QImage FaceFor(const Profile& profile, int size);
}

#endif
