#ifndef GAME_COMMAND_CHAT_LISTCHATCOMMAND_HPP
#define GAME_COMMAND_CHAT_LISTCHATCOMMAND_HPP

#include "game/command/ChatCommand.hpp"

class ListChatCommand : public ChatCommand
{
public:
    ListChatCommand();

    virtual bool run(const CommandSource& source, const Words& words);
};

#endif
