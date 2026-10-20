#include "platform/pc/PcSystemInfo.hpp"

#include <cstring>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#include <intrin.h>
#else
#include <unistd.h>

#include <cstdio>
#include <fstream>
#endif

#if defined(_WIN32)

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
#else
bool PcMemoryUsage(unsigned long long* used, unsigned long long* total)
{
    std::FILE* file = std::fopen("/proc/self/statm", "r");
    if (!file)
        return false;
    unsigned long long pages = 0;
    unsigned long long resident = 0;
    const bool read = std::fscanf(file, "%llu %llu", &pages, &resident) == 2;
    std::fclose(file);
    const long pageSize = sysconf(_SC_PAGESIZE);
    const long physicalPages = sysconf(_SC_PHYS_PAGES);
    if (!read || pageSize <= 0 || physicalPages <= 0)
        return false;
    *used = resident * static_cast<unsigned long long>(pageSize);
    *total = static_cast<unsigned long long>(physicalPages) * static_cast<unsigned long long>(pageSize);
    return true;
}

std::string PcCpuName()
{
    std::ifstream file("/proc/cpuinfo");
    std::string line;
    while (std::getline(file, line))
    {
        if (line.compare(0, 10, "model name") != 0)
            continue;
        const std::string::size_type colon = line.find(':');
        if (colon == std::string::npos)
            continue;
        const std::string::size_type first = line.find_first_not_of(" \t", colon + 1);
        const std::string::size_type last = line.find_last_not_of(" \t");
        return first == std::string::npos ? std::string() : line.substr(first, last - first + 1);
    }
    return std::string();
}
#endif
