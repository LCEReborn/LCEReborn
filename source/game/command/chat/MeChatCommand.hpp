#ifndef GAME_COMMAND_CHAT_MECHATCOMMAND_HPP
#define GAME_COMMAND_CHAT_MECHATCOMMAND_HPP

#include "game/command/ChatCommand.hpp"

class MeChatCommand : public ChatCommand
{
public:
    MeChatCommand();

    virtual bool run(const CommandSource& source, const Words& words);
};

#endif
