#include "game/ui/UIScene_SettingsListMenu.hpp"
#include "game/core/StringIds.hpp"

#include <cwchar>

#include "game/core/CConsoleMinecraftApp.hpp"
#include "game/core/CMinecraftApp.hpp"
#include "game/core/ConsoleUIController.hpp"
#include "game/core/Difficulty.hpp"
#include "game/core/GameSettings.hpp"
#include "game/core/L10N.hpp"
#include "game/core/Minecraft.hpp"
#include "game/network/CGameNetworkManager.hpp"
#include "game/client/audio/SoundEvents.hpp"
#include "game/minigame/MiniGameDef.hpp"
#include "game/ui/FJ_MultilistMenu.hpp"
#include "game/ui/FJ_FuiNode.hpp"
#include "game/ui/UILayer.hpp"
#include "game/ui/fuiFile.hpp"
#include "game/ui/fuiRenderNode.hpp"
#include "platform/CProfile.hpp"

extern ConsoleUIController gConsoleUIController;

UIScene_SettingsListMenu::UIScene_SettingsListMenu(int sceneType, void* data, UILayer* layer)
    : UIScene(sceneType, layer)
{
    initialiseMovie();
    int* settingsData = static_cast<int*>(data);
    m_settingsMode = static_cast<unsigned int>(settingsData[1]);
    delete settingsData;
    Init();
}

UIScene_SettingsListMenu::~UIScene_SettingsListMenu()
{
}

unsigned int UIScene_SettingsListMenu::mapElementsAndNames()
{
    UIScene::mapElementsAndNames();
    fuiRenderNode* currentNode = m_movie->getRootNode();
    {
        m_multiList.setupControl(this, currentNode, std::string("MultiList"));
        m_controls.push_back(&m_multiList);
    }
    {
        m_description.setupControl(this, currentNode, std::string("Description"));
        m_controls.push_back(&m_description);
    }
    return 1;
}

void UIScene_SettingsListMenu::doVerticalResizeCheck()
{
    FJ_MultilistMenu* menu = static_cast<FJ_MultilistMenu*>(m_movie->getRootNode()->getFuiNode());
    menu->DoVerticalResizeCheck();
}

void UIScene_SettingsListMenu::HideDescription()
{
    FJ_MultilistMenu* menu = static_cast<FJ_MultilistMenu*>(m_movie->getRootNode()->getFuiNode());
    menu->HideDescription();
}

inline void UIScene_SettingsListMenu::addSettingCheckbox(int playerId, const wchar_t* label, int id,
                                                         eGameSetting setting)
{
    m_multiList.AddNewCheckbox(std::wstring(label), id, GameSettings::GetGameSettings(playerId, setting) == 1);
}

inline void UIScene_SettingsListMenu::addInvertedSettingCheckbox(int playerId, const wchar_t* label, int id,
                                                                 eGameSetting setting)
{
    m_multiList.AddNewCheckbox(std::wstring(label), id, GameSettings::GetGameSettings(playerId, setting) != 1);
}

void UIScene_SettingsListMenu::Init()
{
    wchar_t text[256];
    Minecraft* minecraft = Minecraft::GetInstance();
    m_gameStarted = minecraft->mLevel != 0;
    m_primaryPlayer = gProfile->GetPrimaryPad() == m_playerId;
    m_host = CGameNetworkManager::sInstance.IsHost();
    m_saveRequested = false;

    switch (m_settingsMode)
    {
    case 0:
    {
        bool canChangeDifficulty = true;
        bool canChangeAutosave = true;
        bool showSaveButton = true;
        if (!m_primaryPlayer)
        {
            canChangeDifficulty = false;
            canChangeAutosave = false;
        }
        if (m_gameStarted)
        {
            canChangeDifficulty = false;
            if (!CGameNetworkManager::sInstance.IsHost())
                canChangeAutosave = false;
        }
        if (gApp.GetGameStarted())
            showSaveButton = false;
        if (m_primaryPlayer)
            addSettingCheckbox(m_playerId, L10N::GetString(IDS_CHECKBOX_VERTICAL_SPLIT_SCREEN), 0,
                               eGameSetting_SplitScreenVertical);
        addSettingCheckbox(m_playerId, L10N::GetString(IDS_AUTOJUMP), 9, eGameSetting_AutoJump);
        addSettingCheckbox(m_playerId, L10N::GetString(IDS_VIEW_BOBBING), 1, eGameSetting_ViewBob);
        addInvertedSettingCheckbox(m_playerId, L10N::GetString(IDS_FLYING_VIEW_ROLLING), 2,
                                   eGameSetting_FlyingViewRolling);
        addSettingCheckbox(m_playerId, L10N::GetString(IDS_HINTS), 3, eGameSetting_Hints);
        addSettingCheckbox(m_playerId, L10N::GetString(IDS_CHECKBOX_DEATH_MESSAGES), 4, eGameSetting_DeathMessages);
        if (showSaveButton)
            m_multiList.AddNewButton(std::wstring(L10N::GetString(IDS_LANGUAGE_SELECTOR)), 5, false);
        if (canChangeAutosave)
        {
            int skin = static_cast<int>(GameSettings::GetGameSettings(m_playerId, eGameSetting_Autosave));
            for (unsigned int index = 0; index < 9; ++index)
            {
                if (index == 0)
                    std::swprintf(m_skinNames[index], 256, L"%ls", L10N::GetString(IDS_SLIDER_AUTOSAVE_OFF));
                else
                    std::swprintf(m_skinNames[index], 256, L"%ls: %d %ls", L10N::GetString(IDS_SLIDER_AUTOSAVE),
                                  index * 15, L10N::GetString(IDS_MINUTES));
            }
            m_multiList.AddNewSlider(std::wstring(m_skinNames[skin]), 6, 0, 8, skin, false, 7, 3);
        }
        std::swprintf(text, 256, L"%ls: %d%%", L10N::GetString(IDS_SLIDER_SENSITIVITY_INGAME),
                      GameSettings::GetGameSettings(m_playerId, eGameSetting_GameSensitivity));
        m_multiList.AddNewSlider(std::wstring(text), 7, 1, 200,
                                 GameSettings::GetGameSettings(m_playerId, eGameSetting_GameSensitivity), true, 7, 3);
        if (canChangeDifficulty)
        {
            int difficulty = static_cast<int>(GameSettings::GetGameSettings(m_playerId, eGameSetting_Difficulty));
            for (int index = 0; index < 4; ++index)
                std::swprintf(m_difficultyNames[index], 256, L"%ls: %ls", L10N::GetString(IDS_SLIDER_DIFFICULTY),
                              L10N::GetString(Difficulty::byId(index)->getKey()));
            m_multiList.AddNewSlider(std::wstring(m_difficultyNames[difficulty]), 8, 0, 3, difficulty, false, 7, 3);
        }
    }
    break;
    case 1:
    {
        std::swprintf(text, 256, L"%ls: %d%%", L10N::GetString(IDS_SLIDER_MUSIC),
                      GameSettings::GetGameSettings(m_playerId, eGameSetting_MusicVolume));
        m_multiList.AddNewSlider(std::wstring(text), 10, 0, 100,
                                 GameSettings::GetGameSettings(m_playerId, eGameSetting_MusicVolume), true, 7, 3);
        std::swprintf(text, 256, L"%ls: %d%%", L10N::GetString(IDS_SLIDER_SOUND),
                      GameSettings::GetGameSettings(m_playerId, eGameSetting_SoundVolume));
        m_multiList.AddNewSlider(std::wstring(text), 11, 0, 100,
                                 GameSettings::GetGameSettings(m_playerId, eGameSetting_SoundVolume), true, 7, 3);
        addSettingCheckbox(m_playerId, L10N::GetString(IDS_CAVE_SOUNDS), 12, eGameSetting_CaveSounds);
        addSettingCheckbox(m_playerId, L10N::GetString(IDS_MINECART_SOUNDS), 13, eGameSetting_MinecartSounds);
        addSettingCheckbox(m_playerId, L10N::GetString(IDS_VOICE_CHAT), 14, eGameSetting_GameChat);
        m_multiList.EnableItem(14, true, true);
    }
    break;
    case 2:
    {
        if (gProfile->GetPrimaryPad() == m_playerId)
        {
            addSettingCheckbox(m_playerId, L10N::GetString(IDS_CHECKBOX_RENDER_CLOUDS), 15, eGameSetting_Clouds);
            if (!m_gameStarted || m_primaryPlayer)
                addSettingCheckbox(m_playerId, L10N::GetString(IDS_CHECKBOX_CUSTOM_SKIN_ANIM), 16,
                                   eGameSetting_CustomSkinAnim);
            std::swprintf(text, 256, L"%ls: %d%%", L10N::GetString(IDS_SLIDER_GAMMA),
                          GameSettings::GetGameSettings(m_playerId, eGameSetting_Gamma));
            m_multiList.AddNewSlider(std::wstring(text), 17, 0, 100,
                                     GameSettings::GetGameSettings(m_playerId, eGameSetting_Gamma), true, 7, 3);
        }
        if (!m_gameStarted
            || (Minecraft::InMiniGame(eMiniGameId_Glide, true)
                && Minecraft::GetInstance()->GetMiniGame()->GetHostOption(eCustomGameModeHostOption_TimeTrial)))
        {
            addSettingCheckbox(m_playerId, L10N::GetString(IDS_MG03_ENABLE_GHOST), 18, eGameSetting_ShowGlideGhost);
            addSettingCheckbox(m_playerId, L10N::GetString(IDS_MG03_SHOW_GHOST_PATH), 19,
                               eGameSetting_ShowGlideGhostPath);
        }
    }
    break;
    case 3:
    {
        addSettingCheckbox(m_playerId, L10N::GetString(IDS_CHECKBOX_DISPLAY_HUD), 20, eGameSetting_DisplayHUD);
        addSettingCheckbox(m_playerId, L10N::GetString(IDS_CHECKBOX_DISPLAY_HAND), 21, eGameSetting_DisplayHand);
        addSettingCheckbox(m_playerId, L10N::GetString(IDS_CHECKBOX_DISPLAY_CHAT_MESSAGES), 22,
                           eGameSetting_DisplayGameMessages);
        if (m_primaryPlayer)
        {
            addSettingCheckbox(m_playerId, L10N::GetString(IDS_CHECKBOX_DISPLAY_SAVE_ICON), 23,
                               eGameSetting_DisplaySaveIcon);
            std::swprintf(text, 256, L"%ls: %d%%", L10N::GetString(IDS_SLIDER_INTERFACEOPACITY),
                          GameSettings::GetGameSettings(m_playerId, eGameSetting_InterfaceOpacity));
            m_multiList.AddNewSlider(std::wstring(text), 24, 0, 100,
                                     GameSettings::GetGameSettings(m_playerId, eGameSetting_InterfaceOpacity), true, 7,
                                     3);
        }
        addSettingCheckbox(m_playerId, L10N::GetString(IDS_IN_GAME_TOOLTIPS), 25, eGameSetting_Tooltips);
        addSettingCheckbox(m_playerId, L10N::GetString(IDS_CHECKBOX_ANIMATED_CHARACTER), 26,
                           eGameSetting_AnimatedCharacter);
        std::swprintf(text, 256, L"%ls: %d%%", L10N::GetString(IDS_SLIDER_SENSITIVITY_INMENU),
                      GameSettings::GetGameSettings(m_playerId, eGameSetting_InterfaceSensitivity));
        m_multiList.AddNewSlider(std::wstring(text), 27, 0, 200,
                                 GameSettings::GetGameSettings(m_playerId, eGameSetting_InterfaceSensitivity), true, 7,
                                 3);
        bool primaryPlayer = m_primaryPlayer;
        if ((!m_gameStarted
             || (CGameNetworkManager::sInstance.IsHost() && !Minecraft::InMiniGame(eMiniGameId_Any, true)))
            && primaryPlayer)
            addSettingCheckbox(m_playerId, L10N::GetString(IDS_IN_GAME_GAMERTAGS), 28, eGameSetting_DisplayMii);
        if (m_primaryPlayer)
            addSettingCheckbox(m_playerId, L10N::GetString(IDS_CHECKBOX_DISPLAY_SPLITSCREENGAMERTAGS), 29,
                               eGameSetting_SplitscreenNicknames);
        if (gApp.GetGameHostOption(eGameHostOption_MiniGame) == 0)
            addSettingCheckbox(m_playerId, L10N::GetString(IDS_CHECKBOX_CLASSIC_CRAFTING), 30,
                               eGameSetting_ClassicCrafting);
        if (!m_primaryPlayer || gProfile->IsGuest(m_playerId))
        {
            std::swprintf(text, 256, L"%ls: %d", L10N::GetString(IDS_SLIDER_UISIZESPLITSCREEN),
                          GameSettings::GetGameSettings(m_playerId, eGameSetting_HUDSizeSplitscreen) + 1);
            m_multiList.AddNewSlider(std::wstring(text), 32, 1, 3,
                                     GameSettings::GetGameSettings(m_playerId, eGameSetting_HUDSizeSplitscreen) + 1,
                                     false, 7, 3);
        }
        else
        {
            std::swprintf(text, 256, L"%ls: %d", L10N::GetString(IDS_SLIDER_UISIZE),
                          GameSettings::GetGameSettings(m_playerId, eGameSetting_HUDSize) + 1);
            m_multiList.AddNewSlider(std::wstring(text), 31, 1, 3,
                                     GameSettings::GetGameSettings(m_playerId, eGameSetting_HUDSize) + 1, false, 7, 3);
            std::swprintf(text, 256, L"%ls: %d", L10N::GetString(IDS_SLIDER_UISIZESPLITSCREEN),
                          GameSettings::GetGameSettings(m_playerId, eGameSetting_HUDSizeSplitscreen) + 1);
            m_multiList.AddNewSlider(std::wstring(text), 32, 1, 3,
                                     GameSettings::GetGameSettings(m_playerId, eGameSetting_HUDSizeSplitscreen) + 1,
                                     false, 7, 3);
        }
    }
    break;
    }
    doVerticalResizeCheck();
    doHorizontalResizeCheck();
    HideDescription();
    if (gApp.GetLocalPlayerCount() > 1)
    {
    }
    gConsoleUIController.TouchBoxRebuild(this);
}

void UIScene_SettingsListMenu::setGameSettings()
{
    switch (m_settingsMode)
    {
    case 0:
    {
        int playerId = m_playerId;
        if (m_multiList.CheckElementExists(9))
            GameSettings::SetGameSettings(playerId, eGameSetting_AutoJump, m_multiList.GetCheckboxValue(9));
    }
        {
            int playerId = m_playerId;
            if (m_multiList.CheckElementExists(1))
                GameSettings::SetGameSettings(playerId, eGameSetting_ViewBob, m_multiList.GetCheckboxValue(1));
        }
        {
            int playerId = m_playerId;
            if (m_multiList.CheckElementExists(2))
                GameSettings::SetGameSettings(playerId, eGameSetting_FlyingViewRolling,
                                              !m_multiList.GetCheckboxValue(2));
        }
        {
            int playerId = m_playerId;
            if (m_multiList.CheckElementExists(3))
                GameSettings::SetGameSettings(playerId, eGameSetting_Hints, m_multiList.GetCheckboxValue(3));
        }
        {
            int playerId = m_playerId;
            if (m_multiList.CheckElementExists(4))
                GameSettings::SetGameSettings(playerId, eGameSetting_DeathMessages, m_multiList.GetCheckboxValue(4));
        }
        {
            int playerId = m_playerId;
            if (m_multiList.CheckElementExists(0))
                GameSettings::SetGameSettings(playerId, eGameSetting_SplitScreenVertical,
                                              m_multiList.GetCheckboxValue(0));
        }
        break;
    case 1:
    {
        int playerId = m_playerId;
        if (m_multiList.CheckElementExists(12))
            GameSettings::SetGameSettings(playerId, eGameSetting_CaveSounds, m_multiList.GetCheckboxValue(12));
    }
        {
            int playerId = m_playerId;
            if (m_multiList.CheckElementExists(13))
                GameSettings::SetGameSettings(playerId, eGameSetting_MinecartSounds, m_multiList.GetCheckboxValue(13));
        }
        {
            int playerId = m_playerId;
            if (m_multiList.CheckElementExists(14))
                GameSettings::SetGameSettings(playerId, eGameSetting_GameChat, m_multiList.GetCheckboxValue(14));
        }
        break;
    case 2:
    {
        int playerId = m_playerId;
        if (m_multiList.CheckElementExists(15))
            GameSettings::SetGameSettings(playerId, eGameSetting_Clouds, m_multiList.GetCheckboxValue(15));
    }
        {
            int playerId = m_playerId;
            if (m_multiList.CheckElementExists(16))
                GameSettings::SetGameSettings(playerId, eGameSetting_CustomSkinAnim, m_multiList.GetCheckboxValue(16));
        }
        {
            int playerId = m_playerId;
            if (m_multiList.CheckElementExists(18))
                GameSettings::SetGameSettings(playerId, eGameSetting_ShowGlideGhost, m_multiList.GetCheckboxValue(18));
        }
        {
            int playerId = m_playerId;
            if (m_multiList.CheckElementExists(19))
                GameSettings::SetGameSettings(playerId, eGameSetting_ShowGlideGhostPath,
                                              m_multiList.GetCheckboxValue(19));
        }
        break;
    case 3:
    {
        int playerId = m_playerId;
        if (m_multiList.CheckElementExists(20))
            GameSettings::SetGameSettings(playerId, eGameSetting_DisplayHUD, m_multiList.GetCheckboxValue(20));
    }
        {
            int playerId = m_playerId;
            if (m_multiList.CheckElementExists(21))
                GameSettings::SetGameSettings(playerId, eGameSetting_DisplayHand, m_multiList.GetCheckboxValue(21));
        }
        {
            int playerId = m_playerId;
            if (m_multiList.CheckElementExists(22))
                GameSettings::SetGameSettings(playerId, eGameSetting_DisplayGameMessages,
                                              m_multiList.GetCheckboxValue(22));
        }
        {
            int playerId = m_playerId;
            if (m_multiList.CheckElementExists(23))
                GameSettings::SetGameSettings(playerId, eGameSetting_DisplaySaveIcon, m_multiList.GetCheckboxValue(23));
        }
        {
            int playerId = m_playerId;
            if (m_multiList.CheckElementExists(29))
                GameSettings::SetGameSettings(playerId, eGameSetting_SplitscreenNicknames,
                                              m_multiList.GetCheckboxValue(29));
        }
        {
            int playerId = m_playerId;
            if (m_multiList.CheckElementExists(28))
                GameSettings::SetGameSettings(playerId, eGameSetting_DisplayMii, m_multiList.GetCheckboxValue(28));
        }
        {
            int playerId = m_playerId;
            if (m_multiList.CheckElementExists(26))
                GameSettings::SetGameSettings(playerId, eGameSetting_AnimatedCharacter,
                                              m_multiList.GetCheckboxValue(26));
        }
        {
            int playerId = m_playerId;
            if (m_multiList.CheckElementExists(30))
                GameSettings::SetGameSettings(playerId, eGameSetting_ClassicCrafting, m_multiList.GetCheckboxValue(30));
        }
        {
            int playerId = m_playerId;
            if (m_multiList.CheckElementExists(25))
                GameSettings::SetGameSettings(playerId, eGameSetting_Tooltips, m_multiList.GetCheckboxValue(25));
        }
        break;
    }
}

void UIScene_SettingsListMenu::tick()
{
    UIScene::tick();
    if (m_saveRequested)
    {
        m_saveRequested = false;
        setGameSettings();
        gConsoleUIController.NavigateToScene(m_playerId, EUIScene_LanguageSelector, 0, EUILayer_Scene,
                                             EUIGroup_PlayerDefault);
    }
}

void UIScene_SettingsListMenu::updateTooltips()
{
    gConsoleUIController.SetTooltips(m_playerId, IDS_TOOLTIPS_SELECT, IDS_TOOLTIPS_BACK, -1, -1, -1, -1, -1, -1, -1, -1,
                                     -1, -1, -1, -1, -1, -1, false, false);
}

void UIScene_SettingsListMenu::handlePreUnloadForReload()
{
    setGameSettings();
}

void UIScene_SettingsListMenu::handleReload()
{
    Init();
}

void UIScene_SettingsListMenu::updateComponents()
{
    bool notInGame = Minecraft::GetInstance()->mLevel == 0;
    if (notInGame)
    {
        m_layer->showComponent(m_playerId, EUIScene_Panorama, true);
        m_layer->showComponent(m_playerId, EUIScene_Logo, true);
    }
    else
    {
        m_layer->showComponent(m_playerId, EUIScene_Panorama, false);
        m_layer->showComponent(m_playerId, EUIScene_Logo, gApp.GetLocalPlayerCount() == 1);
    }
}

std::wstring UIScene_SettingsListMenu::getMoviePath()
{
    return gApp.GetLocalPlayerCount() > 1 ? std::wstring(L"MultilistMenuSplit") : std::wstring(L"MultilistMenu");
}

void UIScene_SettingsListMenu::handleInput(int playerId, int action, bool repeat, bool pressed, bool released,
                                           bool& handled)
{
    gConsoleUIController.AnimateKeyPress(playerId, action, repeat, pressed, released);
    switch (action)
    {
    case EControllerActions_MenuCancel:
        if (pressed)
        {
            setGameSettings();
            navigateBack();
            handled = true;
        }
        break;
    case EControllerActions_MenuUp:
    case EControllerActions_MenuDown:
    case EControllerActions_MenuRight:
    case EControllerActions_MenuLeft:
    case EControllerActions_MenuPageUp:
    case EControllerActions_MenuPageDown:
    case EControllerActions_MenuOtherStickUp:
    case EControllerActions_MenuOtherStickDown:
    case EControllerActions_MenuOk:
        sendInputToMovie(action, repeat, pressed, released);
        break;
    }
}

void UIScene_SettingsListMenu::handlePress(int, int childId)
{
    gConsoleUIController.PlayUISFX(SoundEvents::UI_PRESS);
    if (childId == 5)
        m_saveRequested = true;
}

void UIScene_SettingsListMenu::handleCheckboxElementToggled(int, int, bool)
{
    gConsoleUIController.PlayUISFX(SoundEvents::UI_PRESS);
}

void UIScene_SettingsListMenu::handleSliderElementMove(int, int id, int value)
{
    gConsoleUIController.PlayUISFX(SoundEvents::UI_SCROLL);
    wchar_t text[256];
    switch (id)
    {
    case 6:
        GameSettings::SetGameSettings(m_playerId, eGameSetting_Autosave, value);
        m_multiList.SetItemLabel(6, UIString(m_skinNames[value]), false);
        gApp.SetAutosaveTimerTime();
        break;
    case 7:
        GameSettings::SetGameSettings(m_playerId, eGameSetting_GameSensitivity, value);
        std::swprintf(text, 256, L"%ls: %d%%", L10N::GetString(IDS_SLIDER_SENSITIVITY_INGAME),
                      GameSettings::GetGameSettings(m_playerId, eGameSetting_GameSensitivity));
        m_multiList.SetItemLabel(7, UIString(text), false);
        break;
    case 8:
        GameSettings::SetGameSettings(m_playerId, eGameSetting_Difficulty, value);
        m_multiList.SetItemLabel(8, UIString(m_difficultyNames[value]), false);
        break;
    case 10:
        GameSettings::SetGameSettings(m_playerId, eGameSetting_MusicVolume, value);
        std::swprintf(text, 256, L"%ls: %d%%", L10N::GetString(IDS_SLIDER_MUSIC), value);
        m_multiList.SetItemLabel(10, UIString(text), false);
        break;
    case 11:
        GameSettings::SetGameSettings(m_playerId, eGameSetting_SoundVolume, value);
        std::swprintf(text, 256, L"%ls: %d%%", L10N::GetString(IDS_SLIDER_SOUND), value);
        m_multiList.SetItemLabel(11, UIString(text), false);
        break;
    case 17:
        GameSettings::SetGameSettings(m_playerId, eGameSetting_Gamma, value);
        std::swprintf(text, 256, L"%ls: %d%%", L10N::GetString(IDS_SLIDER_GAMMA), value);
        m_multiList.SetItemLabel(17, UIString(text), false);
        break;
    case 24:
        GameSettings::SetGameSettings(m_playerId, eGameSetting_InterfaceOpacity, value);
        std::swprintf(text, 256, L"%ls: %d%%", L10N::GetString(IDS_SLIDER_INTERFACEOPACITY), value);
        m_multiList.SetItemLabel(24, UIString(text), false);
        break;
    case 31:
        std::swprintf(text, 256, L"%ls: %d", L10N::GetString(IDS_SLIDER_UISIZE), value);
        m_multiList.SetItemLabel(31, UIString(text), false);
        if (value != GameSettings::GetGameSettings(m_playerId, eGameSetting_HUDSize) + 1)
        {
            GameSettings::SetGameSettings(m_playerId, eGameSetting_HUDSize, value - 1);
            gConsoleUIController.UpdateSelectedItemPos(m_playerId);
        }
        break;
    case 32:
        std::swprintf(text, 256, L"%ls: %d", L10N::GetString(IDS_SLIDER_UISIZESPLITSCREEN), value);
        m_multiList.SetItemLabel(32, UIString(text), false);
        if (value != GameSettings::GetGameSettings(m_playerId, eGameSetting_HUDSizeSplitscreen) + 1)
        {
            GameSettings::SetGameSettings(m_playerId, eGameSetting_HUDSizeSplitscreen, value - 1);
            gConsoleUIController.UpdateSelectedItemPos(m_playerId);
        }
        break;
    case 27:
        GameSettings::SetGameSettings(m_playerId, eGameSetting_InterfaceSensitivity, value);
        std::swprintf(text, 256, L"%ls: %d%%", L10N::GetString(IDS_SLIDER_SENSITIVITY_INMENU), value);
        m_multiList.SetItemLabel(27, UIString(text), false);
        break;
    }
}

int UIScene_SettingsListMenu::getSceneType()
{
    return EUIScene_SettingsListMenu;
}
