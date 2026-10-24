#ifndef GAME_COMMAND_CHAT_SETBLOCKCHATCOMMAND_HPP
#define GAME_COMMAND_CHAT_SETBLOCKCHATCOMMAND_HPP

#include "game/command/ChatCommand.hpp"

class SetBlockChatCommand : public ChatCommand
{
public:
    SetBlockChatCommand();

    virtual bool run(const CommandSource& source, const Words& words);
};

#endif
