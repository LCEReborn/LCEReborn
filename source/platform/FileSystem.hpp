#ifndef PLATFORM_FILESYSTEM_HPP
#define PLATFORM_FILESYSTEM_HPP

#include "platform/Platform.hpp"

#if LCE_PLATFORM_WIIU
#include "platform/wiiu/NintendoFileSystem_WiiU.hpp"
#elif LCE_PLATFORM_PC
#include "platform/pc/FileApi.hpp"
#endif

#endif
