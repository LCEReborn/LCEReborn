#include "platform/pc/PcSystemLocale.hpp"

#include <cstdlib>

#if defined(_WIN32)
#include <windows.h>
#endif

std::string PcSystemLocaleName()
{
    const char* configured = std::getenv("LCE_LANGUAGE");
    if (configured != nullptr && configured[0] != '\0')
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
