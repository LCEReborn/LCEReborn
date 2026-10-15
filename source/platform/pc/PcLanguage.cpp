#include "game/core/CConsoleMinecraftApp.hpp"
#include "game/core/GameSettings.hpp"
#include "game/core/XLanguage.hpp"

#include "platform/pc/PcSystemLocale.hpp"

#include <cctype>
#include <string>

extern CConsoleMinecraftApp gApp;

enum ESystemLanguage
{
    eSystemLanguage_Japanese,
    eSystemLanguage_English,
    eSystemLanguage_French,
    eSystemLanguage_German,
    eSystemLanguage_Italian,
    eSystemLanguage_Spanish,
    eSystemLanguage_SimplifiedChinese,
    eSystemLanguage_Korean,
    eSystemLanguage_Dutch,
    eSystemLanguage_Portuguese,
    eSystemLanguage_Russian,
    eSystemLanguage_TraditionalChinese,
    eSystemLanguage_Other
};

static ESystemLanguage SystemLanguage()
{
    std::string name = PcSystemLocaleName();
    for (char& character : name)
        character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    const std::string language = name.substr(0, name.find_first_of("-_."));
    if (language == "ja")
        return eSystemLanguage_Japanese;
    if (language == "en")
        return eSystemLanguage_English;
    if (language == "fr")
        return eSystemLanguage_French;
    if (language == "de")
        return eSystemLanguage_German;
    if (language == "it")
        return eSystemLanguage_Italian;
    if (language == "es")
        return eSystemLanguage_Spanish;
    if (language == "ko")
        return eSystemLanguage_Korean;
    if (language == "nl")
        return eSystemLanguage_Dutch;
    if (language == "pt")
        return eSystemLanguage_Portuguese;
    if (language == "ru")
        return eSystemLanguage_Russian;
    if (language == "zh")
    {
        const bool traditional = name.find("hant") != std::string::npos || name.find("tw") != std::string::npos ||
                                 name.find("hk") != std::string::npos || name.find("mo") != std::string::npos;
        return traditional ? eSystemLanguage_TraditionalChinese : eSystemLanguage_SimplifiedChinese;
    }
    return eSystemLanguage_Other;
}

unsigned int XGetLanguage()
{
    unsigned int language = GameSettings::GetMinecraftLanguage(0);
    if (language != 0)
        return language;
    switch (SystemLanguage())
    {
    case eSystemLanguage_Japanese:
        return XC_LANGUAGE_JAPANESE;
    case eSystemLanguage_English:
        return XC_LANGUAGE_ENGLISH;
    case eSystemLanguage_French:
        return XC_LANGUAGE_FRENCH;
    case eSystemLanguage_German:
        return XC_LANGUAGE_GERMAN;
    case eSystemLanguage_Italian:
        return XC_LANGUAGE_ITALIAN;
    case eSystemLanguage_Spanish:
        return XC_LANGUAGE_SPANISH;
    case eSystemLanguage_SimplifiedChinese:
        return XC_LANGUAGE_ZH_HANS;
    case eSystemLanguage_Korean:
        return XC_LANGUAGE_KOREAN;
    case eSystemLanguage_Dutch:
        return XC_LANGUAGE_DUTCH;
    case eSystemLanguage_Portuguese:
        return XC_LANGUAGE_PORTUGUESE;
    case eSystemLanguage_Russian:
        return XC_LANGUAGE_RUSSIAN;
    case eSystemLanguage_TraditionalChinese:
        return XC_LANGUAGE_TCHINESE;
    default:
        return XC_LANGUAGE_ENGLISH;
    }
}

int XGetLocale()
{
    int minecraftLocale = GameSettings::GetMinecraftLocale(0);
    if (minecraftLocale != 0)
        return minecraftLocale;
    switch (SystemLanguage())
    {
    case eSystemLanguage_Japanese:
        return XC_LOCALE_JAPAN;
    case eSystemLanguage_French:
        return gApp.IsAmericanSKU() ? XC_LOCALE_CANADA : XC_LOCALE_FRANCE;
    case eSystemLanguage_German:
        return XC_LOCALE_GERMANY;
    case eSystemLanguage_Italian:
        return XC_LOCALE_ITALY;
    case eSystemLanguage_Spanish:
        return gApp.IsAmericanSKU() ? XC_LOCALE_MEXICO : XC_LOCALE_SPAIN;
    case eSystemLanguage_SimplifiedChinese:
        return XC_LOCALE_CHINA;
    case eSystemLanguage_Korean:
        return XC_LOCALE_KOREA;
    case eSystemLanguage_Dutch:
        return XC_LOCALE_NETHERLANDS;
    case eSystemLanguage_Portuguese:
        return gApp.IsAmericanSKU() ? XC_LOCALE_BRAZIL : XC_LOCALE_PORTUGAL;
    case eSystemLanguage_Russian:
        return XC_LOCALE_RUSSIAN_FEDERATION;
    case eSystemLanguage_TraditionalChinese:
        return XC_LOCALE_TAIWAN;
    default:
        return XC_LOCALE_UNITED_STATES;
    }
}
