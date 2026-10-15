#ifndef PLATFORM_PC_PCPLATFORM_HPP
#define PLATFORM_PC_PCPLATFORM_HPP

#include "game/world/generation/PlayerUID.hpp"

wchar_t* GetRichPresenceStringFromId(int id);
bool IsEqualXUID(PlayerUID first, PlayerUID second);

#endif
