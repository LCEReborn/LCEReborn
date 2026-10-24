#ifndef GAME_COMMAND_CHAT_OPERATORCHATCOMMAND_HPP
#define GAME_COMMAND_CHAT_OPERATORCHATCOMMAND_HPP

#include "game/command/ChatCommand.hpp"

class OperatorChatCommand : public ChatCommand
{
public:
    explicit OperatorChatCommand(bool op);

    virtual bool run(const CommandSource& source, const Words& words);

private:
    bool mOp;
};

#endif
