#include "platform/pc/PcPlayerProfiles.hpp"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>

#include "platform/pc/Log.hpp"
#include "platform/pc/PcJson.hpp"
#include "platform/pc/PcLaunchOptions.hpp"

namespace
{
const Logger sLog("Profiles");

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
    if (!PcJsonParse(source, root) || root.type != JsonValue::Object)
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
