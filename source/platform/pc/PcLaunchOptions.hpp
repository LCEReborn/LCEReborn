#ifndef PLATFORM_PC_PCLAUNCHOPTIONS_HPP
#define PLATFORM_PC_PCLAUNCHOPTIONS_HPP

#include <string>
#if LCE_DEBUG_BUILD
#include <vector>
#endif

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
    std::string world;
#if LCE_DEBUG_BUILD
    std::vector<double> position;
    std::vector<double> rotation;
    std::string captureDir = "capture";
    int captureFrames = 0;
    double captureDelay = 10.0;
#endif
    bool splitScreen = true;
    bool joinPrompt = false;
};

bool PcParseLaunchOptions(int argc, char** argv, int& exitCode);
const PcLaunchOptions& PcGetLaunchOptions();

#endif
