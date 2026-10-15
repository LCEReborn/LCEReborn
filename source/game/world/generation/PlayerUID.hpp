#ifndef GAME_WORLD_GENERATION_PLAYERUID_HPP
#define GAME_WORLD_GENERATION_PLAYERUID_HPP

#include <string>

#include "game/util/ACTUuid.hpp"
#include "platform/Platform.hpp"

#if LCE_PLATFORM_WIIU
struct FFLCreateID;
#endif

struct PlayerUID
{
    enum Flag
    {
        FlagShift = 28,
        FlagOnlineAccount = 0x40000000u,
        FlagDrcController = 0x10000000u,
        FlagMiiPrincipal = 0x20000000u,
        FlagUuid = 0x80000000u,
        FlagPayloadMask = 0x0fffffffu,
        FlagMiiMask = 0x5fffffffu,
        FlagUuidMask = 0xdfffffffu
    };

    union
    {
        ACTUuid uuid;
        struct
        {
            unsigned char createId[10];
            union
            {
                unsigned int principalId;
                unsigned char principalIdBytes[4];
            };
        } mii;
    };
    unsigned int flags;

    PlayerUID(int = 0);
    PlayerUID(ACTUuid uuid, bool onlineAccount, bool drcController);
#if LCE_PLATFORM_WIIU
    PlayerUID(FFLCreateID createId, unsigned int principalId, bool hasPrincipalId, bool drcController);
#endif
    PlayerUID(std::wstring uidString);

    std::wstring UuidString() const;
    std::wstring toString() const;
    bool operator==(const PlayerUID& other) const;
    bool operator!=(const PlayerUID& other) const;
};

std::size_t hash_value(const PlayerUID& uid);

#endif
