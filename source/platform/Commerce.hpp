#ifndef PLATFORM_COMMERCE_HPP
#define PLATFORM_COMMERCE_HPP

#include "platform/Platform.hpp"

#if LCE_PLATFORM_WIIU
#include "platform/wiiu/cCommerceNintendoWiiU.hpp"
#elif LCE_PLATFORM_PC
#include "game/core/cCommerce.hpp"
#endif

#endif
