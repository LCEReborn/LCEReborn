#ifndef GAME_COMMAND_CHAT_KILLCHATCOMMAND_HPP
#define GAME_COMMAND_CHAT_KILLCHATCOMMAND_HPP

#include "game/command/ChatCommand.hpp"

class KillChatCommand : public ChatCommand
{
public:
    KillChatCommand();

    virtual bool run(const CommandSource& source, const Words& words);
};

#endif
