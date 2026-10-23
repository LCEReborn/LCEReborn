#ifndef GAME_COMMAND_CHAT_SPAWNPOINTCHATCOMMAND_HPP
#define GAME_COMMAND_CHAT_SPAWNPOINTCHATCOMMAND_HPP

#include "game/command/ChatCommand.hpp"

class SpawnpointChatCommand : public ChatCommand
{
public:
    SpawnpointChatCommand();

    virtual bool run(const CommandSource& source, const Words& words);
};

#endif
