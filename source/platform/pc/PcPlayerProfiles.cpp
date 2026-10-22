#include "platform/pc/PcPlayerProfiles.hpp"

#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>

#include "platform/pc/Log.hpp"
#include "platform/pc/PcLaunchOptions.hpp"

namespace
{
const Logger sLog("Profiles");

struct JsonValue
{
    enum Type
    {
        Null,
        Boolean,
        Number,
        String,
        Array,
        Object
    };

    Type type = Null;
    std::wstring text;
    std::vector<JsonValue> items;
    std::map<std::wstring, JsonValue> members;
};

class JsonReader
{
public:
    explicit JsonReader(const std::string& source) : mSource(source), mPosition(0) {}

    bool Read(JsonValue& value)
    {
        if (!ReadValue(value, 0))
            return false;
        SkipSpace();
        return mPosition == mSource.size();
    }

private:
    static const int kMaxDepth = 32;

    void SkipSpace()
    {
        while (mPosition < mSource.size() && std::isspace(static_cast<unsigned char>(mSource[mPosition])))
            ++mPosition;
    }

    bool Match(const char* word)
    {
        const size_t length = std::char_traits<char>::length(word);
        if (mSource.compare(mPosition, length, word) != 0)
            return false;
        mPosition += length;
        return true;
    }

    bool ReadValue(JsonValue& value, int depth)
    {
        if (depth > kMaxDepth)
            return false;
        SkipSpace();
        if (mPosition >= mSource.size())
            return false;
        const char lead = mSource[mPosition];
        if (lead == '{')
            return ReadObject(value, depth);
        if (lead == '[')
            return ReadArray(value, depth);
        if (lead == '"')
        {
            value.type = JsonValue::String;
            return ReadString(value.text);
        }
        if (Match("true") || Match("false"))
        {
            value.type = JsonValue::Boolean;
            return true;
        }
        if (Match("null"))
        {
            value.type = JsonValue::Null;
            return true;
        }
        const char* start = mSource.c_str() + mPosition;
        char* end = NULL;
        std::strtod(start, &end);
        if (end == start)
            return false;
        mPosition += static_cast<size_t>(end - start);
        value.type = JsonValue::Number;
        return true;
    }

    bool ReadObject(JsonValue& value, int depth)
    {
        value.type = JsonValue::Object;
        ++mPosition;
        SkipSpace();
        if (mPosition < mSource.size() && mSource[mPosition] == '}')
        {
            ++mPosition;
            return true;
        }
        for (;;)
        {
            SkipSpace();
            std::wstring key;
            if (mPosition >= mSource.size() || mSource[mPosition] != '"' || !ReadString(key))
                return false;
            SkipSpace();
            if (mPosition >= mSource.size() || mSource[mPosition] != ':')
                return false;
            ++mPosition;
            if (!ReadValue(value.members[key], depth + 1))
                return false;
            SkipSpace();
            if (mPosition >= mSource.size())
                return false;
            if (mSource[mPosition] == '}')
            {
                ++mPosition;
                return true;
            }
            if (mSource[mPosition] != ',')
                return false;
            ++mPosition;
        }
    }

    bool ReadArray(JsonValue& value, int depth)
    {
        value.type = JsonValue::Array;
        ++mPosition;
        SkipSpace();
        if (mPosition < mSource.size() && mSource[mPosition] == ']')
        {
            ++mPosition;
            return true;
        }
        for (;;)
        {
            value.items.push_back(JsonValue());
            if (!ReadValue(value.items.back(), depth + 1))
                return false;
            SkipSpace();
            if (mPosition >= mSource.size())
                return false;
            if (mSource[mPosition] == ']')
            {
                ++mPosition;
                return true;
            }
            if (mSource[mPosition] != ',')
                return false;
            ++mPosition;
        }
    }

    bool ReadHex(unsigned int& code)
    {
        if (mPosition + 4 > mSource.size())
            return false;
        code = 0;
        for (int digit = 0; digit < 4; ++digit)
        {
            const char character = mSource[mPosition++];
            code <<= 4;
            if (character >= '0' && character <= '9')
                code |= static_cast<unsigned int>(character - '0');
            else if (character >= 'a' && character <= 'f')
                code |= static_cast<unsigned int>(character - 'a' + 10);
            else if (character >= 'A' && character <= 'F')
                code |= static_cast<unsigned int>(character - 'A' + 10);
            else
                return false;
        }
        return true;
    }

    static void Append(std::wstring& text, unsigned int code)
    {
        if (sizeof(wchar_t) == 2 && code >= 0x10000)
        {
            code -= 0x10000;
            text.push_back(static_cast<wchar_t>(0xd800 + (code >> 10)));
            text.push_back(static_cast<wchar_t>(0xdc00 + (code & 0x3ff)));
        }
        else
        {
            text.push_back(static_cast<wchar_t>(code));
        }
    }

    bool ReadString(std::wstring& text)
    {
        ++mPosition;
        while (mPosition < mSource.size())
        {
            unsigned char character = static_cast<unsigned char>(mSource[mPosition++]);
            if (character == '"')
                return true;
            if (character < 0x20)
                return false;
            if (character == '\\')
            {
                if (mPosition >= mSource.size())
                    return false;
                const char escape = mSource[mPosition++];
                unsigned int code = 0;
                switch (escape)
                {
                case '"':
                case '\\':
                case '/':
                    text.push_back(static_cast<wchar_t>(escape));
                    break;
                case 'b':
                    text.push_back(L'\b');
                    break;
                case 'f':
                    text.push_back(L'\f');
                    break;
                case 'n':
                    text.push_back(L'\n');
                    break;
                case 'r':
                    text.push_back(L'\r');
                    break;
                case 't':
                    text.push_back(L'\t');
                    break;
                case 'u':
                    if (!ReadHex(code))
                        return false;
                    if (code >= 0xd800 && code < 0xdc00 && mSource.compare(mPosition, 2, "\\u") == 0)
                    {
                        mPosition += 2;
                        unsigned int low = 0;
                        if (!ReadHex(low) || low < 0xdc00 || low >= 0xe000)
                            return false;
                        code = 0x10000 + ((code - 0xd800) << 10) + (low - 0xdc00);
                    }
                    Append(text, code);
                    break;
                default:
                    return false;
                }
                continue;
            }
            unsigned int code = character;
            int extra = 0;
            if (character >= 0xf0)
            {
                code = character & 0x07;
                extra = 3;
            }
            else if (character >= 0xe0)
            {
                code = character & 0x0f;
                extra = 2;
            }
            else if (character >= 0xc0)
            {
                code = character & 0x1f;
                extra = 1;
            }
            else if (character >= 0x80)
            {
                return false;
            }
            for (int index = 0; index < extra; ++index)
            {
                if (mPosition >= mSource.size())
                    return false;
                const unsigned char next = static_cast<unsigned char>(mSource[mPosition++]);
                if ((next & 0xc0) != 0x80)
                    return false;
                code = (code << 6) | (next & 0x3f);
            }
            Append(text, code);
        }
        return false;
    }

    const std::string& mSource;
    size_t mPosition;
};

const std::wstring* Member(const JsonValue& object, const wchar_t* name)
{
    std::map<std::wstring, JsonValue>::const_iterator found = object.members.find(name);
    if (found == object.members.end() || found->second.type != JsonValue::String)
        return NULL;
    return &found->second.text;
}

bool ValidUsername(const std::wstring& username)
{
    if (username.size() < 3 || username.size() > 16)
        return false;
    for (wchar_t character : username)
    {
        const bool letter = (character >= L'a' && character <= L'z') || (character >= L'A' && character <= L'Z');
        const bool digit = character >= L'0' && character <= L'9';
        if (!letter && !digit && character != L'_')
            return false;
    }
    return true;
}

bool ValidUuid(const std::wstring& uuid)
{
    if (uuid.size() != 32)
        return false;
    for (wchar_t character : uuid)
    {
        if (!((character >= L'0' && character <= L'9') || (character >= L'a' && character <= L'f')))
            return false;
    }
    return true;
}
}

std::vector<PcPlayerProfile> PcReadPlayerProfiles()
{
    std::vector<PcPlayerProfile> profiles;
    const std::string& path = PcGetLaunchOptions().profiles;
    if (path.empty())
        return profiles;

    std::ifstream input(std::filesystem::u8path(path), std::ios::binary);
    if (!input)
    {
        LOG_WARN(sLog, "%s could not be read", path.c_str());
        return profiles;
    }
    const std::string source((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());

    JsonValue root;
    JsonReader reader(source);
    if (!reader.Read(root) || root.type != JsonValue::Object)
    {
        LOG_WARN(sLog, "%s is not a launcher profiles file", path.c_str());
        return profiles;
    }
    std::map<std::wstring, JsonValue>::const_iterator list = root.members.find(L"profiles");
    if (list == root.members.end() || list->second.type != JsonValue::Array)
        return profiles;

    for (const JsonValue& entry : list->second.items)
    {
        if (entry.type != JsonValue::Object)
            continue;
        const std::wstring* name = Member(entry, L"name");
        const std::wstring* username = Member(entry, L"username");
        const std::wstring* uuid = Member(entry, L"uuid");
        if (username == NULL || uuid == NULL || !ValidUsername(*username) || !ValidUuid(*uuid))
            continue;
        PcPlayerProfile profile;
        profile.username = *username;
        profile.name = name != NULL && !name->empty() ? *name : *username;
        profile.uuid.assign(uuid->begin(), uuid->end());
        profiles.push_back(profile);
    }
    return profiles;
}
