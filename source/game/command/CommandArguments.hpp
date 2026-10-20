#ifndef GAME_COMMAND_COMMANDARGUMENTS_HPP
#define GAME_COMMAND_COMMANDARGUMENTS_HPP

#include <string>
#include <vector>

namespace CommandArguments
{
std::vector<std::wstring> Split(const std::wstring& line);
std::wstring Lower(const std::wstring& text);
std::wstring Join(const std::vector<std::wstring>& words, size_t first);
bool ParseNumber(const std::wstring& word, double* value);
bool ParseInt(const std::wstring& word, int minimum, int maximum, int* value);
bool ParseCoordinate(const std::wstring& word, double origin, bool centre, double* value);
bool ParsePosition(const std::vector<std::wstring>& words, size_t index, const double origin[3], float yRot,
                   float xRot, bool centre, double result[3], std::wstring* error);
std::wstring StripNamespace(const std::wstring& name);
std::wstring Pretty(const std::wstring& path);
std::wstring Number(int value);
}

#endif
