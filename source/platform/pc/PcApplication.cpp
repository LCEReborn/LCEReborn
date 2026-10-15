#include "platform/pc/PcApplication.hpp"

#include <atomic>

static std::atomic<bool> s_exitRequested(false);

void PcRequestExit()
{
    s_exitRequested = true;
}

bool PcExitRequested()
{
    return s_exitRequested;
}
