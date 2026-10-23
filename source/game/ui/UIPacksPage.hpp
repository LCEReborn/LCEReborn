#ifndef GAME_UI_UIPACKSPAGE_HPP
#define GAME_UI_UIPACKSPAGE_HPP

#include <string>
#include <vector>

#include "game/ui/UICustomMenuPage.hpp"

class DLCPack;

class UIPacksPage : public UICustomMenuPage
{
public:
    enum
    {
        eItem_Folder,
        eItem_OpenFolder,
        eItem_Loading,
        eItem_None,
        eItem_FirstHeading = 100,
        eItem_FirstPack = 200
    };

    virtual void Build(UIScene_CustomMenu& menu);
    virtual void OnPress(UIScene_CustomMenu& menu, int id);

private:
    enum EKind
    {
        eKind_MashUp,
        eKind_Texture,
        eKind_Skin,
        eKind_Map,
        eKind_Other,
        eKind_Count
    };

    static EKind KindOf(DLCPack* pack);
    static std::wstring FolderName();
};

#endif
