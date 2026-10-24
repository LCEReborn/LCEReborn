#ifndef GAME_COMMAND_CHAT_KICKCHATCOMMAND_HPP
#define GAME_COMMAND_CHAT_KICKCHATCOMMAND_HPP

#include "game/command/ChatCommand.hpp"

class KickChatCommand : public ChatCommand
{
public:
    KickChatCommand();

    virtual bool run(const CommandSource& source, const Words& words);
};

#endif
