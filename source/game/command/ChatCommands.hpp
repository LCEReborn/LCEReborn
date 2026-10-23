#ifndef GAME_COMMAND_CHATCOMMANDS_HPP
#define GAME_COMMAND_CHATCOMMANDS_HPP

#include <stddef.h>

#include <string>
#include <vector>

#include <boost/shared_ptr.hpp>

#include "game/world/biome/arrayWithLength.hpp"

class ChatCommand;
class ServerPlayer;

namespace ChatCommands
{
const unsigned int kMaxMessageLength = 100;

struct CommandUsage
{
    std::wstring name;
    std::wstring usage;
    std::wstring arguments;
};

void Register(ChatCommand* command);
ChatCommand* Find(const std::wstring& name);
const std::vector<ChatCommand*>& Commands();
int PermissionOf(boost::shared_ptr<ServerPlayer> player);
arrayWithLength<unsigned char> WriteUsages(int permission);
std::vector<CommandUsage> ReadUsages(arrayWithLength<unsigned char> data);

std::wstring CleanMessage(const std::wstring& message);
std::wstring FormatMessage(const std::wstring& message);
void Reply(boost::shared_ptr<ServerPlayer> player, const std::wstring& text);
void Error(boost::shared_ptr<ServerPlayer> player, const std::wstring& text);
void Run(boost::shared_ptr<ServerPlayer> player, const std::wstring& line);
void RunConsole(const std::wstring& line);
void SetConsoleOutput(void (*output)(const std::wstring& text));
void Echo(const std::wstring& text);
}

#endif
