#ifndef PLATFORM_RUNTIME_HPP
#define PLATFORM_RUNTIME_HPP

#include "platform/Platform.hpp"

#if LCE_PLATFORM_WIIU
#include "platform/wiiu/WiiU_Runtime.hpp"
#elif LCE_PLATFORM_PC
#include "platform/pc/Runtime.hpp"
#endif

#endif
