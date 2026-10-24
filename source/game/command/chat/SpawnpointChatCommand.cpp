#include "game/command/chat/SpawnpointChatCommand.hpp"

#include "game/server/ServerPlayer.hpp"
#include "game/world/block/BlockPos.hpp"

SpawnpointChatCommand::SpawnpointChatCommand()
    : ChatCommand(L"spawnpoint", L"[player] [x y z]", L"targets coord coord coord", ePermission_Operator)
{
}

bool SpawnpointChatCommand::run(const CommandSource& source, const Words& words)
{
    if (words.size() != 1 && words.size() != 2 && words.size() != 5)
        return false;
    if (!InOverworld(source))
    {
        Fail(source, L"Spawn points can only be set in the Overworld.");
        return true;
    }
    BlockPos pos = SourceBlockPos(source);
    if (words.size() == 5 && !ParseBlockPos(source, words, 2, &pos))
        return true;
    PlayerVector targets;
    if (!ResolvePlayers(source, words, 1, targets))
        return true;
    for (size_t index = 0; index < targets.size(); ++index)
    {
        targets[index]->setRespawnPosition(new BlockPos(pos), true);
        Tell(source, L"Set " + targets[index]->getName() + L"'s spawn point to (" + Position(pos) + L")");
    }
    return true;
}
