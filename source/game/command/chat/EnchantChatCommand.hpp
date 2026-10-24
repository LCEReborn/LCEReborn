#ifndef GAME_COMMAND_CHAT_ENCHANTCHATCOMMAND_HPP
#define GAME_COMMAND_CHAT_ENCHANTCHATCOMMAND_HPP

#include "game/command/ChatCommand.hpp"

class EnchantChatCommand : public ChatCommand
{
public:
    EnchantChatCommand();

    virtual bool run(const CommandSource& source, const Words& words);
};

#endif
