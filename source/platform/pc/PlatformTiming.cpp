#include "platform/pc/PlatformTiming.hpp"

#include <chrono>

unsigned int QueryPerformanceCounter(_LARGE_INTEGER* value)
{
    value->QuadPart = std::chrono::duration_cast<std::chrono::microseconds>(
                          std::chrono::steady_clock::now().time_since_epoch())
                          .count();
    return 1;
}

unsigned int QueryPerformanceFrequency(_LARGE_INTEGER* value)
{
    value->QuadPart = 1000000;
    return 1;
}
