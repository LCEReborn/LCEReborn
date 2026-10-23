#include "game/ui/UIPacksPage.hpp"

#include <cwchar>

#include "game/core/CConsoleMinecraftApp.hpp"
#include "game/core/DLCManager.hpp"
#include "game/core/DLCPack.hpp"
#include "game/core/StringConversion.hpp"
#include "game/ui/UIScene_CustomMenu.hpp"
#include "platform/C4JStorage.hpp"
#if LCE_PLATFORM_PC
#include "platform/pc/PcPaths.hpp"
#endif

extern C4JStorage* gStorage;

UIPacksPage::EKind UIPacksPage::KindOf(DLCPack* pack)
{
    bool texture = false;
    std::vector<DLCPack*>& children = pack->getChildPacks();
    for (size_t index = 0; index < children.size() && !texture; ++index)
        texture = children[index]->getDLCItemsCount(DLCManager::eDLCType_Texture) > 0;
    const bool skin = pack->getDLCItemsCount(DLCManager::eDLCType_Skin) > 0;
    if (texture && skin)
        return eKind_MashUp;
    if (texture)
        return eKind_Texture;
    if (skin)
        return eKind_Skin;
    if (pack->getDLCItemsCount(DLCManager::eDLCType_GameRules) > 0
        || pack->getDLCItemsCount(DLCManager::eDLCType_GameRulesHeader) > 0)
        return eKind_Map;
    return eKind_Other;
}

std::wstring UIPacksPage::FolderName()
{
    const std::string folder = gStorage->GetPacksFolder();
#if LCE_PLATFORM_WIIU
    static const char kSdCard[] = "/vol/external01/";
    if (folder.compare(0, sizeof(kSdCard) - 1, kSdCard) == 0)
        return L"SD card: " + convStringToWstring(folder.substr(sizeof(kSdCard) - 1));
#endif
    return convStringToWstring(folder);
}

void UIPacksPage::Build(UIScene_CustomMenu& menu)
{
    const std::wstring folder = FolderName();
    const std::wstring howTo = L"Put each pack there as a .pck file, or as a folder holding the .pck and its Data "
                               L"folder. New packs load the next time the game starts.";
#if LCE_PLATFORM_PC
    menu.AddButton(eItem_OpenFolder, L"Open Packs Folder", L"The packs folder is " + folder + L". " + howTo);
#else
    if (folder.empty())
        menu.AddValue(eItem_Folder, L"Packs Folder", L"No SD Card",
                      L"Insert an SD card to load packs from it. They go in wiiu/LCEReborn/packs on the card.");
    else
        menu.AddValue(eItem_Folder, L"Packs Folder", L"SD Card", L"The packs folder is " + folder + L". " + howTo);
#endif

    if (gApp.DLCInstallProcessCompleted() == 0)
    {
        menu.AddLabel(eItem_Loading, L"Loading packs...");
        return;
    }

    static const wchar_t* const kHeadings[eKind_Count] = {L"Mash-up Packs", L"Texture Packs", L"Skin Packs",
                                                          L"Map Packs", L"Other Packs"};
    std::vector<DLCPack*> packs[eKind_Count];
    const int count = gApp.mDLCManager.getPackCount(DLCManager::eDLCType_All);
    for (int index = 0; index < count; ++index)
    {
        DLCPack* pack = gApp.mDLCManager.getPack(static_cast<unsigned int>(index), DLCManager::eDLCType_All);
        if (pack != NULL && !pack->GetRootPath().empty())
            packs[KindOf(pack)].push_back(pack);
    }

    int id = eItem_FirstPack;
    bool any = false;
    for (int kind = 0; kind < eKind_Count; ++kind)
    {
        if (packs[kind].empty())
            continue;
        any = true;
        menu.AddLabel(eItem_FirstHeading + kind, kHeadings[kind]);
        for (size_t index = 0; index < packs[kind].size(); ++index)
        {
            DLCPack* pack = packs[kind][index];
            wchar_t packId[32];
            std::swprintf(packId, 32, L"Pack ID %u. ", pack->GetPackId());
            std::wstring description(packId);
            std::wstring source;
            switch (pack->GetSource())
            {
            case C4JStorage::EPackSource_PacksFolder:
                source = L"Packs Folder";
                description += L"Loaded from " + pack->getName() + L" in the packs folder.";
                break;
            case C4JStorage::EPackSource_Server:
                source = L"From Server";
                description += L"Sent by a server you joined, and kept so it isn't sent again.";
                break;
            default:
                source = L"Built In";
                description += L"Comes with the game.";
                break;
            }
            std::wstring name = pack->getLocalisedDisplayName();
            if (name.empty())
                name = pack->getName();
            menu.AddValue(id++, name, source, description);
        }
    }
    if (!any)
        menu.AddLabel(eItem_None, L"No packs are loaded.");
}

void UIPacksPage::OnPress(UIScene_CustomMenu&, int id)
{
#if LCE_PLATFORM_PC
    if (id == eItem_OpenFolder)
        PcOpenFolder(PcPacksRoot());
#else
    (void)id;
#endif
}
