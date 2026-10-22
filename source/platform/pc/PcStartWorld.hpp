#ifndef PLATFORM_PC_PCSTARTWORLD_HPP
#define PLATFORM_PC_PCSTARTWORLD_HPP

#include <string>
#include <vector>

class _NetworkGameInitData;

void PcStartWorldTick();
bool PcInWorld();
bool PcSaveFolderExists(const std::string& folder);
bool PcReadSaveFile(const std::string& folder, const char* file, std::vector<unsigned char>& data, std::string& error);
_NetworkGameInitData* PcPrepareWorldLoad(const std::string& folder, std::string& error);

#endif
