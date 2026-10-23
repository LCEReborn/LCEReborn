#ifndef GAME_COMMAND_CHAT_FILLCHATCOMMAND_HPP
#define GAME_COMMAND_CHAT_FILLCHATCOMMAND_HPP

#include "game/command/ChatCommand.hpp"

class FillChatCommand : public ChatCommand
{
public:
    FillChatCommand();

    virtual bool run(const CommandSource& source, const Words& words);
};

#endif
