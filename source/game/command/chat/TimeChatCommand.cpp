#include "game/command/chat/TimeChatCommand.hpp"

#include "game/command/CommandArguments.hpp"
#include "game/command/TimeCommand.hpp"
#include "game/server/MinecraftServer.hpp"
#include "game/server/ServerLevel.hpp"

using CommandArguments::Lower;

TimeChatCommand::TimeChatCommand()
    : ChatCommand(L"time", L"<set|add|query> <value>", L"set|add|query time", ePermission_Operator)
{
}

bool TimeChatCommand::run(const CommandSource& source, const Words& words)
{
    if (words.size() < 2 || words.size() > 3)
        return false;
    const std::wstring operation = words.size() == 2 ? std::wstring(L"set") : Lower(words[1]);
    const std::wstring value = Lower(words[words.size() - 1]);
    ServerLevel* level = gMinecraftServer->getLevel(0);
    if (operation == L"query")
    {
        if (value == L"daytime")
            Tell(source, L"The time is " + LongNumber(level->getDayTime() % 24000));
        else if (value == L"gametime")
            Tell(source, L"The time is " + LongNumber(level->getGameTime()));
        else if (value == L"day")
            Tell(source, L"The day is " + LongNumber(level->getDayTime() / 24000));
        else
            return false;
        return true;
    }

    double number = 0.0;
    const bool numeric = CommandArguments::ParseNumber(value, &number) && number >= 0.0 && number <= 2147483647.0;
    long long time = 0;
    if (operation == L"set")
    {
        if (value == L"day")
            time = 1000;
        else if (value == L"noon")
            time = 6000;
        else if (value == L"night")
            time = 13000;
        else if (value == L"midnight")
            time = 18000;
        else if (numeric)
            time = static_cast<long long>(number);
        else
            return false;
    }
    else if (operation == L"add" && numeric)
        time = level->getDayTime() + static_cast<long long>(number);
    else
        return false;
    Perform(source, TimeCommand::preparePacket(time));
    return true;
}
