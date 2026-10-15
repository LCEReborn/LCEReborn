#ifndef PLATFORM_TLSSTORAGE_HPP
#define PLATFORM_TLSSTORAGE_HPP

#include "platform/Platform.hpp"

#if LCE_PLATFORM_WIIU
#include "platform/wiiu/TLSStorageCafe.hpp"
#elif LCE_PLATFORM_PC
#include "platform/pc/TLSStorage.hpp"
#endif

#endif
