#include "platform/pc/PcStopSignals.hpp"

#include <atomic>
#include <csignal>

#if defined(_WIN32)
#include <windows.h>
#endif

static std::atomic<bool> s_stopSignalled(false);

static void HandleSignal(int)
{
    s_stopSignalled = true;
}

#if defined(_WIN32)
static BOOL WINAPI HandleConsoleControl(DWORD)
{
    s_stopSignalled = true;
    return TRUE;
}
#endif

void PcInstallStopSignals()
{
    std::signal(SIGINT, HandleSignal);
    std::signal(SIGTERM, HandleSignal);
#if defined(_WIN32)
    SetConsoleCtrlHandler(HandleConsoleControl, TRUE);
#endif
}

bool PcStopSignalled()
{
    return s_stopSignalled;
}
