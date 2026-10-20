#include "platform/pc/PcSystemInfo.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#include <intrin.h>

#include <cstring>

bool PcMemoryUsage(unsigned long long* used, unsigned long long* total)
{
    PROCESS_MEMORY_COUNTERS counters;
    std::memset(&counters, 0, sizeof(counters));
    counters.cb = sizeof(counters);
    if (!K32GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters)))
        return false;
    MEMORYSTATUSEX status;
    std::memset(&status, 0, sizeof(status));
    status.dwLength = sizeof(status);
    if (!GlobalMemoryStatusEx(&status))
        return false;
    *used = counters.WorkingSetSize;
    *total = status.ullTotalPhys;
    return true;
}

std::string PcCpuName()
{
    int registers[4] = {0, 0, 0, 0};
    __cpuid(registers, static_cast<int>(0x80000000u));
    if (static_cast<unsigned int>(registers[0]) < 0x80000004u)
        return std::string();
    char brand[49];
    std::memset(brand, 0, sizeof(brand));
    for (unsigned int leaf = 0; leaf < 3; ++leaf)
    {
        __cpuid(registers, static_cast<int>(0x80000002u + leaf));
        std::memcpy(brand + leaf * 16, registers, sizeof(registers));
    }
    std::string name(brand);
    const std::string::size_type first = name.find_first_not_of(' ');
    const std::string::size_type last = name.find_last_not_of(' ');
    return first == std::string::npos ? std::string() : name.substr(first, last - first + 1);
}
