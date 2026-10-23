#include "game/command/chat/TeleportChatCommand.hpp"

#include "game/command/TargetSelector.hpp"
#include "game/command/TeleportCommand.hpp"
#include "game/entity/mob/Entity.hpp"
#include "game/server/ServerPlayer.hpp"
#include "game/world/Level.hpp"

TeleportChatCommand::TeleportChatCommand()
    : ChatCommand(L"tp", L"[targets] <destination|x y z>", L"targets targets coord coord", ePermission_Operator)
{
    addAlias(L"teleport");
}

bool TeleportChatCommand::run(const CommandSource& source, const Words& words)
{
    const size_t arguments = words.size() - 1;
    if (arguments < 1 || arguments > 4)
        return false;

    EntityList moved;
    size_t next = 1;
    if (arguments == 2 || arguments == 4)
    {
        if (!ResolveEntities(source, words[1], moved))
            return true;
        next = 2;
    }
    else if (source.entity)
        moved.push_back(source.entity);
    else
    {
        Fail(source, L"No entity was found.");
        return true;
    }

    EntityPtr destination;
    if (words.size() - next == 1)
    {
        EntityList found;
        if (!ResolveEntities(source, words[next], found))
            return true;
        if (found.size() != 1)
        {
            Fail(source, L"The destination matched more than one entity.");
            return true;
        }
        destination = found[0];
    }

    double target[3] = {0.0, 0.0, 0.0};
    if (!destination && !ParsePosition(source, words, next, true, target))
        return true;
    Level* level = destination ? destination->mLevel : source.level;
    const std::wstring where = destination ? TargetSelector::DisplayName(destination)
                                           : Position(target[0], target[1], target[2]);

    int teleported = 0;
    for (size_t index = 0; index < moved.size(); ++index)
    {
        const EntityPtr& entity = moved[index];
        if (entity->mLevel != level)
        {
            Fail(source, TargetSelector::DisplayName(entity) + L" is in another dimension.");
            continue;
        }
        const double x = destination ? destination->mX : target[0];
        const double y = destination ? destination->mY : target[1];
        const double z = destination ? destination->mZ : target[2];
        PlayerPtr movedPlayer = boost::dynamic_pointer_cast<ServerPlayer>(entity);
        PlayerPtr destinationPlayer = boost::dynamic_pointer_cast<ServerPlayer>(destination);
        if (movedPlayer && destinationPlayer)
            Perform(source, TeleportCommand::preparePacket(movedPlayer->getXuid(), destinationPlayer->getXuid()));
        else if (movedPlayer)
            Perform(source, TeleportCommand::preparePacket(movedPlayer->getXuid(), (float)x, (float)y, (float)z));
        else
            entity->moveTo(x, y, z, entity->mYRot, entity->mXRot);
        ++teleported;
    }
    if (teleported == 1 && moved.size() == 1)
        Tell(source, L"Teleported " + TargetSelector::DisplayName(moved[0]) + L" to " + where + L".");
    else if (teleported > 0)
        Tell(source, L"Teleported " + Count(teleported, L"entity", L"entities") + L" to " + where + L".");
    return true;
}
