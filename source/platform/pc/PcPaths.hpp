#ifndef PLATFORM_PC_PCPATHS_HPP
#define PLATFORM_PC_PCPATHS_HPP

#include <string>

void PcSetContentRoot(const std::string& path);
void PcSetSaveRoot(const std::string& path);
const std::string& PcContentRoot();
const std::string& PcSaveRoot();
const std::string& PcResourceRoot();
const std::string& PcLicenseFile();
bool PcCheckGameFolders(std::string& message, bool needInterfaceArt = true);

#endif
