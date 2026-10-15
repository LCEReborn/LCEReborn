#ifndef PLATFORM_CINPUT_HPP
#define PLATFORM_CINPUT_HPP

#include "platform/Platform.hpp"

#if LCE_PLATFORM_WIIU
#include "platform/wiiu/CInput.hpp"
#elif LCE_PLATFORM_PC
#include "platform/pc/CInput.hpp"
#endif

#endif
