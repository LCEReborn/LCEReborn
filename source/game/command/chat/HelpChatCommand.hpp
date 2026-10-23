#ifndef GAME_COMMAND_CHAT_HELPCHATCOMMAND_HPP
#define GAME_COMMAND_CHAT_HELPCHATCOMMAND_HPP

#include "game/command/ChatCommand.hpp"

class HelpChatCommand : public ChatCommand
{
public:
    HelpChatCommand();

    virtual bool run(const CommandSource& source, const Words& words);
};

#endif
