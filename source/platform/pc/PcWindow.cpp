#include "platform/pc/PcWindow.hpp"
#include "platform/pc/PcGpu.hpp"

#include <SDL3/SDL.h>
#include <stb_image.h>

#include <stdio.h>

#include <fstream>
#include <sstream>
#include <string>

#include "BuildInfo.hpp"
#include "platform/CInput.hpp"
#include "platform/pc/Log.hpp"
#include "platform/pc/PcApplication.hpp"
#include "platform/pc/PcInputEvents.hpp"
#include "platform/pc/PcLaunchOptions.hpp"
#include "platform/pc/PcPaths.hpp"

namespace
{
const Logger sLog("Window");
const Logger sSdlLog("SDL");
SDL_Window* sWindow = NULL;
bool sShown = false;
bool sFullscreen = false;
bool sVSync = true;
Uint64 sNextFrame = 0;
const Uint64 kFramePeriod = SDL_NS_PER_SECOND / 60;
// With VSync on a display of about 60 Hz the display paces the frames and the
// timer is only a cap a little above it. Pacing to both clocks lets them drift
// apart, which moves the wait for the display to after the frame's input.
const float kVSyncPacedRefresh = 61.0f;
const Uint64 kVSyncCapPeriod = SDL_NS_PER_SECOND / 62;

std::string WindowTitle()
{
    static const char* const kMonths[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                          "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    std::string title = std::string("LCEReborn - ") + LCE_BUILD_COMMIT + (LCE_BUILD_DIRTY ? " (modified)" : "");
    int year = 0;
    int month = 0;
    int day = 0;
    if (sscanf(LCE_BUILD_DATE, "%d-%d-%d", &year, &month, &day) == 3 && month >= 1 && month <= 12)
    {
        char date[32];
        snprintf(date, sizeof(date), ", %d %s %d", day, kMonths[month - 1], year);
        title += date;
    }
    return title;
}

void SdlLog(void*, int, SDL_LogPriority priority, const char* message)
{
    LogLevel level = priority >= SDL_LOG_PRIORITY_CRITICAL ? LogLevel::Fatal
                     : priority >= SDL_LOG_PRIORITY_ERROR  ? LogLevel::Error
                     : priority >= SDL_LOG_PRIORITY_WARN   ? LogLevel::Warn
                     : priority >= SDL_LOG_PRIORITY_INFO   ? LogLevel::Info
                                                           : LogLevel::Debug;
    sSdlLog.WriteText(level, message);
}

std::string DisplaySettingsPath()
{
    return PcSaveRoot() + "/display.txt";
}

void LoadDisplaySettings()
{
    std::ifstream file(DisplaySettingsPath().c_str());
    std::string line;
    while (std::getline(file, line))
    {
        std::string::size_type equals = line.find('=');
        if (equals == std::string::npos)
            continue;
        std::istringstream name(line.substr(0, equals));
        std::istringstream value(line.substr(equals + 1));
        std::string key;
        int setting = 0;
        if (!(name >> key) || !(value >> setting))
            continue;
        if (key == "Fullscreen")
            sFullscreen = setting != 0;
        else if (key == "VSync")
            sVSync = setting != 0;
    }
}

void SaveDisplaySettings()
{
    std::ofstream file(DisplaySettingsPath().c_str(), std::ios::trunc);
    if (!file)
    {
        LOG_WARN(sLog, "Display settings could not be written to %s", DisplaySettingsPath().c_str());
        return;
    }
    file << "Fullscreen = " << (sFullscreen ? 1 : 0) << "\n";
    file << "VSync = " << (sVSync ? 1 : 0) << "\n";
}

Uint64 FramePeriod()
{
    if (!sVSync || sWindow == NULL)
        return kFramePeriod;
    const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(SDL_GetDisplayForWindow(sWindow));
    if (mode == NULL || mode->refresh_rate <= 0.0f || mode->refresh_rate >= kVSyncPacedRefresh)
        return kFramePeriod;
    return kVSyncCapPeriod;
}

bool IsFullscreenToggle(const SDL_Event& event)
{
    if (event.type != SDL_EVENT_KEY_DOWN && event.type != SDL_EVENT_KEY_UP)
        return false;
    if (event.key.scancode == SDL_SCANCODE_F11)
        return true;
    return (event.key.scancode == SDL_SCANCODE_RETURN || event.key.scancode == SDL_SCANCODE_KP_ENTER)
           && (event.key.mod & SDL_KMOD_ALT) != 0;
}

void LogWindowScale()
{
    int width = 0, height = 0, pixelsWidth = 0, pixelsHeight = 0;
    SDL_GetWindowSize(sWindow, &width, &height);
    SDL_GetWindowSizeInPixels(sWindow, &pixelsWidth, &pixelsHeight);
    LOG_INFO(sLog, "Window: %dx%d logical, %dx%d pixels, display scale %.2f", width, height,
             pixelsWidth, pixelsHeight, SDL_GetWindowDisplayScale(sWindow));
}

void SetWindowIcon()
{
    const std::string path = PcResourceRoot() + "/icon.png";
    int width = 0;
    int height = 0;
    int channels = 0;
    unsigned char* pixels = stbi_load(path.c_str(), &width, &height, &channels, 4);
    if (pixels == NULL)
    {
        LOG_WARN(sLog, "Window icon %s could not be loaded", path.c_str());
        return;
    }
    SDL_Surface* icon = SDL_CreateSurfaceFrom(width, height, SDL_PIXELFORMAT_RGBA32, pixels, width * 4);
    if (icon == NULL)
        LOG_WARN(sLog, "The window icon could not be set: %s", SDL_GetError());
    else if (!SDL_SetWindowIcon(sWindow, icon))
    {
        if (std::string(SDL_GetCurrentVideoDriver()) == "wayland")
            LOG_DEBUG(sLog, "Compositor window icons are unavailable: %s", SDL_GetError());
        else
            LOG_WARN(sLog, "The window icon could not be set: %s", SDL_GetError());
    }
    SDL_DestroySurface(icon);
    stbi_image_free(pixels);
}
}

bool PcWindowCreate(int width, int height)
{
    SDL_SetLogOutputFunction(SdlLog, NULL);
    SDL_SetAppMetadata("LCEReborn", NULL, "org.lcereborn.LCEReborn");
    SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_TYPE_STRING, "game");
    // We render preedit text; the platform still owns the candidate list.
    SDL_SetHint(SDL_HINT_IME_IMPLEMENTED_UI, "composition");
    SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_SYSTEM_SCALE, "0");
#if defined(_WIN32)
    SDL_SetHint(SDL_HINT_WINDOWS_RAW_KEYBOARD, "1");
#endif
#if defined(__linux__)
    // SDL's automatic selection can prefer XWayland on compositors without
    // fifo-v1. This is a native Wayland port, so try Wayland first even there.
    // Normal hint priority preserves an explicit SDL_VIDEO_DRIVER override.
    const char* session = SDL_getenv("XDG_SESSION_TYPE");
    if ((session && SDL_strcmp(session, "wayland") == 0)
        || (!session && SDL_getenv("WAYLAND_DISPLAY")))
        SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "wayland,x11");
#endif
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD | SDL_INIT_EVENTS))
    {
        LOG_FATAL(sLog, "SDL_Init failed: %s", SDL_GetError());
        return false;
    }

    sWindow = SDL_CreateWindow(WindowTitle().c_str(), width, height,
                               SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN
                                   | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (sWindow == NULL)
    {
        LOG_FATAL(sLog, "SDL_CreateWindow failed: %s", SDL_GetError());
        return false;
    }
    SetWindowIcon();
    LOG_INFO(sLog, "SDL %d.%d.%d", SDL_VERSIONNUM_MAJOR(SDL_GetVersion()),
             SDL_VERSIONNUM_MINOR(SDL_GetVersion()), SDL_VERSIONNUM_MICRO(SDL_GetVersion()));
    LogWindowScale();

    if (!PcGraphics().Create(sWindow, PcGetLaunchOptions().gpuDebug))
    {
        LOG_FATAL(sLog, "Vulkan initialization failed: %s", SDL_GetError());
        SDL_DestroyWindow(sWindow);
        sWindow = NULL;
        return false;
    }
    LoadDisplaySettings();
    if (sFullscreen && !SDL_SetWindowFullscreen(sWindow, true))
    {
        LOG_WARN(sLog, "SDL_SetWindowFullscreen failed: %s", SDL_GetError());
        sFullscreen = false;
    }
    sShown = false;
    return true;
}

void PcWindowDestroy()
{
    PcGraphics().Destroy();

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
    PcGraphics().Present();
}

void PcWindowWaitForFrame()
{
    PcGraphics().WaitForFrame(sVSync);
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
        case SDL_EVENT_WINDOW_ENTER_FULLSCREEN:
            sFullscreen = true;
            break;
        case SDL_EVENT_WINDOW_LEAVE_FULLSCREEN:
            sFullscreen = false;
            break;
        case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
            LogWindowScale();
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

        if (IsFullscreenToggle(event))
        {
            if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat)
                PcWindowSetFullscreen(!sFullscreen);
            continue;
        }

        PcInputHandleEvent(event);
    }
}

void PcWindowPaceFrame()
{
    const Uint64 period = FramePeriod();
    Uint64 now = SDL_GetTicksNS();
    if (sNextFrame == 0 || now >= sNextFrame + period)
    {
        sNextFrame = now + period;
        return;
    }
    if (now < sNextFrame)
        SDL_DelayPrecise(sNextFrame - now);
    sNextFrame += period;
}

bool PcWindowIsFullscreen()
{
    return sFullscreen;
}

void PcWindowSetFullscreen(bool fullscreen)
{
    if (sWindow == NULL)
        return;
    if (!SDL_SetWindowFullscreen(sWindow, fullscreen))
    {
        LOG_WARN(sLog, "SDL_SetWindowFullscreen failed: %s", SDL_GetError());
        return;
    }
    sFullscreen = fullscreen;
    SaveDisplaySettings();
}

bool PcWindowIsVSync()
{
    return sVSync;
}

void PcWindowSetVSync(bool enabled)
{
    sVSync = enabled;
    SaveDisplaySettings();
}
