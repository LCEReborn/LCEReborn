#ifndef GAME_COMMAND_CHAT_TELEPORTCHATCOMMAND_HPP
#define GAME_COMMAND_CHAT_TELEPORTCHATCOMMAND_HPP

#include "game/command/ChatCommand.hpp"

class TeleportChatCommand : public ChatCommand
{
public:
    TeleportChatCommand();

    virtual bool run(const CommandSource& source, const Words& words);
};

#endif
