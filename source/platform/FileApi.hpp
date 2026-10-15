#ifndef PLATFORM_FILEAPI_HPP
#define PLATFORM_FILEAPI_HPP

#include "platform/Platform.hpp"

#if LCE_PLATFORM_WIIU
#include "platform/wiiu/WiiU_FileCache.hpp"
#elif LCE_PLATFORM_PC
#include "platform/pc/FileApi.hpp"
#endif

#endif
