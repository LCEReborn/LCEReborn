#include "game/command/ConsoleCommandSender.hpp"

#include "game/server/MinecraftServer.hpp"
#include "game/server/ServerLevel.hpp"
#include "game/util/Vec3.hpp"
#include "game/world/block/BlockPos.hpp"

boost::shared_ptr<ConsoleCommandSender> ConsoleCommandSender::Get()
{
    static boost::shared_ptr<ConsoleCommandSender> sender(new ConsoleCommandSender());
    return sender;
}

void ConsoleCommandSender::sendMessage(ClientboundChatPacket::EChatPacketMessage, int*, unsigned int, std::wstring*,
                                       unsigned int)
{
}

bool ConsoleCommandSender::hasPermission(EGameCommand)
{
    return true;
}

std::wstring ConsoleCommandSender::getDisplayName()
{
    return gMinecraftServer->getConsoleName();
}

BlockPos ConsoleCommandSender::getCommandSenderBlockPosition()
{
    return gMinecraftServer->getLevel(0)->getSharedSpawnPos();
}

Vec3* ConsoleCommandSender::getCommandSenderWorldPosition()
{
    const BlockPos spawn = getCommandSenderBlockPosition();
    return Vec3::newTemp(spawn.x + 0.5, spawn.y, spawn.z + 0.5);
}

Level* ConsoleCommandSender::getCommandSenderWorld()
{
    return gMinecraftServer->getLevel(0);
}

bool ConsoleCommandSender::isSendMessageEnabled()
{
    return true;
}

void ConsoleCommandSender::setCommandStats(CommandStats::CommandStatType, int)
{
}

MinecraftServer* ConsoleCommandSender::getServer()
{
    return gMinecraftServer;
}
