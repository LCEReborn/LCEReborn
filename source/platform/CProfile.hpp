#ifndef PLATFORM_CPROFILE_HPP
#define PLATFORM_CPROFILE_HPP

#include "platform/Platform.hpp"

#if LCE_PLATFORM_WIIU
#include "platform/wiiu/CProfile.hpp"
#elif LCE_PLATFORM_PC
#include "platform/pc/CProfile.hpp"
#endif

#endif
