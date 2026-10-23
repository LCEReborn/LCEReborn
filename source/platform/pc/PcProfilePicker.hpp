#ifndef PLATFORM_PC_PCPROFILEPICKER_HPP
#define PLATFORM_PC_PCPROFILEPICKER_HPP

#include <vector>

#include "game/ui/UICustomMenuPage.hpp"
#include "platform/pc/PcPlayerProfiles.hpp"

class PcProfilePickerPage : public UICustomMenuPage
{
public:
    PcProfilePickerPage(int pad, const std::vector<PcPlayerProfile>& profiles);
    virtual ~PcProfilePickerPage();

    virtual void Build(UIScene_CustomMenu& menu);
    virtual void OnPress(UIScene_CustomMenu& menu, int id);
    virtual void OnClose(UIScene_CustomMenu& menu);
    virtual bool IsOverlay() const;
    virtual bool ShowLogo() const;

private:
    int mPad;
    std::vector<PcPlayerProfile> mProfiles;
    bool mResolved;
};

#endif
