#ifndef GAME_COMMAND_CHAT_TOGGLEDOWNFALLCHATCOMMAND_HPP
#define GAME_COMMAND_CHAT_TOGGLEDOWNFALLCHATCOMMAND_HPP

#include "game/command/ChatCommand.hpp"

class ToggleDownfallChatCommand : public ChatCommand
{
public:
    ToggleDownfallChatCommand();

    virtual bool run(const CommandSource& source, const Words& words);
};

#endif
