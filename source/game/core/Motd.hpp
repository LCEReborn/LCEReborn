#ifndef GAME_CORE_MOTD_HPP
#define GAME_CORE_MOTD_HPP

#include <stddef.h>
#include <string>
#include <vector>

class Font;

namespace Motd
{
const size_t kMaxLength = 2048;

// Java properties-file escapes: \n, \t, \uXXXX, and a backslash before any other character.
std::wstring Unescape(const std::wstring& text);

// A message of the day as Java servers take it (a JSON text component, or text with § or & codes, &#RRGGBB
// and §x§R§R§G§G§B§B colours and newlines), as the § text the game draws. Hex colours become §x sequences.
std::wstring Parse(const std::wstring& text);

// The first maxLines lines of a parsed message wrapped to width, each starting with the formatting in effect.
std::vector<std::wstring> Lines(Font& font, const std::wstring& motd, int width, size_t maxLines);

std::wstring HexCode(unsigned int rgb);
}

#endif
