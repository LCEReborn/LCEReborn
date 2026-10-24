#include "game/command/chat/OperatorChatCommand.hpp"

#include "game/core/CMinecraftApp.hpp"
#include "game/network/INetworkPlayer.hpp"
#include "game/network/PlayerInfoPacket.hpp"
#include "game/network/ServerGamePacketListenerImpl.hpp"
#include "game/server/MinecraftServer.hpp"
#include "game/server/ServerPlayer.hpp"
#include "game/world/generation/PlayerList.hpp"

OperatorChatCommand::OperatorChatCommand(bool op)
    : ChatCommand(op ? L"op" : L"deop", L"<player|uuid>", L"player", ePermission_Operator),
      mOp(op)
{
}

bool OperatorChatCommand::run(const CommandSource& source, const Words& words)
{
    if (words.size() != 2)
        return false;
    PlayerList* players = gMinecraftServer->getPlayers();
    const bool dedicated = CMinecraftApp::IsDedicatedServer();
    PlayerPtr target = players->getPlayerByNameIgnoringCase(words[1]);
    if (!target && !dedicated)
    {
        Fail(source, L"There is no player called " + words[1] + L".");
        return true;
    }
    std::wstring id;
    std::wstring name;
    if (target)
    {
        INetworkPlayer* network = target->mConnection ? target->mConnection->getNetworkPlayer() : NULL;
        if (network != NULL && network->IsHost())
        {
            Fail(source, L"The host is always an operator.");
            return true;
        }
        id = target->getXuid().toString();
        name = target->getName();
    }
    else if (PlayerList::ParsePlayerId(words[1], &id))
    {
        std::vector<PlayerPtr>* online = players->getPlayers();
        for (size_t index = 0; index < online->size() && !target; ++index)
        {
            if ((*online)[index]->getXuid().toString() == id)
                target = (*online)[index];
        }
        if (target)
            name = target->getName();
    }
    else
    {
        ServerOperator known;
        if (!mOp && PlayerList::FindOperatorByName(words[1], &known))
            id = known.id;
        name = mOp || known.name.empty() ? words[1] : known.name;
    }

    bool changed = false;
    if (dedicated)
        changed = mOp ? PlayerList::AddOperator(id, name) : PlayerList::RemoveOperator(id, name);
    if (target && target->isModerator() != mOp)
    {
        target->setPlayerGamePrivilege(Player::ePlayerGamePrivilege_Op, mOp ? 1 : 0);
        players->broadcastAll(boost::shared_ptr<Packet>(new PlayerInfoPacket(target)));
        changed = true;
    }
    if (!changed)
    {
        Fail(source, mOp ? L"Nothing changed. The player already is an operator"
                         : L"Nothing changed. The player is not an operator");
        return true;
    }
    const std::wstring shown = name.empty() ? id : name;
    Tell(source, mOp ? L"Made " + shown + L" a server operator" : L"Made " + shown + L" no longer a server operator");
    if (mOp && id.empty())
        Tell(source, L"No player called " + name
                         + L" is online, so the first player with that name to join is "
                           L"made the operator and kept by their UUID. Anyone can join with any name, so give the "
                           L"player's UUID instead if you know it.");
    return true;
}
