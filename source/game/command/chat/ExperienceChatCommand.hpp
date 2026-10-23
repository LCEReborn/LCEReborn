#ifndef GAME_COMMAND_CHAT_EXPERIENCECHATCOMMAND_HPP
#define GAME_COMMAND_CHAT_EXPERIENCECHATCOMMAND_HPP

#include "game/command/ChatCommand.hpp"

class ExperienceChatCommand : public ChatCommand
{
public:
    ExperienceChatCommand();

    virtual bool run(const CommandSource& source, const Words& words);
};

#endif
