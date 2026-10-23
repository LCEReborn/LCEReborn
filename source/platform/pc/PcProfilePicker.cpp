#include "platform/pc/PcProfilePicker.hpp"

#include "game/ui/UIScene_CustomMenu.hpp"
#include "platform/pc/CProfile.hpp"

namespace
{
const int kTitleId = 1000;
}

PcProfilePickerPage::PcProfilePickerPage(int pad, const std::vector<PcPlayerProfile>& profiles)
    : mPad(pad), mProfiles(profiles), mResolved(false)
{
}

PcProfilePickerPage::~PcProfilePickerPage()
{
    if (!mResolved && gProfileInstance != NULL)
        gProfileInstance->ProfilePickerClosed(mPad, NULL);
}

void PcProfilePickerPage::Build(UIScene_CustomMenu& menu)
{
    menu.AddLabel(kTitleId, L"Who is playing?");
    for (size_t index = 0; index < mProfiles.size(); ++index)
    {
        const PcPlayerProfile& profile = mProfiles[index];
        menu.AddButton(static_cast<int>(index), profile.name, L"Joins as " + profile.username + L".");
    }
}

void PcProfilePickerPage::OnPress(UIScene_CustomMenu& menu, int id)
{
    if (id < 0 || static_cast<size_t>(id) >= mProfiles.size() || mResolved)
        return;
    mResolved = true;
    const PcPlayerProfile chosen = mProfiles[id];
    const int pad = mPad;
    menu.navigateBack();
    if (gProfileInstance != NULL)
        gProfileInstance->ProfilePickerClosed(pad, &chosen);
}

void PcProfilePickerPage::OnClose(UIScene_CustomMenu&)
{
    if (mResolved)
        return;
    mResolved = true;
    if (gProfileInstance != NULL)
        gProfileInstance->ProfilePickerClosed(mPad, NULL);
}

bool PcProfilePickerPage::IsOverlay() const
{
    return true;
}

bool PcProfilePickerPage::ShowLogo() const
{
    return false;
}
