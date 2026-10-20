#include "game/ui/UIMoreOptionsPage.hpp"

#include <cwchar>
#include <string>

#include "game/core/ConsoleUIController.hpp"
#include "game/core/VideoSettings.hpp"
#include "game/ui/UILayer.hpp"
#include "game/ui/UIScene_CustomMenu.hpp"
#if LCE_PLATFORM_PC
#include "platform/pc/PcWindow.hpp"
#endif

extern ConsoleUIController gConsoleUIController;

void UIMoreOptionsPage::Build(UIScene_CustomMenu& menu)
{
#if LCE_PLATFORM_PC
    menu.AddCheckbox(eItem_Fullscreen, L"Fullscreen", PcWindowIsFullscreen(),
                     L"Fills the whole screen with the game. Turn it off to play in a window you can move and resize. "
                     L"F11 switches between the two at any time.");
    menu.AddCheckbox(eItem_VSync, L"V-Sync", PcWindowIsVSync(),
                     L"Matches the frame rate to your display's refresh rate, which stops screen tearing. "
                     L"Turn it off for the lowest input delay.");
#endif
    menu.AddSlider(eItem_RenderDistance, RenderDistanceLabel(VideoSettings::GetRenderDistance()),
                   VideoSettings::RENDER_DISTANCE_MIN, VideoSettings::RENDER_DISTANCE_MAX,
                   VideoSettings::GetRenderDistance(),
                   L"How many chunks around you are drawn. Lower it for a higher frame rate. When you host, the "
                   L"world is also sent this far to every player. The original game draws 10.");
    menu.AddSlider(eItem_Fov, FovLabel(VideoSettings::GetFov()), VideoSettings::FOV_MIN, VideoSettings::FOV_MAX,
                   VideoSettings::GetFov(),
                   L"How wide the view is, in degrees. Normal is 70, as in the original game; Quake Pro is the "
                   L"widest.");
    menu.AddCheckbox(eItem_FancyGraphics, L"Fancy Graphics", VideoSettings::GetFancyGraphics(),
                     L"Draws see-through leaves, entity shadows and other detail. Turn it off for a higher frame "
                     L"rate.");
    menu.AddCheckbox(eItem_SmoothLighting, L"Smooth Lighting", VideoSettings::GetSmoothLighting(),
                     L"Shades block corners softly. Turn it off for flat lighting and faster chunk building.");
    menu.AddSlider(eItem_Particles, ParticlesLabel(VideoSettings::GetParticles()), VideoSettings::EParticles_All,
                   VideoSettings::EParticles_Count - 1, VideoSettings::GetParticles(),
                   L"How many particles are shown. Fewer particles help the frame rate in busy places.");
    menu.AddSlider(eItem_BiomeBlend, BiomeBlendLabel(VideoSettings::GetBiomeBlend()), 0, VideoSettings::BIOME_BLEND_MAX,
                   VideoSettings::GetBiomeBlend(),
                   L"Smooths grass, leaf and water colours across biome borders. Wider blending looks softer but "
                   L"builds chunks more slowly. The original game blends 3x3.");
    menu.AddButton(eItem_DebugOverlay, L"Debug Overlay",
                   L"Choose what the debug overlay shows down the left of the screen: each line is always shown, "
                   L"shown while it is open, or off. F3 opens it on a keyboard, and both sticks pressed together "
                   L"on a controller.");
}

void UIMoreOptionsPage::OnPress(UIScene_CustomMenu& menu, int id)
{
    if (id == eItem_DebugOverlay)
        gConsoleUIController.NavigateToScene(menu.GetPlayer(), EUIScene_DebugOverlayMenu, NULL, EUILayer_Scene,
                                             EUIGroup_PlayerDefault);
}

void UIMoreOptionsPage::OnToggle(UIScene_CustomMenu&, int id, bool checked)
{
    switch (id)
    {
#if LCE_PLATFORM_PC
    case eItem_Fullscreen:
        PcWindowSetFullscreen(checked);
        break;
    case eItem_VSync:
        PcWindowSetVSync(checked);
        break;
#endif
    case eItem_FancyGraphics:
        VideoSettings::SetFancyGraphics(checked);
        break;
    case eItem_SmoothLighting:
        VideoSettings::SetSmoothLighting(checked);
        break;
    default:
        break;
    }
}

void UIMoreOptionsPage::OnSlide(UIScene_CustomMenu& menu, int id, int value)
{
    switch (id)
    {
    case eItem_RenderDistance:
        VideoSettings::SetRenderDistance(value);
        menu.SetItemLabel(id, RenderDistanceLabel(VideoSettings::GetRenderDistance()));
        break;
    case eItem_Fov:
        VideoSettings::SetFov(value);
        menu.SetItemLabel(id, FovLabel(VideoSettings::GetFov()));
        break;
    case eItem_Particles:
        VideoSettings::SetParticles(value);
        menu.SetItemLabel(id, ParticlesLabel(VideoSettings::GetParticles()));
        break;
    case eItem_BiomeBlend:
        VideoSettings::SetBiomeBlend(value);
        menu.SetItemLabel(id, BiomeBlendLabel(VideoSettings::GetBiomeBlend()));
        break;
    default:
        break;
    }
}

std::wstring UIMoreOptionsPage::RenderDistanceLabel(int chunks)
{
    wchar_t text[64];
    std::swprintf(text, sizeof(text) / sizeof(text[0]), L"Render Distance: %d chunks", chunks);
    return text;
}

std::wstring UIMoreOptionsPage::FovLabel(int fov)
{
    if (fov == VideoSettings::FOV_NORMAL)
        return L"FOV: Normal";
    if (fov == VideoSettings::FOV_MAX)
        return L"FOV: Quake Pro";
    wchar_t text[64];
    std::swprintf(text, sizeof(text) / sizeof(text[0]), L"FOV: %d", fov);
    return text;
}

std::wstring UIMoreOptionsPage::ParticlesLabel(int particles)
{
    switch (particles)
    {
    case VideoSettings::EParticles_Decreased:
        return L"Particles: Decreased";
    case VideoSettings::EParticles_Minimal:
        return L"Particles: Minimal";
    default:
        return L"Particles: All";
    }
}

std::wstring UIMoreOptionsPage::BiomeBlendLabel(int radius)
{
    if (radius == 0)
        return L"Biome Blend: Off";
    wchar_t text[64];
    int size = radius * 2 + 1;
    std::swprintf(text, sizeof(text) / sizeof(text[0]), L"Biome Blend: %dx%d", size, size);
    return text;
}
