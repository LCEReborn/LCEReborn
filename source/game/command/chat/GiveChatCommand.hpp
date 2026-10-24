#ifndef GAME_COMMAND_CHAT_GIVECHATCOMMAND_HPP
#define GAME_COMMAND_CHAT_GIVECHATCOMMAND_HPP

#include "game/command/ChatCommand.hpp"

class GiveChatCommand : public ChatCommand
{
public:
    GiveChatCommand();

    virtual bool run(const CommandSource& source, const Words& words);
};

#endif
