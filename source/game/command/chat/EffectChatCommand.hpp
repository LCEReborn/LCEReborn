#ifndef GAME_COMMAND_CHAT_EFFECTCHATCOMMAND_HPP
#define GAME_COMMAND_CHAT_EFFECTCHATCOMMAND_HPP

#include "game/command/ChatCommand.hpp"

class EffectChatCommand : public ChatCommand
{
public:
    EffectChatCommand();

    virtual bool run(const CommandSource& source, const Words& words);
};

#endif
