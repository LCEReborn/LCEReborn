#ifndef PLATFORM_C4JTHREADIMPL_HPP
#define PLATFORM_C4JTHREADIMPL_HPP

#include "platform/Platform.hpp"

#if LCE_PLATFORM_WIIU
#include "platform/wiiu/C4JThreadImpl.hpp"
#elif LCE_PLATFORM_PC
#include "platform/pc/C4JThreadImpl.hpp"
#endif

#endif
