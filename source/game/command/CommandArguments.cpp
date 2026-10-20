#include "game/command/CommandArguments.hpp"

#include <math.h>
#include <stdio.h>
#include <wctype.h>

namespace CommandArguments
{
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

std::wstring Lower(const std::wstring& text)
{
    std::wstring result(text);
    for (size_t index = 0; index < result.size(); ++index)
        result[index] = static_cast<wchar_t>(towlower(result[index]));
    return result;
}

std::wstring Join(const std::vector<std::wstring>& words, size_t first)
{
    std::wstring result;
    for (size_t index = first; index < words.size(); ++index)
    {
        if (!result.empty())
            result += L' ';
        result += words[index];
    }
    return result;
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

bool ParseInt(const std::wstring& word, int minimum, int maximum, int* value)
{
    double number = 0.0;
    if (!ParseNumber(word, &number) || number != floor(number) || number < minimum || number > maximum)
        return false;
    *value = static_cast<int>(number);
    return true;
}

bool ParseCoordinate(const std::wstring& word, double origin, bool centre, double* value)
{
    if (!word.empty() && word[0] == L'~')
    {
        double offset = 0.0;
        if (word.size() > 1 && !ParseNumber(word.substr(1), &offset))
            return false;
        *value = origin + offset;
        return true;
    }
    if (!ParseNumber(word, value))
        return false;
    if (centre && word.find(L'.') == std::wstring::npos)
        *value += 0.5;
    return true;
}

bool ParsePosition(const std::vector<std::wstring>& words, size_t index, const double origin[3], float yRot,
                   float xRot, bool centre, double result[3], std::wstring* error)
{
    int local = 0;
    for (size_t axis = 0; axis < 3; ++axis)
    {
        if (!words[index + axis].empty() && words[index + axis][0] == L'^')
            ++local;
    }
    if (local != 0 && local != 3)
    {
        *error = L"Local coordinates with ^ can't be mixed with other coordinates.";
        return false;
    }
    if (local == 0)
    {
        for (size_t axis = 0; axis < 3; ++axis)
        {
            if (!ParseCoordinate(words[index + axis], origin[axis], centre && axis != 1, &result[axis]))
            {
                *error = L"Coordinates are numbers, ~ for relative or ^ for local.";
                return false;
            }
        }
        return true;
    }

    double offsets[3] = {0.0, 0.0, 0.0};
    for (size_t axis = 0; axis < 3; ++axis)
    {
        const std::wstring& word = words[index + axis];
        if (word.size() > 1 && !ParseNumber(word.substr(1), &offsets[axis]))
        {
            *error = L"Coordinates are numbers, ~ for relative or ^ for local.";
            return false;
        }
    }
    const double radians = 3.14159265358979323846 / 180.0;
    const double yawCos = cos((yRot + 90.0) * radians);
    const double yawSin = sin((yRot + 90.0) * radians);
    const double pitchCos = cos(-xRot * radians);
    const double pitchSin = sin(-xRot * radians);
    const double upCos = cos((-xRot + 90.0) * radians);
    const double upSin = sin((-xRot + 90.0) * radians);
    const double forward[3] = {yawCos * pitchCos, pitchSin, yawSin * pitchCos};
    const double up[3] = {yawCos * upCos, upSin, yawSin * upCos};
    const double left[3] = {-(forward[1] * up[2] - forward[2] * up[1]), -(forward[2] * up[0] - forward[0] * up[2]),
                            -(forward[0] * up[1] - forward[1] * up[0])};
    for (size_t axis = 0; axis < 3; ++axis)
        result[axis] = origin[axis] + left[axis] * offsets[0] + up[axis] * offsets[1] + forward[axis] * offsets[2];
    return true;
}

std::wstring StripNamespace(const std::wstring& name)
{
    const std::wstring lower = Lower(name);
    const std::wstring prefix = L"minecraft:";
    if (lower.compare(0, prefix.size(), prefix) == 0)
        return lower.substr(prefix.size());
    return lower;
}

std::wstring Pretty(const std::wstring& path)
{
    std::wstring text;
    bool start = true;
    for (size_t index = 0; index < path.size(); ++index)
    {
        if (path[index] == L'_')
        {
            text.push_back(L' ');
            start = true;
            continue;
        }
        text.push_back(start ? static_cast<wchar_t>(towupper(path[index])) : path[index]);
        start = false;
    }
    return text;
}

std::wstring Number(int value)
{
    wchar_t buffer[24];
    swprintf(buffer, sizeof(buffer) / sizeof(buffer[0]), L"%d", value);
    return buffer;
}
}
