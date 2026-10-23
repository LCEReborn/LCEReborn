#ifndef GAME_COMMAND_CHAT_MESSAGECHATCOMMAND_HPP
#define GAME_COMMAND_CHAT_MESSAGECHATCOMMAND_HPP

#include "game/command/ChatCommand.hpp"

class MessageChatCommand : public ChatCommand
{
public:
    MessageChatCommand();

    virtual bool run(const CommandSource& source, const Words& words);
};

#endif
