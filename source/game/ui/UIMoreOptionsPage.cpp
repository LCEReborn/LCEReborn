#include "game/ui/UIMoreOptionsPage.hpp"

#include <string>

#include "game/ui/UIScene_CustomMenu.hpp"
#if LCE_PLATFORM_PC
#include "platform/pc/PcWindow.hpp"
#endif

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
