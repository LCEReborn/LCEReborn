#include "game/command/chat/KickChatCommand.hpp"

#include "game/command/ChatCommands.hpp"
#include "game/command/CommandArguments.hpp"
#include "game/network/INetworkPlayer.hpp"
#include "game/network/ServerGamePacketListenerImpl.hpp"
#include "game/server/MinecraftServer.hpp"
#include "game/server/ServerPlayer.hpp"
#include "game/world/generation/PlayerList.hpp"

KickChatCommand::KickChatCommand() : ChatCommand(L"kick", L"<player> [reason]", L"player text", ePermission_Operator)
{
}

bool KickChatCommand::run(const CommandSource& source, const Words& words)
{
    if (words.size() < 2)
        return false;
    PlayerPtr target = gMinecraftServer->getPlayers()->getPlayerByNameIgnoringCase(words[1]);
    if (!target)
    {
        Fail(source, L"There is no player called " + words[1] + L".");
        return true;
    }
    if (!target->mConnection)
    {
        Fail(source, target->getName() + L" can't be kicked.");
        return true;
    }
    INetworkPlayer* network = target->mConnection->getNetworkPlayer();
    if (network == NULL || network->IsHost() || network->IsLocal())
    {
        Fail(source, L"You can't kick the host or a player playing on the host's console.");
        return true;
    }
    const std::wstring reason = ChatCommands::FormatMessage(CommandArguments::Join(words, 2));
    const std::wstring name = target->getName();
    target->mConnection->kick();
    Tell(source, L"Kicked " + name + L" from the game" + (reason.empty() ? std::wstring() : L": '" + reason + L"§r'"));
    return true;
}
