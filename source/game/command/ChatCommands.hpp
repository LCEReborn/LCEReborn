#ifndef GAME_COMMAND_CHATCOMMANDS_HPP
#define GAME_COMMAND_CHATCOMMANDS_HPP

#include <stddef.h>

#include <string>

#include <boost/shared_ptr.hpp>

class ServerPlayer;

namespace ChatCommands
{
const unsigned int kMaxMessageLength = 100;

struct CommandUsage
{
    const wchar_t* name;
    const wchar_t* usage;
    const wchar_t* arguments;
};

std::wstring CleanMessage(const std::wstring& message);
std::wstring FormatMessage(const std::wstring& message);
void Reply(boost::shared_ptr<ServerPlayer> player, const std::wstring& text);
void Error(boost::shared_ptr<ServerPlayer> player, const std::wstring& text);
void Run(boost::shared_ptr<ServerPlayer> player, const std::wstring& line);
const CommandUsage* Usages(size_t* count);
}

#endif
