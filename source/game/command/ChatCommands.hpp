#ifndef GAME_COMMAND_CHATCOMMANDS_HPP
#define GAME_COMMAND_CHATCOMMANDS_HPP

#include <string>

#include <boost/shared_ptr.hpp>

class ServerPlayer;

namespace ChatCommands
{
const unsigned int kMaxMessageLength = 100;

std::wstring CleanMessage(const std::wstring& message);
std::wstring FormatMessage(const std::wstring& message);
void Reply(boost::shared_ptr<ServerPlayer> player, const std::wstring& text);
void Error(boost::shared_ptr<ServerPlayer> player, const std::wstring& text);
void Run(boost::shared_ptr<ServerPlayer> player, const std::wstring& line);
}

#endif
