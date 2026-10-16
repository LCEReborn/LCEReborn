#include "game/ui/UIMoreOptionsPage.hpp"

#include <string>

#include "game/ui/UIScene_CustomMenu.hpp"
#if LCE_PLATFORM_PC
#include "platform/pc/PcWindow.hpp"
#endif

void UIMoreOptionsPage::Build(UIScene_CustomMenu& menu)
{
#if LCE_PLATFORM_PC
    menu.AddLabel(eItem_DisplayHeading, L"Display");
    menu.AddCheckbox(eItem_Fullscreen, L"Fullscreen", PcWindowIsFullscreen(),
                     L"Fill the screen with the game. F11 or Alt+Enter also switches.");
    menu.AddCheckbox(eItem_VSync, L"V-Sync", PcWindowIsVSync(),
                     L"Wait for the monitor before showing each frame, so the picture never tears.");
#endif
    menu.AddLabel(eItem_AboutHeading, L"About");
    menu.AddValue(eItem_Version, L"LCE Reborn", L"Preview",
                  L"LCE Reborn is built from the reconstructed source of Minecraft: Wii U Edition.");
}

void UIMoreOptionsPage::OnToggle(UIScene_CustomMenu& menu, int id, bool checked)
{
#if LCE_PLATFORM_PC
    switch (id)
    {
    case eItem_Fullscreen:
        PcWindowSetFullscreen(checked);
        break;
    case eItem_VSync:
        PcWindowSetVSync(checked);
        break;
    default:
        break;
    }
#endif
}
