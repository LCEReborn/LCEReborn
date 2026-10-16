#ifndef GAME_UI_UIMOREOPTIONSPAGE_HPP
#define GAME_UI_UIMOREOPTIONSPAGE_HPP

#include "game/ui/UICustomMenuPage.hpp"

class UIMoreOptionsPage : public UICustomMenuPage
{
public:
    enum
    {
        eItem_Fullscreen,
        eItem_VSync
    };

    virtual void Build(UIScene_CustomMenu& menu);
    virtual void OnToggle(UIScene_CustomMenu& menu, int id, bool checked);
};

#endif
