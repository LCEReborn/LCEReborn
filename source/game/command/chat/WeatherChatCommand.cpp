#include "game/command/chat/WeatherChatCommand.hpp"

#include "game/command/CommandArguments.hpp"
#include "game/command/WeatherCommand.hpp"

using CommandArguments::Lower;

WeatherChatCommand::WeatherChatCommand()
    : ChatCommand(L"weather", L"<clear|rain|thunder>", L"clear|rain|thunder", ePermission_Operator)
{
}

bool WeatherChatCommand::run(const CommandSource& source, const Words& words)
{
    const std::wstring value = words.size() == 2 ? Lower(words[1]) : std::wstring();
    unsigned char mode;
    if (value == L"clear")
        mode = 0;
    else if (value == L"rain")
        mode = 1;
    else if (value == L"thunder")
        mode = 2;
    else
        return false;
    Perform(source, WeatherCommand::preparePacket(mode));
    return true;
}
