#ifndef GAME_CLIENT_RENDER_RENDERER_HPP
#define GAME_CLIENT_RENDER_RENDERER_HPP

#include "platform/Platform.hpp"

#if LCE_PLATFORM_WIIU
#include "platform/wiiu/Renderer.hpp"
#elif LCE_PLATFORM_PC
#include "platform/pc/Renderer.hpp"
#endif

#endif
