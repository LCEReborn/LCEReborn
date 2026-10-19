#ifndef PLATFORM_PC_PCLAUNCHOPTIONS_HPP
#define PLATFORM_PC_PCLAUNCHOPTIONS_HPP

#include <string>

struct PcLaunchOptions
{
    std::string username;
    std::string contentDir;
    std::string saveDir;
    std::string resourceDir;
    std::string language;
    std::string logDir;
    std::string uuid;
    std::string licenseFile;
    bool splitScreen = true;
    bool joinPrompt = false;
};

bool PcParseLaunchOptions(int argc, char** argv, int& exitCode);
const PcLaunchOptions& PcGetLaunchOptions();

#endif
