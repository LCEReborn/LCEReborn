#include "game/core/Motd.hpp"

#include <cwchar>
#include <cwctype>

#include "game/client/render/Font.hpp"

namespace Motd
{
namespace
{
const wchar_t kSection = 0xA7;
const size_t kMaxJsonDepth = 64;
const int kDefaultColour = -1;
const int kHexColour = 0x1000000;

const wchar_t* const kColourNames[] = {L"black",     L"dark_blue", L"dark_green", L"dark_aqua",    L"dark_red",
                                       L"dark_purple", L"gold",    L"gray",       L"dark_gray",    L"blue",
                                       L"green",     L"aqua",      L"red",        L"light_purple", L"yellow",
                                       L"white"};

int HexDigit(wchar_t character)
{
    if (character >= L'0' && character <= L'9')
        return character - L'0';
    if (character >= L'a' && character <= L'f')
        return character - L'a' + 10;
    if (character >= L'A' && character <= L'F')
        return character - L'A' + 10;
    return -1;
}

bool IsColourCode(wchar_t code)
{
    return HexDigit(code) >= 0 || code == L'r';
}

bool IsFormatCode(wchar_t code)
{
    return code >= L'k' && code <= L'o';
}

wchar_t Lower(wchar_t character)
{
    return static_cast<wchar_t>(towlower(character));
}

bool ReadHex(const std::wstring& text, size_t start, size_t step, unsigned int& rgb)
{
    if (start + 6 * step > text.size() + step - 1)
        return false;
    rgb = 0;
    for (size_t digit = 0; digit < 6; ++digit)
    {
        const size_t index = start + digit * step;
        if (index >= text.size() || HexDigit(text[index]) < 0)
            return false;
        if (step == 2 && text[index - 1] != kSection && text[index - 1] != L'&')
            return false;
        rgb = (rgb << 4) | static_cast<unsigned int>(HexDigit(text[index]));
    }
    return true;
}

bool HasVisibleText(const std::wstring& text)
{
    for (size_t index = 0; index < text.size(); ++index)
    {
        if (text[index] == kSection)
        {
            ++index;
            continue;
        }
        if (!iswspace(text[index]))
            return true;
    }
    return false;
}

struct Json
{
    enum Type
    {
        eNull,
        eBool,
        eNumber,
        eString,
        eArray,
        eObject
    };

    Json() : type(eNull), boolean(false) {}

    const Json* Find(const wchar_t* key) const
    {
        for (size_t index = 0; index < keys.size(); ++index)
            if (keys[index] == key)
                return &items[index];
        return NULL;
    }

    Type type;
    bool boolean;
    std::wstring text;
    std::vector<Json> items;
    std::vector<std::wstring> keys;
};

class JsonParser
{
public:
    explicit JsonParser(const std::wstring& text) : m_text(text), m_position(0), m_depth(0) {}

    bool Parse(Json& value)
    {
        if (!Value(value))
            return false;
        SkipSpace();
        return m_position == m_text.size();
    }

private:
    void SkipSpace()
    {
        while (m_position < m_text.size() && iswspace(m_text[m_position]))
            ++m_position;
    }

    bool Literal(const wchar_t* word)
    {
        const size_t length = wcslen(word);
        if (m_text.compare(m_position, length, word) != 0)
            return false;
        m_position += length;
        return true;
    }

    bool Value(Json& value)
    {
        SkipSpace();
        if (m_position >= m_text.size() || m_depth >= kMaxJsonDepth)
            return false;
        ++m_depth;
        bool ok;
        const wchar_t character = m_text[m_position];
        if (character == L'{')
        {
            ok = Object(value);
        }
        else if (character == L'[')
        {
            ok = Array(value);
        }
        else if (character == L'"')
        {
            value.type = Json::eString;
            ok = String(value.text);
        }
        else if (Literal(L"true"))
        {
            value.type = Json::eBool;
            value.boolean = true;
            value.text = L"true";
            ok = true;
        }
        else if (Literal(L"false"))
        {
            value.type = Json::eBool;
            value.text = L"false";
            ok = true;
        }
        else if (Literal(L"null"))
        {
            value.type = Json::eNull;
            ok = true;
        }
        else
        {
            ok = Number(value);
        }
        --m_depth;
        return ok;
    }

    bool String(std::wstring& text)
    {
        ++m_position;
        while (m_position < m_text.size())
        {
            const wchar_t character = m_text[m_position++];
            if (character == L'"')
                return true;
            if (character != L'\\')
            {
                text += character;
                continue;
            }
            if (m_position >= m_text.size())
                return false;
            const wchar_t escape = m_text[m_position++];
            switch (escape)
            {
            case L'n':
                text += L'\n';
                break;
            case L't':
                text += L' ';
                break;
            case L'b':
            case L'f':
            case L'r':
                break;
            case L'u':
            {
                unsigned int code = 0;
                for (int digit = 0; digit < 4; ++digit)
                {
                    if (m_position >= m_text.size() || HexDigit(m_text[m_position]) < 0)
                        return false;
                    code = (code << 4) | static_cast<unsigned int>(HexDigit(m_text[m_position++]));
                }
                text += static_cast<wchar_t>(code);
                break;
            }
            default:
                text += escape;
                break;
            }
        }
        return false;
    }

    bool Number(Json& value)
    {
        const size_t start = m_position;
        while (m_position < m_text.size()
               && (iswdigit(m_text[m_position]) || wcschr(L"+-.eE", m_text[m_position]) != NULL))
            ++m_position;
        if (m_position == start)
            return false;
        value.type = Json::eNumber;
        value.text = m_text.substr(start, m_position - start);
        return true;
    }

    bool Array(Json& value)
    {
        ++m_position;
        value.type = Json::eArray;
        SkipSpace();
        if (m_position < m_text.size() && m_text[m_position] == L']')
        {
            ++m_position;
            return true;
        }
        for (;;)
        {
            value.items.push_back(Json());
            if (!Value(value.items.back()))
                return false;
            SkipSpace();
            if (m_position >= m_text.size())
                return false;
            const wchar_t separator = m_text[m_position++];
            if (separator == L']')
                return true;
            if (separator != L',')
                return false;
        }
    }

    bool Object(Json& value)
    {
        ++m_position;
        value.type = Json::eObject;
        SkipSpace();
        if (m_position < m_text.size() && m_text[m_position] == L'}')
        {
            ++m_position;
            return true;
        }
        for (;;)
        {
            SkipSpace();
            if (m_position >= m_text.size() || m_text[m_position] != L'"')
                return false;
            value.keys.push_back(std::wstring());
            if (!String(value.keys.back()))
                return false;
            SkipSpace();
            if (m_position >= m_text.size() || m_text[m_position++] != L':')
                return false;
            value.items.push_back(Json());
            if (!Value(value.items.back()))
                return false;
            SkipSpace();
            if (m_position >= m_text.size())
                return false;
            const wchar_t separator = m_text[m_position++];
            if (separator == L'}')
                return true;
            if (separator != L',')
                return false;
        }
    }

    const std::wstring& m_text;
    size_t m_position;
    size_t m_depth;
};

struct Style
{
    Style() : colour(kDefaultColour), bold(false), italic(false), underlined(false), strikethrough(false),
              obfuscated(false)
    {
    }

    int colour;
    bool bold;
    bool italic;
    bool underlined;
    bool strikethrough;
    bool obfuscated;
};

std::wstring StylePrefix(const Style& style)
{
    std::wstring prefix;
    if (style.colour == kDefaultColour)
    {
        prefix += kSection;
        prefix += L'r';
    }
    else if (style.colour < 16)
    {
        prefix += kSection;
        prefix += L"0123456789abcdef"[style.colour];
    }
    else
    {
        prefix += HexCode(static_cast<unsigned int>(style.colour) & 0xffffff);
    }
    const wchar_t codes[] = {L'k', L'l', L'm', L'n', L'o'};
    const bool flags[] = {style.obfuscated, style.bold, style.strikethrough, style.underlined, style.italic};
    for (int index = 0; index < 5; ++index)
    {
        if (flags[index])
        {
            prefix += kSection;
            prefix += codes[index];
        }
    }
    return prefix;
}

bool BoolField(const Json& component, const wchar_t* key, bool fallback)
{
    const Json* field = component.Find(key);
    if (field == NULL)
        return fallback;
    if (field->type == Json::eBool)
        return field->boolean;
    if (field->type == Json::eString)
        return field->text == L"true";
    return fallback;
}

int ColourField(const Json& component, int fallback)
{
    const Json* field = component.Find(L"color");
    if (field == NULL || field->type != Json::eString)
        return fallback;
    const std::wstring& name = field->text;
    unsigned int rgb;
    if (name.size() == 7 && name[0] == L'#' && ReadHex(name, 1, 1, rgb))
        return static_cast<int>(kHexColour | rgb);
    if (name == L"reset")
        return kDefaultColour;
    for (int index = 0; index < 16; ++index)
        if (name == kColourNames[index])
            return index;
    return fallback;
}

Style ComponentStyle(const Json& component, const Style& parent)
{
    if (component.type != Json::eObject)
        return parent;
    Style style;
    style.colour = ColourField(component, parent.colour);
    style.bold = BoolField(component, L"bold", parent.bold);
    style.italic = BoolField(component, L"italic", parent.italic);
    style.underlined = BoolField(component, L"underlined", parent.underlined);
    style.strikethrough = BoolField(component, L"strikethrough", parent.strikethrough);
    style.obfuscated = BoolField(component, L"obfuscated", parent.obfuscated);
    return style;
}

void AppendText(const std::wstring& text, const Style& style, std::wstring& out, std::wstring& lastPrefix)
{
    if (text.empty())
        return;
    const std::wstring prefix = StylePrefix(style);
    if (prefix != lastPrefix)
    {
        out += prefix;
        lastPrefix = prefix;
    }
    for (size_t index = 0; index < text.size(); ++index)
        if (text[index] == L'\n' || text[index] >= 0x20)
            out += text[index];
}

void AppendComponent(const Json& component, const Style& parent, std::wstring& out, std::wstring& lastPrefix)
{
    switch (component.type)
    {
    case Json::eString:
    case Json::eNumber:
    case Json::eBool:
        AppendText(component.text, parent, out, lastPrefix);
        break;
    case Json::eArray:
        if (!component.items.empty())
        {
            const Style style = ComponentStyle(component.items[0], parent);
            AppendComponent(component.items[0], parent, out, lastPrefix);
            for (size_t index = 1; index < component.items.size(); ++index)
                AppendComponent(component.items[index], style, out, lastPrefix);
        }
        break;
    case Json::eObject:
    {
        const Style style = ComponentStyle(component, parent);
        const Json* text = component.Find(L"text");
        if (text == NULL)
            text = component.Find(L"translate");
        if (text != NULL && text->type != Json::eArray && text->type != Json::eObject)
            AppendText(text->text, style, out, lastPrefix);
        const Json* extra = component.Find(L"extra");
        if (extra != NULL && extra->type == Json::eArray)
            for (size_t index = 0; index < extra->items.size(); ++index)
                AppendComponent(extra->items[index], style, out, lastPrefix);
        break;
    }
    default:
        break;
    }
}

std::wstring Legacy(const std::wstring& text)
{
    std::wstring out;
    for (size_t index = 0; index < text.size(); ++index)
    {
        const wchar_t character = text[index];
        if ((character == kSection || character == L'&') && index + 1 < text.size())
        {
            const wchar_t code = Lower(text[index + 1]);
            unsigned int rgb;
            if (code == L'#' && ReadHex(text, index + 2, 1, rgb))
            {
                out += HexCode(rgb);
                index += 7;
                continue;
            }
            if (code == L'x' && ReadHex(text, index + 3, 2, rgb))
            {
                out += HexCode(rgb);
                index += 13;
                continue;
            }
            if (IsColourCode(code) || IsFormatCode(code))
            {
                out += kSection;
                out += code;
                ++index;
                continue;
            }
        }
        if (character == L'\t')
            out += L' ';
        else if (character == L'\n' || character >= 0x20)
            out += character;
    }
    return out;
}
}

std::wstring HexCode(unsigned int rgb)
{
    std::wstring code;
    code += kSection;
    code += L'x';
    for (int shift = 20; shift >= 0; shift -= 4)
    {
        code += kSection;
        code += L"0123456789abcdef"[(rgb >> shift) & 0xf];
    }
    return code;
}

std::wstring Unescape(const std::wstring& text)
{
    std::wstring out;
    for (size_t index = 0; index < text.size(); ++index)
    {
        if (text[index] != L'\\' || index + 1 >= text.size())
        {
            out += text[index];
            continue;
        }
        const wchar_t escape = text[++index];
        unsigned int code;
        if (escape == L'n')
            out += L'\n';
        else if (escape == L't')
            out += L'\t';
        else if (escape == L'r' || escape == L'f')
            continue;
        else if (escape == L'u' && index + 4 < text.size() && HexDigit(text[index + 1]) >= 0
                 && HexDigit(text[index + 2]) >= 0 && HexDigit(text[index + 3]) >= 0
                 && HexDigit(text[index + 4]) >= 0)
        {
            code = 0;
            for (int digit = 1; digit <= 4; ++digit)
                code = (code << 4) | static_cast<unsigned int>(HexDigit(text[index + digit]));
            out += static_cast<wchar_t>(code);
            index += 4;
        }
        else
        {
            out += escape;
        }
    }
    return out;
}

std::wstring Parse(const std::wstring& text)
{
    std::wstring formatted;
    bool parsed = false;
    const size_t first = text.find_first_not_of(L" \t\r\n");
    if (first != std::wstring::npos && (text[first] == L'{' || text[first] == L'[' || text[first] == L'"'))
    {
        Json root;
        JsonParser parser(text);
        if (parser.Parse(root))
        {
            std::wstring lastPrefix;
            AppendComponent(root, Style(), formatted, lastPrefix);
            parsed = true;
        }
    }
    if (!parsed)
        formatted = Legacy(text);
    if (formatted.size() > kMaxLength)
        formatted.resize(kMaxLength);
    return HasVisibleText(formatted) ? formatted : std::wstring();
}

std::vector<std::wstring> Lines(Font& font, const std::wstring& motd, int width, size_t maxLines)
{
    std::vector<std::wstring> lines;
    std::wstring style;
    std::wstring line;
    int lineWidth = 0;
    bool bold = false;
    size_t spaceIndex = std::wstring::npos;
    std::wstring styleAtSpace;
    for (size_t index = 0; index < motd.size() && lines.size() < maxLines; ++index)
    {
        const wchar_t character = motd[index];
        if (character == L'\n')
        {
            lines.push_back(line);
            line = style;
            lineWidth = 0;
            spaceIndex = std::wstring::npos;
            continue;
        }
        if (character == kSection && index + 1 < motd.size())
        {
            const wchar_t code = Lower(motd[index + 1]);
            unsigned int rgb;
            if (code == L'x' && ReadHex(motd, index + 3, 2, rgb))
            {
                style = motd.substr(index, 14);
                line += style;
                bold = false;
                index += 13;
                continue;
            }
            const std::wstring sequence = motd.substr(index, 2);
            if (IsColourCode(code))
            {
                style = sequence;
                bold = false;
            }
            else if (IsFormatCode(code))
            {
                style += sequence;
                bold = bold || code == L'l';
            }
            line += sequence;
            ++index;
            continue;
        }

        const int characterWidth = font.width(std::wstring(1, character)) + (bold ? 1 : 0);
        if (character == L' ')
        {
            spaceIndex = line.size();
            styleAtSpace = style;
        }
        if (lineWidth > 0 && lineWidth + characterWidth > width)
        {
            if (character == L' ')
            {
                lines.push_back(line);
                line = style;
                lineWidth = 0;
                spaceIndex = std::wstring::npos;
                continue;
            }
            if (spaceIndex != std::wstring::npos)
            {
                lines.push_back(line.substr(0, spaceIndex));
                line = styleAtSpace + line.substr(spaceIndex + 1);
            }
            else
            {
                lines.push_back(line);
                line = style;
            }
            lineWidth = font.width(line);
            spaceIndex = std::wstring::npos;
            if (lines.size() >= maxLines)
                break;
        }
        line += character;
        lineWidth += characterWidth;
    }
    if (lines.size() < maxLines && HasVisibleText(line))
        lines.push_back(line);
    return lines;
}
}
