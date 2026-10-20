#include "game/core/StringConversion.hpp"

#include <cstdlib>
#include <cstring>
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
        unsigned int lead = static_cast<unsigned char>(*source);
        if (lead == 0 || left == 0)
        {
            *output = 0;
            return;
        }
        if (lead < 0x80)
        {
            *output++ = static_cast<wchar_t>(lead);
        }
        else if (lead - 0xc2 < 0x1e)
        {
            unsigned int trail = static_cast<unsigned char>(source[1]);
            *output++ = static_cast<wchar_t>(lead * 0x40 + trail - 0x3080);
            ++source;
        }
        else if (lead - 0xe0 < 0x10)
        {
            unsigned int second = static_cast<unsigned char>(source[1]);
            unsigned int third = static_cast<unsigned char>(source[2]);
            *output++ = static_cast<wchar_t>(lead * 0x1000 + second * 0x40 + third - 0x2080);
            source += 2;
        }
        else
        {
            *destination = 0;
            return;
        }
        --left;
        ++source;
    }
}

std::wstring char16towstring(wchar_t* text)
{
    return std::wstring(text);
}

std::wstring convUtf16ToWstring(const void* text, unsigned int length)
{
    std::wstring result;
    const unsigned char* bytes = static_cast<const unsigned char*>(text);
    for (unsigned int i = 0; i < length; ++i)
    {
        unsigned short unit;
        std::memcpy(&unit, bytes + i * 2, sizeof(unit));
        if (unit == 0)
            break;
        unsigned int codePoint = unit;
        if (sizeof(wchar_t) > 2 && unit >= 0xd800 && unit <= 0xdbff && i + 1 < length)
        {
            unsigned short low;
            std::memcpy(&low, bytes + (i + 1) * 2, sizeof(low));
            if (low >= 0xdc00 && low <= 0xdfff)
            {
                codePoint = 0x10000 + ((unit - 0xd800) << 10) + (low - 0xdc00);
                ++i;
            }
        }
        result.push_back(static_cast<wchar_t>(codePoint));
    }
    return result;
}

void convWstringToUtf16(const std::wstring& text, unsigned short* destination, unsigned int capacity)
{
    if (capacity == 0)
        return;
    unsigned int output = 0;
    for (std::wstring::size_type i = 0; i < text.size() && output + 1 < capacity; ++i)
    {
        unsigned int codePoint = static_cast<unsigned int>(text[i]);
        if (sizeof(wchar_t) == 2 && codePoint >= 0xd800 && codePoint <= 0xdbff && i + 1 < text.size())
        {
            unsigned int low = static_cast<unsigned int>(text[i + 1]);
            if (low >= 0xdc00 && low <= 0xdfff)
            {
                codePoint = 0x10000 + ((codePoint - 0xd800) << 10) + (low - 0xdc00);
                ++i;
            }
        }
        if (codePoint > 0xffff && codePoint <= 0x10ffff)
        {
            if (output + 2 >= capacity)
                break;
            codePoint -= 0x10000;
            destination[output++] = static_cast<unsigned short>(0xd800 + (codePoint >> 10));
            destination[output++] = static_cast<unsigned short>(0xdc00 + (codePoint & 0x3ff));
        }
        else
            destination[output++] = static_cast<unsigned short>(codePoint <= 0xffff ? codePoint : 0xfffd);
    }
    destination[output] = 0;
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
    char* buffer = new char[text.size() + 1];
    std::string result;
    result.assign(buffer, std::strlen(buffer));
    delete[] buffer;
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
