#ifndef GAME_COMMAND_CHAT_SAYCHATCOMMAND_HPP
#define GAME_COMMAND_CHAT_SAYCHATCOMMAND_HPP

#include "game/command/ChatCommand.hpp"

class SayChatCommand : public ChatCommand
{
public:
    SayChatCommand();

    virtual bool run(const CommandSource& source, const Words& words);
};

#endif
