#ifndef PLATFORM_PC_PCJSON_HPP
#define PLATFORM_PC_PCJSON_HPP

#include <map>
#include <string>
#include <vector>

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

bool PcJsonParse(const std::string& source, JsonValue& value);
std::string PcJsonQuote(const std::wstring& text);

#endif
