#include "vendor/MilesTime.hpp"

#include "platform/Platform.hpp"

#if LCE_PLATFORM_WIIU

#include <coreinit/systeminfo.h>
#include <coreinit/thread.h>
#include <coreinit/time.h>

#include "vendor/MilesRR.hpp"

static const unsigned int RR_TIME_UNINITIALISED = 0;
static const unsigned int RR_TIME_READY = 1;
static const unsigned int RR_TIME_CLAIMED = 2;

static const unsigned int RR_TIME_SPINS_BEFORE_SLEEP = 302;
static const unsigned int RR_TIME_SPINS_BEFORE_YIELD = 300;
static const unsigned long long RR_TIME_SPIN_SLEEP_MICROSECONDS = 1000;

static unsigned int rrTimeState = RR_TIME_UNINITIALISED;

static unsigned long long rrTimeBase;
static unsigned long long rrTickBase;

void GetBases()
{
    unsigned int uSpins;
    unsigned int uPrevious;

    uSpins = 0;

    for (;;)
    {
        uPrevious = rrAtomicCmpXchg32(&rrTimeState, RR_TIME_CLAIMED, RR_TIME_UNINITIALISED);

        if (uPrevious == RR_TIME_READY)
        {
            return;
        }

        if (uPrevious == RR_TIME_UNINITIALISED)
        {
            break;
        }

        if (uSpins > RR_TIME_SPINS_BEFORE_SLEEP)
        {
            uSpins = 0;
            OSSleepTicks((OSTime)rrMicrosToTime(RR_TIME_SPIN_SLEEP_MICROSECONDS));
        }
        else
        {
            uSpins = uSpins + 1;

            if (uSpins > RR_TIME_SPINS_BEFORE_YIELD)
            {
                OSYieldThread();
            }
        }
    }

    rrTickBase = (unsigned int)OSGetTick();
    rrTimeBase = OSGetTime();

    rrAtomicStoreRelease32(&rrTimeState, RR_TIME_READY);
}

extern "C" unsigned long long rrGetTime()
{
    if (rrAtomicLoadAcquire32(&rrTimeState) != RR_TIME_READY)
    {
        GetBases();
    }

    return (unsigned long long)OSGetTime() - rrTimeBase;
}

extern "C" unsigned long long rrTimeToMillis(unsigned long long uTime)
{
    return (uTime * 1000) / (OSGetSystemInfo()->busClockSpeed >> 2);
}

extern "C" unsigned long long rrTimeToMicros(unsigned long long uTime)
{
    return (uTime * 1000000) / (OSGetSystemInfo()->busClockSpeed >> 2);
}

extern "C" unsigned long long rrMillisToTime(unsigned long long uMilliseconds)
{
    return (uMilliseconds * (OSGetSystemInfo()->busClockSpeed >> 2)) / 1000;
}

extern "C" unsigned long long rrMicrosToTime(unsigned long long uMicroseconds)
{
    return (uMicroseconds * (OSGetSystemInfo()->busClockSpeed >> 2)) / 1000000;
}

#elif LCE_PLATFORM_PC

#include <chrono>

#include "vendor/MilesRR.hpp"

static const unsigned int RR_TIME_UNINITIALISED = 0;
static const unsigned int RR_TIME_READY = 1;
static const unsigned int RR_TIME_CLAIMED = 2;

static const unsigned long long RR_TIME_UNITS_PER_SECOND = 1000000000ULL;

static unsigned int rrTimeState = RR_TIME_UNINITIALISED;

static std::chrono::steady_clock::time_point rrTimeBase;

static unsigned long long ElapsedTime()
{
    return static_cast<unsigned long long>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - rrTimeBase).count());
}

void GetBases()
{
    unsigned int uPrevious;

    for (;;)
    {
        uPrevious = rrAtomicCmpXchg32(&rrTimeState, RR_TIME_CLAIMED, RR_TIME_UNINITIALISED);

        if (uPrevious == RR_TIME_READY)
        {
            return;
        }

        if (uPrevious == RR_TIME_UNINITIALISED)
        {
            break;
        }

        rrThreadSleep(0);
    }

    rrTimeBase = std::chrono::steady_clock::now();

    rrAtomicStoreRelease32(&rrTimeState, RR_TIME_READY);
}

extern "C" unsigned long long rrGetTime()
{
    if (rrAtomicLoadAcquire32(&rrTimeState) != RR_TIME_READY)
    {
        GetBases();
    }

    return ElapsedTime();
}

extern "C" unsigned long long rrTimeToMillis(unsigned long long uTime)
{
    return uTime / (RR_TIME_UNITS_PER_SECOND / 1000);
}

extern "C" unsigned long long rrTimeToMicros(unsigned long long uTime)
{
    return uTime / (RR_TIME_UNITS_PER_SECOND / 1000000);
}

extern "C" unsigned long long rrMillisToTime(unsigned long long uMilliseconds)
{
    return uMilliseconds * (RR_TIME_UNITS_PER_SECOND / 1000);
}

extern "C" unsigned long long rrMicrosToTime(unsigned long long uMicroseconds)
{
    return uMicroseconds * (RR_TIME_UNITS_PER_SECOND / 1000000);
}

#endif
