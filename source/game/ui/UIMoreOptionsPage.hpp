#ifndef GAME_UI_UIMOREOPTIONSPAGE_HPP
#define GAME_UI_UIMOREOPTIONSPAGE_HPP

#include <string>

#include "game/ui/UICustomMenuPage.hpp"

class UIMoreOptionsPage : public UICustomMenuPage
{
public:
    enum
    {
        eItem_Fullscreen,
        eItem_VSync,
        eItem_RenderDistance,
        eItem_SimulationDistance,
        eItem_Fov,
        eItem_FancyGraphics,
        eItem_SmoothLighting,
        eItem_Particles,
        eItem_BiomeBlend,
        eItem_DebugOverlay
    };

    virtual void Build(UIScene_CustomMenu& menu);
    virtual void OnPress(UIScene_CustomMenu& menu, int id);
    virtual void OnToggle(UIScene_CustomMenu& menu, int id, bool checked);
    virtual void OnSlide(UIScene_CustomMenu& menu, int id, int value);

private:
    static std::wstring RenderDistanceLabel(int chunks);
    static std::wstring SimulationDistanceLabel(int chunks);
    static std::wstring FovLabel(int fov);
    static std::wstring ParticlesLabel(int particles);
    static std::wstring BiomeBlendLabel(int radius);
};

#endif
