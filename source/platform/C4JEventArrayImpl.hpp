#ifndef PLATFORM_C4JEVENTARRAYIMPL_HPP
#define PLATFORM_C4JEVENTARRAYIMPL_HPP

#include "platform/Platform.hpp"

#if LCE_PLATFORM_WIIU
#include "platform/wiiu/C4JEventArrayImpl.hpp"
#elif LCE_PLATFORM_PC
#include "platform/pc/C4JEventArrayImpl.hpp"
#endif

#endif
