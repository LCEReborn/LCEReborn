#ifndef GAME_COMMAND_CHAT_TIMECHATCOMMAND_HPP
#define GAME_COMMAND_CHAT_TIMECHATCOMMAND_HPP

#include "game/command/ChatCommand.hpp"

class TimeChatCommand : public ChatCommand
{
public:
    TimeChatCommand();

    virtual bool run(const CommandSource& source, const Words& words);
};

#endif
