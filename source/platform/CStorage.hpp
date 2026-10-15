#ifndef PLATFORM_CSTORAGE_HPP
#define PLATFORM_CSTORAGE_HPP

#include "platform/Platform.hpp"

#if LCE_PLATFORM_WIIU
#include "platform/wiiu/CStorage.hpp"
#elif LCE_PLATFORM_PC
#include "platform/pc/CStorage.hpp"
#endif

#endif
