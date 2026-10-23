#ifndef GAME_COMMAND_CONSOLECOMMANDSENDER_HPP
#define GAME_COMMAND_CONSOLECOMMANDSENDER_HPP

#include <string>

#include <boost/shared_ptr.hpp>

#include "game/command/CommandSender.hpp"

class ConsoleCommandSender : public CommandSender
{
public:
    static boost::shared_ptr<ConsoleCommandSender> Get();

    virtual void sendMessage(ClientboundChatPacket::EChatPacketMessage message, int* intParams,
                             unsigned int numIntParams, std::wstring* stringParams, unsigned int numStringParams);
    virtual bool hasPermission(EGameCommand command);
    virtual std::wstring getDisplayName();
    virtual BlockPos getCommandSenderBlockPosition();
    virtual Vec3* getCommandSenderWorldPosition();
    virtual Level* getCommandSenderWorld();
    virtual bool isSendMessageEnabled();
    virtual void setCommandStats(CommandStats::CommandStatType statType, int value);
    virtual MinecraftServer* getServer();
};

#endif
