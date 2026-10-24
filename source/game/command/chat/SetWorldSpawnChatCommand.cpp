#include "game/command/chat/SetWorldSpawnChatCommand.hpp"

#include "game/network/ClientboundSetSpawnPositionPacket.hpp"
#include "game/server/MinecraftServer.hpp"
#include "game/server/ServerLevel.hpp"
#include "game/world/block/BlockPos.hpp"
#include "game/world/generation/PlayerList.hpp"

SetWorldSpawnChatCommand::SetWorldSpawnChatCommand()
    : ChatCommand(L"setworldspawn", L"[x y z]", L"coord coord coord", ePermission_Operator)
{
}

bool SetWorldSpawnChatCommand::run(const CommandSource& source, const Words& words)
{
    if (words.size() != 1 && words.size() != 4)
        return false;
    if (!InOverworld(source))
    {
        Fail(source, L"The world spawn can only be set in the Overworld.");
        return true;
    }
    BlockPos pos = SourceBlockPos(source);
    if (words.size() == 4 && !ParseBlockPos(source, words, 1, &pos))
        return true;
    gMinecraftServer->getLevel(0)->setSpawnPos(pos);
    gMinecraftServer->getPlayers()->broadcastAll(
        boost::shared_ptr<Packet>(new ClientboundSetSpawnPositionPacket(pos.x, pos.y, pos.z)));
    Tell(source, L"Set the world spawn point to (" + Position(pos) + L")");
    return true;
}
