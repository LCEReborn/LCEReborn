#ifndef PLATFORM_PC_PCSYSTEMINFO_HPP
#define PLATFORM_PC_PCSYSTEMINFO_HPP

#include <string>

bool PcMemoryUsage(unsigned long long* used, unsigned long long* total);
std::string PcCpuName();

#endif
