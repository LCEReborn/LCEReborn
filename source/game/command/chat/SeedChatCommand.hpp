#ifndef GAME_COMMAND_CHAT_SEEDCHATCOMMAND_HPP
#define GAME_COMMAND_CHAT_SEEDCHATCOMMAND_HPP

#include "game/command/ChatCommand.hpp"

class SeedChatCommand : public ChatCommand
{
public:
    SeedChatCommand();

    virtual bool run(const CommandSource& source, const Words& words);
};

#endif
