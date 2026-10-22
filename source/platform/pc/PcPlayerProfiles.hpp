#ifndef PLATFORM_PC_PCPLAYERPROFILES_HPP
#define PLATFORM_PC_PCPLAYERPROFILES_HPP

#include <string>
#include <vector>

struct PcPlayerProfile
{
    std::wstring name;
    std::wstring username;
    std::string uuid;
};

std::vector<PcPlayerProfile> PcReadPlayerProfiles();

#endif
