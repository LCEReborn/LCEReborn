#ifndef GAME_COMMAND_CHAT_SUMMONCHATCOMMAND_HPP
#define GAME_COMMAND_CHAT_SUMMONCHATCOMMAND_HPP

#include "game/command/ChatCommand.hpp"

class SummonChatCommand : public ChatCommand
{
public:
    SummonChatCommand();

    virtual bool run(const CommandSource& source, const Words& words);
};

#endif
