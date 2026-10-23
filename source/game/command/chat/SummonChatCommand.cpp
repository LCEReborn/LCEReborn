#include "game/command/chat/SummonChatCommand.hpp"

#include "game/command/CommandArguments.hpp"
#include "game/core/LightningBolt.hpp"
#include "game/entity/mob/Entity.hpp"
#include "game/entity/mob/EntityIO.hpp"
#include "game/item/SpawnEggItem.hpp"
#include "game/server/ServerPlayer.hpp"
#include "game/util/ResourceLocation.hpp"
#include "game/world/Level.hpp"
#include "game/world/block/BlockPos.hpp"

using CommandArguments::StripNamespace;

SummonChatCommand::SummonChatCommand()
    : ChatCommand(L"summon", L"<entity> [x y z]", L"entity coord coord coord", ePermission_Operator)
{
}

bool SummonChatCommand::run(const CommandSource& source, const Words& words)
{
    if (words.size() != 2 && words.size() != 5)
        return false;
    double x = source.x;
    double y = source.y;
    double z = source.z;
    if (words.size() == 5)
    {
        double result[3];
        if (!ParsePosition(source, words, 2, true, result))
            return true;
        x = result[0];
        y = result[1];
        z = result[2];
    }
    Level* level = source.level;
    const BlockPos pos(x, y, z);
    if (!level->isInWorldBounds(pos) || !level->hasChunkAt(pos))
    {
        Fail(source, L"Cannot summon the object out of the world");
        return true;
    }

    const std::wstring type = StripNamespace(words[1]);
    if (type == L"lightning_bolt" || type == L"lightningbolt")
    {
        level->addGlobalEntity(boost::shared_ptr<Entity>(new LightningBolt(level, x, y, z, false)));
        Tell(source, L"Object successfully summoned");
        return true;
    }
    const ResourceLocation location(type);
    if (type == L"player" || !EntityIO::isValidEntityType(location))
    {
        Fail(source, L"Unknown entity type " + words[1] + L".");
        return true;
    }

    EntityPtr entity;
    if (EntityIO::sSpawnInfo.find(location) != EntityIO::sSpawnInfo.end())
    {
        int error = 0;
        entity = SpawnEggItem::spawnMobAt(level, location, x, y, z, &error, L"");
        if (!entity && error != 0)
        {
            if (source.output)
                SpawnEggItem::DisplaySpawnError(source.output, error);
            return true;
        }
    }
    else
    {
        entity = EntityIO::newEntity(location, level);
        if (entity)
        {
            entity->moveTo(x, y, z, source.yRot, 0.0f);
            if (!level->addEntity(entity))
                entity.reset();
        }
    }
    if (!entity)
    {
        Fail(source, L"Unable to summon object");
        return true;
    }
    Tell(source, L"Object successfully summoned");
    return true;
}
