#ifndef GAME_COMMAND_CHAT_GAMEMODECHATCOMMAND_HPP
#define GAME_COMMAND_CHAT_GAMEMODECHATCOMMAND_HPP

#include "game/command/ChatCommand.hpp"

class GameModeChatCommand : public ChatCommand
{
public:
    GameModeChatCommand();

    virtual bool run(const CommandSource& source, const Words& words);
};

#endif
