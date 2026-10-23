#include "game/command/chat/ToggleDownfallChatCommand.hpp"

#include "game/server/MinecraftServer.hpp"
#include "game/server/ServerLevel.hpp"
#include "game/world/LevelData.hpp"

ToggleDownfallChatCommand::ToggleDownfallChatCommand() : ChatCommand(L"toggledownfall", L"", L"", ePermission_Operator)
{
}

bool ToggleDownfallChatCommand::run(const CommandSource& source, const Words& words)
{
    if (words.size() != 1)
        return false;
    LevelData* data = gMinecraftServer->getLevel(0)->getLevelData();
    data->setRaining(!data->isRaining());
    Tell(source, L"Toggled downfall");
    return true;
}
