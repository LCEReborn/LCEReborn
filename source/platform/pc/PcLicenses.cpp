#include "platform/pc/PcLicenses.hpp"

#include "game/core/CConsoleMinecraftApp.hpp"
#include "game/core/StringConversion.hpp"
#include "game/core/cDLCInfo.hpp"
#include "platform/CProfile.hpp"
#include "platform/pc/Log.hpp"
#include "platform/pc/PcLicenseDefaults.hpp"
#include "platform/pc/PcPaths.hpp"
#include "platform/wiiu/cNintendoDLCInfo.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

extern CConsoleMinecraftApp gApp;

static const Logger sLog("Licenses");

struct LicenseConfig
{
    bool fullGame = false;
    bool allPacks = false;
    std::vector<std::string> packs;
};

static std::string Trim(const std::string& text)
{
    const size_t first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
        return std::string();
    const size_t last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

static std::string Lower(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

static bool ParseBool(const std::string& value, bool& result)
{
    const std::string lower = Lower(value);
    if (lower == "true" || lower == "yes" || lower == "1")
        result = true;
    else if (lower == "false" || lower == "no" || lower == "0")
        result = false;
    else
        return false;
    return true;
}

static void WriteDefaultConfig(const std::string& path)
{
    std::error_code error;
    const std::filesystem::path file = std::filesystem::u8path(path);
    if (file.has_parent_path())
        std::filesystem::create_directories(file.parent_path(), error);
    std::ofstream output(file, std::ios::binary);
    output << kPcDefaultLicenseConfig;
    if (!output)
        LOG_WARN(sLog, "%s could not be written", path.c_str());
    else
        LOG_INFO(sLog, "Wrote %s with everything locked; edit it to unlock what you own", path.c_str());
}

static LicenseConfig ReadConfig(const std::string& path)
{
    LicenseConfig config;
    std::ifstream input(std::filesystem::u8path(path), std::ios::binary);
    if (!input)
    {
        WriteDefaultConfig(path);
        return config;
    }

    std::string line;
    int lineNumber = 0;
    while (std::getline(input, line))
    {
        ++lineNumber;
        const size_t comment = line.find('#');
        if (comment != std::string::npos)
            line.erase(comment);
        line = Trim(line);
        if (line.empty())
            continue;
        const size_t equals = line.find('=');
        if (equals == std::string::npos)
        {
            LOG_WARN(sLog, "%s line %d has no '=' and is ignored", path.c_str(), lineNumber);
            continue;
        }
        const std::string key = Lower(Trim(line.substr(0, equals)));
        const std::string value = Trim(line.substr(equals + 1));
        if (key == "full_game")
        {
            if (!ParseBool(value, config.fullGame))
                LOG_WARN(sLog, "%s line %d: full_game must be true or false", path.c_str(), lineNumber);
        }
        else if (key == "dlc")
        {
            const std::string lower = Lower(value);
            if (lower == "all")
            {
                config.allPacks = true;
            }
            else if (lower != "none" && !lower.empty())
            {
                size_t start = 0;
                while (start <= value.size())
                {
                    const size_t comma = value.find(',', start);
                    const std::string name =
                        Trim(value.substr(start, comma == std::string::npos ? std::string::npos : comma - start));
                    if (!name.empty())
                        config.packs.push_back(Lower(name));
                    if (comma == std::string::npos)
                        break;
                    start = comma + 1;
                }
            }
        }
        else
        {
            LOG_WARN(sLog, "%s line %d: unknown setting %s", path.c_str(), lineNumber, key.c_str());
        }
    }
    return config;
}

void PcApplyLicenses()
{
    const std::string& path = PcLicenseFile();
    const LicenseConfig config = ReadConfig(path);

    gProfile->SetFullVersion(config.fullGame);
    if (config.fullGame)
        LOG_INFO(sLog, "%s unlocks the full game", path.c_str());
    else
        LOG_INFO(sLog, "%s leaves the full game locked; the game runs as the trial", path.c_str());

    cNintendoDLCInfo* dlcInfo = gApp.GetDLCInfoInstance();
    std::vector<bool> matched(config.packs.size(), false);
    unsigned int unlocked = 0;
    for (unsigned int position = 0; position < dlcInfo->GetDLCInfoCount(); ++position)
    {
        DLC_INFO* info = dlcInfo->GetDLCInfoByIndex(position);
        if (info == NULL)
            continue;
        bool wanted = config.allPacks;
        const std::string name = Lower(info->PackName);
        for (size_t index = 0; index < config.packs.size(); ++index)
        {
            if (config.packs[index] == name)
            {
                matched[index] = true;
                wanted = true;
            }
        }
        if (!wanted || !dlcInfo->SetDLCPurchased(info->CatalogIndex, true))
            continue;
        std::wstring packName = convStringToWstring(std::string(info->PackName));
        gApp.HandleDLCLicenseChange(packName);
        ++unlocked;
    }
    for (size_t index = 0; index < config.packs.size(); ++index)
    {
        if (!matched[index])
            LOG_WARN(sLog, "%s names a pack the game doesn't have: %s", path.c_str(), config.packs[index].c_str());
    }
    LOG_INFO(sLog, "%u add-on packs unlocked by %s", unlocked, path.c_str());
}
