#ifndef PLATFORM_PLATFORMTIMING_HPP
#define PLATFORM_PLATFORMTIMING_HPP

#include "platform/Platform.hpp"

#if LCE_PLATFORM_WIIU
#include "platform/wiiu/PlatformTiming.hpp"
#elif LCE_PLATFORM_PC
#include "platform/pc/PlatformTiming.hpp"
#endif

#endif
