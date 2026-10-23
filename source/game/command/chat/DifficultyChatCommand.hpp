#ifndef GAME_COMMAND_CHAT_DIFFICULTYCHATCOMMAND_HPP
#define GAME_COMMAND_CHAT_DIFFICULTYCHATCOMMAND_HPP

#include "game/command/ChatCommand.hpp"

class DifficultyChatCommand : public ChatCommand
{
public:
    DifficultyChatCommand();

    virtual bool run(const CommandSource& source, const Words& words);
};

#endif
