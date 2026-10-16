#include "game/ui/UIMoreOptionsPage.hpp"

#include <string>

#include "game/ui/UIScene_CustomMenu.hpp"
#if LCE_PLATFORM_PC
#include "platform/pc/PcWindow.hpp"
#endif

void UIMoreOptionsPage::Build(UIScene_CustomMenu& menu)
{
#if LCE_PLATFORM_PC
    menu.AddCheckbox(eItem_Fullscreen, L"Fullscreen", PcWindowIsFullscreen(), std::wstring());
    menu.AddCheckbox(eItem_VSync, L"V-Sync", PcWindowIsVSync(), std::wstring());
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
