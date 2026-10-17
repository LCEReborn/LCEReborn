#include "platform/pc/PcSystemLocale.hpp"

#include <cstdlib>

#include "platform/pc/PcLaunchOptions.hpp"

#include "platform/pc/PcLaunchOptions.hpp"

#if defined(_WIN32)
#include <windows.h>
#endif

std::string PcSystemLocaleName()
{
    const std::string& configured = PcGetLaunchOptions().language;
    if (!configured.empty())
        return configured;
#if defined(_WIN32)
    wchar_t name[LOCALE_NAME_MAX_LENGTH];
    if (GetUserDefaultLocaleName(name, LOCALE_NAME_MAX_LENGTH) > 0)
    {
        std::string result;
        for (const wchar_t* character = name; *character != L'\0'; ++character)
            result += static_cast<char>(*character);
        return result;
    }
    return std::string();
#else
    const char* variables[] = {"LC_ALL", "LC_MESSAGES", "LANG"};
    for (const char* variable : variables)
    {
        const char* value = std::getenv(variable);
        if (value != nullptr && value[0] != '\0')
            return value;
    }
    return std::string();
#endif
}
