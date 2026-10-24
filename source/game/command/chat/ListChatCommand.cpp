#include "game/command/chat/ListChatCommand.hpp"

#include "game/command/CommandArguments.hpp"
#include "game/command/TargetSelector.hpp"
#include "game/core/CMinecraftApp.hpp"
#include "game/network/CGameNetworkManager.hpp"
#include "game/server/MinecraftServer.hpp"
#include "game/server/ServerPlayer.hpp"
#include "game/world/generation/PlayerList.hpp"

using CommandArguments::Number;

ListChatCommand::ListChatCommand() : ChatCommand(L"list", L"", L"", ePermission_Everyone)
{
}

bool ListChatCommand::run(const CommandSource& source, const Words& words)
{
    if (words.size() != 1)
        return false;
    std::vector<PlayerPtr>* players = gMinecraftServer->getPlayers()->getPlayers();
    int maxPlayers = static_cast<int>(CGameNetworkManager::sInstance.GetMaxPlayers());
    if (CMinecraftApp::IsDedicatedServer())
        --maxPlayers;
    Tell(source, L"There are §e" + Number(static_cast<int>(players->size())) + L"§r/§e" + Number(maxPlayers)
                     + L"§r players online:");
    Tell(source, TargetSelector::Names(*players));
    return true;
}
