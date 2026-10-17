#include "platform/pc/PcUnlockPage.hpp"

#include "game/core/StringConversion.hpp"
#include "game/ui/UIScene_CustomMenu.hpp"
#include "platform/CProfile.hpp"
#include "platform/pc/PcLicenses.hpp"
#include "platform/pc/PcPaths.hpp"

#include <string>

static std::wstring StatusText()
{
    return gProfile->IsFullVersion() ? L"Full game unlocked" : L"Trial";
}

void PcUnlockPage::Build(UIScene_CustomMenu& menu)
{
    menu.AddValue(eItem_Status, L"Base game", StatusText(),
                  L"If you own the game, set full_game = true in " + convStringToWstring(PcLicenseFile())
                      + L", then choose Check Again.");
    menu.AddButton(eItem_CheckAgain, L"Check Again", L"Reads the license file again.");
}

void PcUnlockPage::OnPress(UIScene_CustomMenu& menu, int id)
{
    if (id != eItem_CheckAgain)
        return;
    PcApplyLicenses();
    menu.SetItemValue(eItem_Status, StatusText());
}
