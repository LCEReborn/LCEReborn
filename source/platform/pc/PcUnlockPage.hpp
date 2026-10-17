#ifndef PLATFORM_PC_PCUNLOCKPAGE_HPP
#define PLATFORM_PC_PCUNLOCKPAGE_HPP

#include "game/ui/UICustomMenuPage.hpp"

class PcUnlockPage : public UICustomMenuPage
{
public:
    enum
    {
        eItem_Status,
        eItem_CheckAgain
    };

    virtual void Build(UIScene_CustomMenu& menu);
    virtual void OnPress(UIScene_CustomMenu& menu, int id);
};

#endif
