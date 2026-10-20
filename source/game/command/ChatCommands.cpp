#include "game/command/ChatCommands.hpp"

#include <stdio.h>
#include <wctype.h>

#include <vector>

#include "game/command/CommandDispatcher.hpp"
#include "game/command/GameModeCommand.hpp"
#include "game/command/KillCommand.hpp"
#include "game/command/TeleportCommand.hpp"
#include "game/command/TimeCommand.hpp"
#include "game/command/WeatherCommand.hpp"
#include "game/core/GameType.hpp"
#include "game/core/StringUtils.hpp"
#include "game/network/ClientboundChatPacket.hpp"
#include "game/network/GameCommandPacket.hpp"
#include "game/server/MinecraftServer.hpp"
#include "game/server/ServerPlayer.hpp"
#include "game/world/generation/PlayerList.hpp"

namespace
{
const wchar_t* const kTimeUsage = L"Usage: /time set <day|noon|night|midnight|ticks>";
const wchar_t* const kWeatherUsage = L"Usage: /weather <clear|rain|thunder>";
const wchar_t* const kGameModeUsage = L"Usage: /gamemode <survival|creative|adventure> [player]";
const wchar_t* const kTeleportUsage = L"Usage: /tp [player] <player|x y z>";

std::vector<std::wstring> Split(const std::wstring& line)
{
    std::vector<std::wstring> words;
    size_t index = 0;
    while (index < line.size())
    {
        while (index < line.size() && line[index] == L' ')
            ++index;
        size_t end = index;
        while (end < line.size() && line[end] != L' ')
            ++end;
        if (end > index)
            words.push_back(line.substr(index, end - index));
        index = end;
    }
    return words;
}

bool ParseNumber(const std::wstring& word, double* value)
{
    size_t index = 0;
    bool negative = false;
    if (index < word.size() && (word[index] == L'-' || word[index] == L'+'))
        negative = word[index++] == L'-';
    double number = 0.0;
    double scale = 0.0;
    bool digits = false;
    for (; index < word.size(); ++index)
    {
        const wchar_t character = word[index];
        if (character >= L'0' && character <= L'9')
        {
            digits = true;
            if (scale == 0.0)
                number = number * 10.0 + (character - L'0');
            else
            {
                number += (character - L'0') * scale;
                scale *= 0.1;
            }
        }
        else if (character == L'.' && scale == 0.0)
            scale = 0.1;
        else
            return false;
    }
    if (!digits)
        return false;
    *value = negative ? -number : number;
    return true;
}

bool ParseCoordinate(const std::wstring& word, double origin, double* value)
{
    if (!word.empty() && word[0] == L'~')
    {
        double offset = 0.0;
        if (word.size() > 1 && !ParseNumber(word.substr(1), &offset))
            return false;
        *value = origin + offset;
        return true;
    }
    return ParseNumber(word, value);
}

std::wstring Position(double x, double y, double z)
{
    wchar_t buffer[96];
    swprintf(buffer, sizeof(buffer) / sizeof(buffer[0]), L"%.1f, %.1f, %.1f", x, y, z);
    return buffer;
}

boost::shared_ptr<ServerPlayer> FindPlayer(boost::shared_ptr<ServerPlayer> player, const std::wstring& name)
{
    boost::shared_ptr<ServerPlayer> found = gMinecraftServer->getPlayers()->getPlayerByNameIgnoringCase(name);
    if (!found)
        ChatCommands::Error(player, L"There is no player called " + name + L".");
    return found;
}

void Perform(boost::shared_ptr<ServerPlayer> player, boost::shared_ptr<GameCommandPacket> packet)
{
    gMinecraftServer->getCommandDispatcher()->performCommand(player, packet->command, packet->data);
}

void Help(boost::shared_ptr<ServerPlayer> player)
{
    ChatCommands::Reply(player, L"§6Commands:");
    ChatCommands::Reply(player, L"§e/time §fset §7<day|noon|night|midnight|ticks>");
    ChatCommands::Reply(player, L"§e/weather §7<clear|rain|thunder>");
    ChatCommands::Reply(player, L"§e/gamemode §7<survival|creative|adventure> [player]");
    ChatCommands::Reply(player, L"§e/tp §7[player] <player|x y z>");
    ChatCommands::Reply(player, L"§e/kill");
}

void Time(boost::shared_ptr<ServerPlayer> player, const std::vector<std::wstring>& words)
{
    const size_t first = words.size() > 1 && toLower(words[1]) == L"set" ? 2 : 1;
    if (words.size() != first + 1)
    {
        ChatCommands::Error(player, kTimeUsage);
        return;
    }
    const std::wstring value = toLower(words[first]);
    long long time = 0;
    double number = 0.0;
    if (value == L"day")
        time = 1000;
    else if (value == L"noon")
        time = 6000;
    else if (value == L"night")
        time = 13000;
    else if (value == L"midnight")
        time = 18000;
    else if (ParseNumber(value, &number) && number >= 0.0)
        time = (long long)number;
    else
    {
        ChatCommands::Error(player, kTimeUsage);
        return;
    }
    Perform(player, TimeCommand::preparePacket(time));
}

void Weather(boost::shared_ptr<ServerPlayer> player, const std::vector<std::wstring>& words)
{
    const std::wstring value = words.size() == 2 ? toLower(words[1]) : std::wstring();
    unsigned char mode;
    if (value == L"clear")
        mode = 0;
    else if (value == L"rain")
        mode = 1;
    else if (value == L"thunder")
        mode = 2;
    else
    {
        ChatCommands::Error(player, kWeatherUsage);
        return;
    }
    Perform(player, WeatherCommand::preparePacket(mode));
}

void GameMode(boost::shared_ptr<ServerPlayer> player, const std::vector<std::wstring>& words)
{
    if (words.size() < 2 || words.size() > 3)
    {
        ChatCommands::Error(player, kGameModeUsage);
        return;
    }
    const struct
    {
        const GameType* type;
        const wchar_t* names[3];
    } modes[] = {
        {GameType::SURVIVAL, {L"survival", L"s", L"0"}},
        {GameType::CREATIVE, {L"creative", L"c", L"1"}},
        {GameType::ADVENTURE, {L"adventure", L"a", L"2"}},
    };
    const std::wstring value = toLower(words[1]);
    const GameType* chosen = NULL;
    for (size_t mode = 0; mode < sizeof(modes) / sizeof(modes[0]); ++mode)
    {
        for (size_t name = 0; name < 3; ++name)
        {
            if (value == modes[mode].names[name])
                chosen = modes[mode].type;
        }
    }
    if (chosen == NULL)
    {
        ChatCommands::Error(player, kGameModeUsage);
        return;
    }
    boost::shared_ptr<ServerPlayer> target = player;
    if (words.size() == 3)
    {
        target = FindPlayer(player, words[2]);
        if (!target)
            return;
    }
    Perform(player, GameModeCommand::preparePacket(target->getXuid(), (unsigned char)chosen->getId()));
}

void Teleport(boost::shared_ptr<ServerPlayer> player, const std::vector<std::wstring>& words)
{
    const size_t arguments = words.size() - 1;
    if (arguments < 1 || arguments > 4)
    {
        ChatCommands::Error(player, kTeleportUsage);
        return;
    }

    boost::shared_ptr<ServerPlayer> moved = player;
    size_t next = 1;
    if (arguments == 2 || arguments == 4)
    {
        moved = FindPlayer(player, words[1]);
        if (!moved)
            return;
        next = 2;
    }

    if (words.size() - next == 1)
    {
        boost::shared_ptr<ServerPlayer> target = FindPlayer(player, words[next]);
        if (!target)
            return;
        if (target->mDimension != moved->mDimension)
        {
            ChatCommands::Error(player, target->getName() + L" is in another dimension.");
            return;
        }
        Perform(player, TeleportCommand::preparePacket(moved->getXuid(), target->getXuid()));
        ChatCommands::Reply(player, L"Teleported " + moved->getName() + L" to " + target->getName() + L".");
        return;
    }

    double x;
    double y;
    double z;
    if (!ParseCoordinate(words[next], moved->mX, &x) || !ParseCoordinate(words[next + 1], moved->mY, &y)
        || !ParseCoordinate(words[next + 2], moved->mZ, &z))
    {
        ChatCommands::Error(player, L"Coordinates are numbers, or ~ for where the player is.");
        return;
    }
    Perform(player, TeleportCommand::preparePacket(moved->getXuid(), (float)x, (float)y, (float)z));
    ChatCommands::Reply(player, L"Teleported " + moved->getName() + L" to " + Position(x, y, z) + L".");
}

void Kill(boost::shared_ptr<ServerPlayer> player, const std::vector<std::wstring>&)
{
    Perform(player, KillCommand::preparePacket());
}

struct Entry
{
    const wchar_t* name;
    EGameCommand command;
    void (*run)(boost::shared_ptr<ServerPlayer> player, const std::vector<std::wstring>& words);
};

const Entry kEntries[] = {
    {L"time", eGameCommand_Time, Time},
    {L"weather", eGameCommand_Weather, Weather},
    {L"gamemode", eGameCommand_GameMode, GameMode},
    {L"tp", eGameCommand_Teleport, Teleport},
    {L"teleport", eGameCommand_Teleport, Teleport},
    {L"kill", eGameCommand_Kill, Kill},
};
}

namespace ChatCommands
{
std::wstring CleanMessage(const std::wstring& message)
{
    std::wstring clean;
    for (size_t index = 0; index < message.size() && clean.size() < kMaxMessageLength; ++index)
    {
        const wchar_t character = message[index];
        if (character < 0x20 || (character >= 0x7f && character < 0xa0) || character == 0xa7)
            continue;
        if (character == L' ' && (clean.empty() || clean[clean.size() - 1] == L' '))
            continue;
        clean.push_back(character);
    }
    while (!clean.empty() && clean[clean.size() - 1] == L' ')
        clean.erase(clean.size() - 1);
    return clean;
}

std::wstring FormatMessage(const std::wstring& message)
{
    std::wstring formatted;
    bool visible = false;
    for (size_t index = 0; index < message.size(); ++index)
    {
        const wchar_t code = index + 1 < message.size() ? static_cast<wchar_t>(towlower(message[index + 1])) : 0;
        if (message[index] == L'&' && ((code >= L'0' && code <= L'9') || (code >= L'a' && code <= L'f')
                                       || (code >= L'k' && code <= L'o') || code == L'r'))
        {
            formatted.push_back(static_cast<wchar_t>(0xA7));
            formatted.push_back(code);
            ++index;
            continue;
        }
        if (message[index] != L' ')
            visible = true;
        formatted.push_back(message[index]);
    }
    return visible ? formatted : std::wstring();
}

void Reply(boost::shared_ptr<ServerPlayer> player, const std::wstring& text)
{
    std::wstring strings[2] = {std::wstring(), text};
    player->sendMessage(ClientboundChatPacket::eChatPacketMessage_PlayerChat, NULL, 0, strings, 2);
}

void Error(boost::shared_ptr<ServerPlayer> player, const std::wstring& text)
{
    Reply(player, L"§c" + text);
}

void Run(boost::shared_ptr<ServerPlayer> player, const std::wstring& line)
{
    const std::vector<std::wstring> words = Split(line.substr(1));
    if (words.empty())
        return;
    const std::wstring name = toLower(words[0]);
    if (name == L"help" || name == L"?")
    {
        Help(player);
        return;
    }
    for (size_t index = 0; index < sizeof(kEntries) / sizeof(kEntries[0]); ++index)
    {
        const Entry& entry = kEntries[index];
        if (name != entry.name)
            continue;
        if (!player->hasPermission(entry.command))
        {
            Error(player, L"You don't have permission to use this command.");
            return;
        }
        entry.run(player, words);
        return;
    }
    Error(player, L"Unknown command /" + name + L". Type /help for the commands.");
}
}
