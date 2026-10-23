#include "game/command/chat/DifficultyChatCommand.hpp"

#include "game/command/CommandArguments.hpp"
#include "game/command/GameDifficultyCommand.hpp"

using CommandArguments::Lower;

DifficultyChatCommand::DifficultyChatCommand()
    : ChatCommand(L"difficulty", L"<peaceful|easy|normal|hard>", L"peaceful|easy|normal|hard", ePermission_Operator)
{
}

bool DifficultyChatCommand::run(const CommandSource& source, const Words& words)
{
    if (words.size() != 2)
        return false;
    const std::wstring value = Lower(words[1]);
    int id = -1;
    if (value == L"peaceful" || value == L"p" || value == L"0")
        id = 0;
    else if (value == L"easy" || value == L"e" || value == L"1")
        id = 1;
    else if (value == L"normal" || value == L"n" || value == L"2")
        id = 2;
    else if (value == L"hard" || value == L"h" || value == L"3")
        id = 3;
    else
        return false;
    Perform(source, GameDifficultyCommand::preparePacket(id));
    return true;
}
