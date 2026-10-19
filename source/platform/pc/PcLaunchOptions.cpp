#include "platform/pc/PcLaunchOptions.hpp"

#include <cctype>
#include <cstdio>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#include <shellapi.h>
#endif

static PcLaunchOptions sOptions;

static std::vector<std::string> CommandLineArguments(int argc, char** argv)
{
    std::vector<std::string> arguments;
#if defined(_WIN32)
    (void)argc;
    (void)argv;
    int count = 0;
    LPWSTR* wide = CommandLineToArgvW(GetCommandLineW(), &count);
    if (wide == NULL)
        return arguments;
    for (int index = 1; index < count; ++index)
    {
        int length = WideCharToMultiByte(CP_UTF8, 0, wide[index], -1, NULL, 0, NULL, NULL);
        std::string argument(length > 0 ? length - 1 : 0, '\0');
        if (length > 1)
            WideCharToMultiByte(CP_UTF8, 0, wide[index], -1, &argument[0], length, NULL, NULL);
        arguments.push_back(argument);
    }
    LocalFree(wide);
#else
    for (int index = 1; index < argc; ++index)
        arguments.push_back(argv[index]);
#endif
    return arguments;
}

static void PrintUsage(std::FILE* stream)
{
    std::fputs("Usage: LCEReborn [options]\n"
               "  --username <name>           player name\n"
               "  --uuid <uuid>               player identity, 32 hex digits (default: made from the name)\n"
               "  --content-dir <dir>         game content (default: assets in the working folder)\n"
               "  --save-dir <dir>            saves and settings (default: saves in the working folder)\n"
               "  --resource-dir <dir>        PC interface art (default: resources in the working folder)\n"
               "  --language <code>           language, such as en-GB or ja (default: the system language)\n"
               "  --log-dir <dir>             logs (default: logs in the working folder)\n"
               "  --license-file <file>       what is unlocked (default: license.cfg in the working folder)\n"
               "  --split-screen <on|off>     other controllers can join as more players (default: on)\n"
               "  --join-prompt <on|off>      prompt other controllers to join when a world loads (default: off)\n"
               "  --help                      show this message\n",
               stream);
}

bool PcParseLaunchOptions(int argc, char** argv, int& exitCode)
{
    exitCode = 1;
    const std::vector<std::string> arguments = CommandLineArguments(argc, argv);

    for (std::vector<std::string>::size_type index = 0; index < arguments.size(); ++index)
    {
        std::string name = arguments[index];
        std::string value;
        bool hasValue = false;

        const std::string::size_type equals = name.find('=');
        if (name.compare(0, 2, "--") == 0 && equals != std::string::npos)
        {
            value = name.substr(equals + 1);
            name.resize(equals);
            hasValue = true;
        }

        if (name == "--help" || name == "-h")
        {
            PrintUsage(stdout);
            exitCode = 0;
            return false;
        }

        std::string* target = NULL;
        bool* flag = NULL;
        if (name == "--split-screen")
            flag = &sOptions.splitScreen;
        else if (name == "--join-prompt")
            flag = &sOptions.joinPrompt;
        else if (name == "--username")
            target = &sOptions.username;
        else if (name == "--content-dir")
            target = &sOptions.contentDir;
        else if (name == "--save-dir")
            target = &sOptions.saveDir;
        else if (name == "--resource-dir")
            target = &sOptions.resourceDir;
        else if (name == "--language")
            target = &sOptions.language;
        else if (name == "--log-dir")
            target = &sOptions.logDir;
        else if (name == "--uuid")
            target = &sOptions.uuid;
        else if (name == "--license-file")
            target = &sOptions.licenseFile;
        else
        {
            std::fprintf(stderr, "Unknown option: %s\n", arguments[index].c_str());
            PrintUsage(stderr);
            return false;
        }

        if (!hasValue)
        {
            if (index + 1 >= arguments.size())
            {
                std::fprintf(stderr, "Missing value for %s\n", name.c_str());
                PrintUsage(stderr);
                return false;
            }
            value = arguments[++index];
        }

        if (value.empty())
        {
            std::fprintf(stderr, "Empty value for %s\n", name.c_str());
            return false;
        }

        if (flag != NULL)
        {
            if (value == "on")
                *flag = true;
            else if (value == "off")
                *flag = false;
            else
            {
                std::fprintf(stderr, "%s must be on or off, not %s\n", name.c_str(), value.c_str());
                return false;
            }
            continue;
        }

        *target = value;
    }

    if (!sOptions.uuid.empty())
    {
        std::string digits;
        for (char character : sOptions.uuid)
        {
            if (character == '-')
                continue;
            if (!std::isxdigit(static_cast<unsigned char>(character)))
            {
                digits.clear();
                break;
            }
            digits += static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
        }
        if (digits.size() != 32)
        {
            std::fprintf(stderr, "--uuid must be 32 hexadecimal digits, not %s\n", sOptions.uuid.c_str());
            return false;
        }
        sOptions.uuid = digits;
    }

    return true;
}

const PcLaunchOptions& PcGetLaunchOptions()
{
    return sOptions;
}
