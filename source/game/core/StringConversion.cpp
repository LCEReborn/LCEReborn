#include "game/core/StringConversion.hpp"

#include <cstdlib>
#include <cwchar>
#include <iomanip>
#include <sstream>

#include "platform/Platform.hpp"

static void Utf8ToWideChar(const char* source, wchar_t* destination, unsigned int capacity)
{
    unsigned int left = capacity - 1;
    wchar_t* output = destination;
    for (;;)
    {
        const unsigned int lead = static_cast<unsigned char>(*source);
        if (lead == 0 || left == 0)
        {
            *output = 0;
            return;
        }
        int extra;
        unsigned int code;
        if (lead < 0x80)
        {
            extra = 0;
            code = lead;
        }
        else if (lead >= 0xc2 && lead < 0xe0)
        {
            extra = 1;
            code = lead & 0x1f;
        }
        else if (lead >= 0xe0 && lead < 0xf0)
        {
            extra = 2;
            code = lead & 0x0f;
        }
        else if (lead >= 0xf0 && lead < 0xf5)
        {
            extra = 3;
            code = lead & 0x07;
        }
        else
        {
            *destination = 0;
            return;
        }
        for (int index = 1; index <= extra; ++index)
        {
            const unsigned int trail = static_cast<unsigned char>(source[index]);
            if ((trail & 0xc0) != 0x80)
            {
                *destination = 0;
                return;
            }
            code = (code << 6) | (trail & 0x3f);
        }
        if (code >= 0x10000 && sizeof(wchar_t) == 2)
        {
            if (left < 2)
            {
                *output = 0;
                return;
            }
            code -= 0x10000;
            *output++ = static_cast<wchar_t>(0xd800 + (code >> 10));
            *output++ = static_cast<wchar_t>(0xdc00 + (code & 0x3ff));
            left -= 2;
        }
        else
        {
            *output++ = static_cast<wchar_t>(code);
            --left;
        }
        source += extra + 1;
    }
}

std::wstring char16towstring(const char16_t* text, size_t capacity)
{
    std::wstring result;
    for (size_t index = 0; index < capacity && text[index] != 0; ++index)
        result += static_cast<wchar_t>(text[index]);
    return result;
}

void charTowchar_tArray(const char* text, wchar_t* destination, unsigned int capacity)
{
    bool finished = false;
    wchar_t* start = destination;
    for (;;)
    {
        *destination = *text;
        if (*text == '\0')
            finished = true;
        ++text;
        ++destination;
        if (finished)
            break;
        if (capacity <= (unsigned int)(destination - start))
            return;
    }
}

bool compareStrings(const std::wstring& first, const std::wstring& second)
{
    return std::wcscmp(first.c_str(), second.c_str()) < 0;
}

wchar_t* stringtowchararray(const std::string& text)
{
    static wchar_t buffer[256];

    const std::string::size_type length = text.size();
    for (std::string::size_type index = 0; index < length; ++index)
        buffer[index] = static_cast<signed char>(text[index]);
    buffer[length] = 0;
    return buffer;
}

std::wstring convStringToWstring(wchar_t* text)
{
    return std::wstring(text);
}

std::wstring convStringToWstring(const std::string& text)
{
    const unsigned int length = static_cast<unsigned int>(text.size());
    wchar_t* buffer = new wchar_t[length + 1];
    Utf8ToWideChar(text.c_str(), buffer, length + 1);
    std::wstring result;
    result.assign(buffer, std::wcslen(buffer));
    delete[] buffer;
    return result;
}

std::wstring convUtf8ToWstring(const char* text)
{
    return convStringToWstring(std::string(text));
}

std::string convWstringToString(const std::wstring& text)
{
    std::string result;
    result.reserve(text.size());
    for (size_t index = 0; index < text.size(); ++index)
    {
        unsigned int code = static_cast<unsigned int>(text[index]);
        if (code >= 0xd800 && code < 0xdc00 && index + 1 < text.size()
            && static_cast<unsigned int>(text[index + 1]) >= 0xdc00
            && static_cast<unsigned int>(text[index + 1]) < 0xe000)
        {
            code = 0x10000 + ((code - 0xd800) << 10) + (static_cast<unsigned int>(text[++index]) - 0xdc00);
        }
        else if ((code >= 0xd800 && code < 0xe000) || code > 0x10ffff)
        {
            code = 0xfffd;
        }
        if (code < 0x80)
        {
            result += static_cast<char>(code);
        }
        else if (code < 0x800)
        {
            result += static_cast<char>(0xc0 | (code >> 6));
            result += static_cast<char>(0x80 | (code & 0x3f));
        }
        else if (code < 0x10000)
        {
            result += static_cast<char>(0xe0 | (code >> 12));
            result += static_cast<char>(0x80 | ((code >> 6) & 0x3f));
            result += static_cast<char>(0x80 | (code & 0x3f));
        }
        else
        {
            result += static_cast<char>(0xf0 | (code >> 18));
            result += static_cast<char>(0x80 | ((code >> 12) & 0x3f));
            result += static_cast<char>(0x80 | ((code >> 6) & 0x3f));
            result += static_cast<char>(0x80 | (code & 0x3f));
        }
    }
    return result;
}

template <typename T>
std::wstring _toString(T value)
{
    std::wostringstream stream;
    stream << value;
    return stream.str();
}

template <typename T>
T _fromString(const std::wstring& value)
{
    std::wistringstream stream(value);
    T result;
    stream >> result;
    return result;
}

template <typename T>
std::wstring _toHexString(T value)
{
    std::wostringstream stream;
    stream << std::hex << value;
    return stream.str();
}

template <typename T>
T _fromHEXString(const std::wstring& value)
{
    std::wistringstream stream(value);
    T result;
    stream >> std::hex >> result;
    return result;
}

template std::wstring _toString<bool>(bool);
template std::wstring _toString<short>(short);
template std::wstring _toString<unsigned short>(unsigned short);
template std::wstring _toString<unsigned char>(unsigned char);
template std::wstring _toString<int>(int);
template std::wstring _toString<unsigned int>(unsigned int);
template std::wstring _toString<float>(float);
template std::wstring _toString<double>(double);
template std::wstring _toString<long long>(long long);
template std::wstring _toString<unsigned long long>(unsigned long long);
template std::wstring _toString<const Direction*>(const Direction*);
template std::wstring _toString<Block*>(Block*);
template std::wstring _toString<const BlockState*>(const BlockState*);

template bool _fromString<bool>(const std::wstring&);
template unsigned short _fromString<unsigned short>(const std::wstring&);
template int _fromString<int>(const std::wstring&);
template unsigned int _fromString<unsigned int>(const std::wstring&);
template float _fromString<float>(const std::wstring&);
template double _fromString<double>(const std::wstring&);
template long long _fromString<long long>(const std::wstring&);

template std::wstring _toHexString<long long>(long long);
template long long _fromHEXString<long long>(const std::wstring&);
template int _fromHEXString<int>(const std::wstring&);
