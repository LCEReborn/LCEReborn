#ifndef GAME_COMMAND_CHAT_DEFAULTGAMEMODECHATCOMMAND_HPP
#define GAME_COMMAND_CHAT_DEFAULTGAMEMODECHATCOMMAND_HPP

#include "game/command/ChatCommand.hpp"

class DefaultGameModeChatCommand : public ChatCommand
{
public:
    DefaultGameModeChatCommand();

    virtual bool run(const CommandSource& source, const Words& words);
};

#endif
