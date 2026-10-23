#ifndef GAME_COMMAND_CHAT_CLEARCHATCOMMAND_HPP
#define GAME_COMMAND_CHAT_CLEARCHATCOMMAND_HPP

#include "game/command/ChatCommand.hpp"

class ClearChatCommand : public ChatCommand
{
public:
    ClearChatCommand();

    virtual bool run(const CommandSource& source, const Words& words);
};

#endif
