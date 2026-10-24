#include "game/command/chat/SeedChatCommand.hpp"

#include "game/server/MinecraftServer.hpp"
#include "game/server/ServerLevel.hpp"

SeedChatCommand::SeedChatCommand() : ChatCommand(L"seed", L"", L"", ePermission_Operator)
{
}

bool SeedChatCommand::run(const CommandSource& source, const Words& words)
{
    if (words.size() != 1)
        return false;
    Tell(source, L"Seed: [§a" + LongNumber(gMinecraftServer->getLevel(0)->getSeed()) + L"§r]");
    return true;
}
