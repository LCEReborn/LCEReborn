#ifndef PLATFORM_C4JEVENTIMPL_HPP
#define PLATFORM_C4JEVENTIMPL_HPP

#include "platform/Platform.hpp"

#if LCE_PLATFORM_WIIU
#include "platform/wiiu/C4JEventImpl.hpp"
#elif LCE_PLATFORM_PC
#include "platform/pc/C4JEventImpl.hpp"
#endif

#endif
