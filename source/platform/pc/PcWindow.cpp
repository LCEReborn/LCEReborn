#include "platform/pc/PcWindow.hpp"

#include <SDL3/SDL.h>

#include <stdio.h>

#include "platform/CInput.hpp"
#include "platform/pc/PcApplication.hpp"
#include "platform/pc/PcInputEvents.hpp"

namespace
{
SDL_Window* sWindow = NULL;
SDL_GLContext sContext = NULL;
bool sShown = false;
}

bool PcWindowCreate(int width, int height)
{
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD | SDL_INIT_EVENTS))
    {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 0);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0);

    sWindow = SDL_CreateWindow("Minecraft: Wii U Edition", width, height,
                               SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN
                                   | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (sWindow == NULL)
    {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return false;
    }

    sContext = SDL_GL_CreateContext(sWindow);
    if (sContext == NULL)
    {
        fprintf(stderr, "SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(sWindow);
        sWindow = NULL;
        return false;
    }

    SDL_GL_MakeCurrent(sWindow, sContext);
    SDL_GL_SetSwapInterval(1);
    sShown = false;
    return true;
}

void PcWindowDestroy()
{
    if (sContext != NULL)
    {
        SDL_GL_DestroyContext(sContext);
        sContext = NULL;
    }

    if (sWindow != NULL)
    {
        SDL_DestroyWindow(sWindow);
        sWindow = NULL;
    }
}

void PcWindowShow()
{
    if (sWindow != NULL && !sShown)
    {
        SDL_ShowWindow(sWindow);
        sShown = true;
    }
}

void PcWindowSwap()
{
    if (sWindow != NULL)
    {
        SDL_GL_SwapWindow(sWindow);
    }
}

void PcWindowGetDrawableSize(int* width, int* height)
{
    int drawableWidth = 0;
    int drawableHeight = 0;

    if (sWindow != NULL)
    {
        SDL_GetWindowSizeInPixels(sWindow, &drawableWidth, &drawableHeight);
    }

    *width = drawableWidth;
    *height = drawableHeight;
}

void* PcWindowGetProcAddress(const char* name)
{
    return reinterpret_cast<void*>(SDL_GL_GetProcAddress(name));
}

void PcPumpEvents()
{
    SDL_Event event;

    while (SDL_PollEvent(&event))
    {
        switch (event.type)
        {
        case SDL_EVENT_QUIT:
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            PcRequestExit();
            break;
        case SDL_EVENT_WINDOW_FOCUS_GAINED:
            if (g_pInput != NULL)
            {
                g_pInput->AcquiredForeground();
            }
            break;
        default:
            break;
        }

        PcInputHandleEvent(event);
    }
}
