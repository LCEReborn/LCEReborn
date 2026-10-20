#include "platform/pc/Runtime.hpp"

#include <chrono>
#include <csignal>
#include <ctime>

#if !defined(_WIN32)
void __debugbreak()
{
    std::raise(SIGTRAP);
}
#endif

unsigned int FileTimeToSystemTime(const FILETIME*, SYSTEMTIME*)
{
    return 0;
}

unsigned int GetLastFileErrorCodeForViewer()
{
    return 0;
}

unsigned int GetLastError()
{
    return 0;
}

void GlobalMemoryStatus(MEMORYSTATUS* status)
{
    status->dwTotalPhys = 0;
    status->dwAvailPhys = 0;
    status->dwAvailVirtual = 0;
}

void GlobalMemoryStatus(_MEMORYSTATUS* status)
{
    status->dwTotalPhys = 0;
    status->dwAvailPhys = 0;
    status->dwAvailVirtual = 0;
}

unsigned int GetTickCount()
{
    return static_cast<unsigned int>(
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

void GetSystemTime(SYSTEMTIME* systemTime)
{
    const std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
    const std::time_t seconds = std::chrono::system_clock::to_time_t(now);
    std::tm calendar;
#if defined(_WIN32)
    localtime_s(&calendar, &seconds);
#else
    localtime_r(&seconds, &calendar);
#endif
    const long long milliseconds =
        std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000;
    systemTime->year = static_cast<unsigned short>(calendar.tm_year + 1900);
    systemTime->month = static_cast<unsigned short>(calendar.tm_mon + 1);
    systemTime->dayOfWeek = static_cast<unsigned short>(calendar.tm_wday);
    systemTime->day = static_cast<unsigned short>(calendar.tm_mday);
    systemTime->hour = static_cast<unsigned short>(calendar.tm_hour);
    systemTime->minute = static_cast<unsigned short>(calendar.tm_min);
    systemTime->second = static_cast<unsigned short>(calendar.tm_sec);
    systemTime->milliseconds = static_cast<unsigned short>(milliseconds);
}

void GetLocalTime(SYSTEMTIME* systemTime)
{
    GetSystemTime(systemTime);
}

void SetThreadName(unsigned int, const char*)
{
}

long InterlockedCompareExchangeRelease(volatile long* destination, long exchange, long comparand)
{
    __atomic_compare_exchange_n(destination, &comparand, exchange, false, __ATOMIC_RELEASE, __ATOMIC_RELAXED);
    return comparand;
}

long long InterlockedCompareExchangeRelease64(volatile long long* destination, long long exchange, long long comparand)
{
    __atomic_compare_exchange_n(destination, &comparand, exchange, false, __ATOMIC_RELEASE, __ATOMIC_RELAXED);
    return comparand;
}
