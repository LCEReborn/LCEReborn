#include "game/command/chat/DefaultGameModeChatCommand.hpp"

#include "game/command/DefaultGameModeCommand.hpp"
#include "game/core/GameType.hpp"

DefaultGameModeChatCommand::DefaultGameModeChatCommand()
    : ChatCommand(L"defaultgamemode", L"<mode>", L"mode", ePermission_Operator)
{
}

bool DefaultGameModeChatCommand::run(const CommandSource& source, const Words& words)
{
    const GameType* type = NULL;
    if (words.size() != 2 || !ParseGameType(words[1], &type))
        return false;
    Perform(source, DefaultGameModeCommand::preparePacket((unsigned char)type->getId()));
    return true;
}
