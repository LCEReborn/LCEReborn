#ifndef GAME_COMMAND_CHAT_SETWORLDSPAWNCHATCOMMAND_HPP
#define GAME_COMMAND_CHAT_SETWORLDSPAWNCHATCOMMAND_HPP

#include "game/command/ChatCommand.hpp"

class SetWorldSpawnChatCommand : public ChatCommand
{
public:
    SetWorldSpawnChatCommand();

    virtual bool run(const CommandSource& source, const Words& words);
};

#endif
