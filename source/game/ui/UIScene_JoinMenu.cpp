#include "game/ui/UIScene_JoinMenu.hpp"
#include "game/ui/UIScene_QuadrantSignin.hpp"
#include "game/core/StringIds.hpp"
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include "game/client/audio/SoundEvents.hpp"
#include "game/core/CConsoleMinecraftApp.hpp"
#include "game/core/ConsoleUIController.hpp"
#include "game/core/GameType.hpp"
#include "game/core/L10N.hpp"
#include "game/core/UnityClient6.hpp"
#include "game/core/Minecraft.hpp"
#include "game/minigame/MiniGameDef.hpp"
#include "game/network/CGameNetworkManager.hpp"
#include "game/ui/UIString.hpp"
#include "game/ui/UILayer.hpp"
#include "game/ui/fuiFile.hpp"
#include "game/core/GameSettings.hpp"
#include <cwctype>
#include "game/core/IUIScene_LoadCreateJoin.hpp"
#include "game/core/StringFormat.hpp"
#include "game/core/StringUtils.hpp"

namespace
{
std::wstring MotdHtml(const std::wstring& motd)
{
    std::wstring html;
    std::wstring run;
    eMinecraftColour colour = eMinecraftColour_NOTSET;
    int hexColour = -1;
    for (size_t index = 0; index <= motd.size(); ++index)
    {
        const wchar_t character = index < motd.size() ? motd[index] : L'\n';
        const bool code = character == 0xA7 && index + 1 < motd.size();
        if ((code || character == L'\n') && !run.empty())
        {
            for (size_t edge = 0; edge < run.size() && run[edge] == L' '; ++edge)
                run[edge] = static_cast<wchar_t>(0xA0);
            for (size_t edge = run.size(); edge > 0 && run[edge - 1] == L' '; --edge)
                run[edge - 1] = static_cast<wchar_t>(0xA0);
            if (hexColour >= 0)
                html += formatwstr(L"<font color=\"#%06x\">", hexColour) + escapeXML(run) + L"</font>";
            else
                html += HtmlString(run, colour, false, false, 0).ToString();
            run.clear();
        }
        if (code)
        {
            const wchar_t value = static_cast<wchar_t>(towlower(motd[++index]));
            if (value == L'x' && index + 12 < motd.size())
            {
                int rgb = 0;
                bool valid = true;
                for (size_t digit = 0; digit < 6 && valid; ++digit)
                {
                    const wchar_t hex = static_cast<wchar_t>(towlower(motd[index + 2 + digit * 2]));
                    valid = motd[index + 1 + digit * 2] == 0xA7 && iswxdigit(hex);
                    rgb = (rgb << 4) | (hex <= L'9' ? hex - L'0' : hex - L'a' + 10);
                }
                if (valid)
                {
                    hexColour = rgb;
                    index += 12;
                }
            }
            else if (value >= L'0' && value <= L'9')
            {
                colour = static_cast<eMinecraftColour>(eMinecraftColour_HTMLColor_0 + (value - L'0'));
                hexColour = -1;
            }
            else if (value >= L'a' && value <= L'f')
            {
                colour = static_cast<eMinecraftColour>(eMinecraftColour_HTMLColor_a + (value - L'a'));
                hexColour = -1;
            }
            else if (value == L'r')
            {
                colour = eMinecraftColour_NOTSET;
                hexColour = -1;
            }
        }
        else if (character == L'\n')
        {
            if (index < motd.size())
                html += L"<br>";
        }
        else if (character != 0xA7)
        {
            run.push_back(character);
        }
    }
    return html;
}

std::wstring SessionInfoHtml(const FriendSessionInfo* info, std::vector<HtmlString>& descriptions)
{
    int players = info->m_gameSessionData.m_numPlayers;
    int maxPlayers = info->m_maxPlayers;
    if (info->m_dedicated)
    {
        players = players > 0 ? players - 1 : 0;
        maxPlayers = maxPlayers > 0 ? maxPlayers - 1 : 0;
    }
    std::wstring count = maxPlayers > 0 ? formatwstr(L"%d/%d", players, maxPlayers) : formatwstr(L"%d", players);
    descriptions.insert(descriptions.begin(),
                        HtmlString(formatwstr(L"%ls: %ls", L10N::GetString(IDS_PLAYERS), count.c_str()),
                                   eMinecraftColour_NOTSET, false, false, 0));
    std::wstring html = MotdHtml(info->m_motd);
    if (!html.empty())
        html += L"<br><br>";
    return html + HtmlString::Compose(&descriptions);
}
}

UIScene_JoinMenu::UIScene_JoinMenu(int sceneType, void* data, UILayer* layer) : UIScene(sceneType, layer)
{
    initialiseMovie();
    m_gameInfo.init(UIString(L""));
    JoinSceneData* value = static_cast<JoinSceneData*>(data);
    m_friendSessionInfo = value->session;
    m_matchmaking = value->miniGames;
    m_friendSessionUpdated = false;
    m_error = false;
    m_initialised = false;
}

unsigned int UIScene_JoinMenu::GetCustomGameModeID()
{
    return m_friendSessionInfo->m_gameSessionData.m_miniGameId;
}
unsigned int UIScene_JoinMenu::mapElementsAndNames()
{
    fuiRenderNode* root = m_movie->getRootNode();
    {
        m_joinButton.setupControl(this, root, "JoinGame");
        m_controls.push_back(&m_joinButton);
    }
    {
        m_gamePlayers.setupControl(this, root, "GamePlayers");
        m_controls.push_back(&m_gamePlayers);
    }
    {
        m_gameInfo.setupControl(this, root, "GameInfo");
        m_controls.push_back(&m_gameInfo);
    }
    return 1;
}
void UIScene_JoinMenu::friendSessionUpdated(bool updated, void* data)
{
    UIScene_JoinMenu* scene = static_cast<UIScene_JoinMenu*>(data);
    gConsoleUIController.NavigateBack(scene->m_playerId, false, EUIScene_None, EUILayer_Count);
    if (updated)
        scene->m_friendSessionUpdated = true;
    else
        scene->m_error = true;
}
void UIScene_JoinMenu::AddDescriptionForOption(std::vector<HtmlString>& descriptions, eGameHostOption option,
                                               int options)
{
    std::wstring value = L"";
    std::wstring on = L10N::GetString(IDS_ON);
    std::wstring off = L10N::GetString(IDS_OFF);
    int title = -1;
    switch (option)
    {
    case eGameHostOption_Difficulty:
        title = IDS_LABEL_DIFFICULTY;
        switch (gApp.GetGameHostOption(options, eGameHostOption_Difficulty))
        {
        case 1:
            value = L10N::GetString(IDS_DIFFICULTY_TITLE_EASY);
            break;
        case 2:
            value = L10N::GetString(IDS_DIFFICULTY_TITLE_NORMAL);
            break;
        case 3:
            value = L10N::GetString(IDS_DIFFICULTY_TITLE_HARD);
            break;
        case 0:
            value = L10N::GetString(IDS_DIFFICULTY_TITLE_PEACEFUL);
            break;
        default:
            value = L10N::GetString(IDS_DIFFICULTY_TITLE_PEACEFUL);
            break;
        }
        break;
    case eGameHostOption_Gamertags:
        title = IDS_LABEL_GAMERTAGS;
        value = gApp.GetGameHostOption(options, eGameHostOption_Gamertags) ? on : off;
        break;
    case eGameHostOption_Structures:
        title = IDS_LABEL_STRUCTURES;
        value = gApp.GetGameHostOption(options, eGameHostOption_Structures) ? on : off;
        break;
    case eGameHostOption_LevelType:
        title = IDS_LABEL_LEVEL_TYPE;
        value = L10N::GetString(gApp.GetGameHostOption(options, eGameHostOption_LevelType) ? IDS_LEVELTYPE_SUPERFLAT
                                                                                           : IDS_LEVELTYPE_NORMAL);
        break;
    case eGameHostOption_PvP:
        title = IDS_LABEL_PvP;
        value = gApp.GetGameHostOption(options, eGameHostOption_PvP) ? on : off;
        break;
    case eGameHostOption_TrustPlayers:
        title = IDS_LABEL_TRUST;
        value = gApp.GetGameHostOption(options, eGameHostOption_TrustPlayers) ? on : off;
        break;
    case eGameHostOption_TNT:
        title = IDS_LABEL_TNT;
        value = gApp.GetGameHostOption(options, eGameHostOption_TNT) ? on : off;
        break;
    case eGameHostOption_FireSpreads:
        title = IDS_LABEL_FIRE_SPREADS;
        value = gApp.GetGameHostOption(options, eGameHostOption_FireSpreads) ? on : off;
        break;
    default:
        break;
    }
    if (title != -1)
    {
        descriptions.push_back(HtmlString(formatwstr(L"%ls %ls", L10N::GetString(title), value.c_str()),
                                          eMinecraftColour_NOTSET, false, false, 0));
    }
}
int UIScene_JoinMenu::ErrorDialogReturned(void* data, int, C4JStorage::EMessageResult)
{
    UIScene_JoinMenu* scene = static_cast<UIScene_JoinMenu*>(data);
    gConsoleUIController.NavigateBack(scene->m_playerId, false, EUIScene_None, EUILayer_Count);
    return 0;
}
void UIScene_JoinMenu::tick()
{
    if (!m_initialised)
    {
        gConsoleUIController.NavigateToScene(m_playerId, EUIScene_Timer, NULL, EUILayer_Scene, EUIGroup_PlayerDefault);
        CGameNetworkManager::sInstance.GetFullFriendSessionInfo(m_friendSessionInfo, friendSessionUpdated, this);
        m_initialised = true;
    }
    if (m_friendSessionUpdated)
    {
        m_friendSessionUpdated = false;
        m_joinButton.init(UIString(L10N::GetString(IDS_JOIN_GAME)), 0);
        m_gamePlayers.init(1);
        for (int i = 0; i < GameSessionData::MAX_SESSION_PLAYERS; ++i)
        {
            if (m_friendSessionInfo->m_gameSessionData.m_playerGamertags[i] != GameSessionData::Gamertag())
            {
                const char* playerName = m_friendSessionInfo->m_gameSessionData.m_playerGamertags[i].m_name;
                if (m_friendSessionInfo->m_dedicated && i == 0)
                    continue;
                wchar_t displayName[32];
                char gamertag[32];
                if (strcmp(playerName, m_friendSessionInfo->m_gameSessionData.m_hostGamertag.m_name) == 0)
                {
                    char hostGamertag[32];
                    memcpy(hostGamertag, playerName, GameSessionData::MAX_GAMERTAG_LENGTH);
                    hostGamertag[GameSessionData::MAX_GAMERTAG_LENGTH + 1] = 0;
                    wchar_t hostName[32];
                    mbstowcs(hostName, hostGamertag, 32);
                    swprintf(displayName, 32, L"%ls %ls", hostName, L10N::GetString(0x566a7b93));
                }
                else
                {
                    memcpy(gamertag, playerName, GameSessionData::MAX_GAMERTAG_LENGTH);
                    gamertag[GameSessionData::MAX_GAMERTAG_LENGTH + 1] = 0;
                    mbstowcs(displayName, gamertag, 32);
                }
                m_gamePlayers.addItem(std::wstring(displayName));
            }
            else
                break;
        }

        if (m_friendSessionInfo->m_gameSessionData.m_gameModeId == static_cast<unsigned int>(GameType::LOBBY->getId()))
        {
            EMiniGameId miniGameId = static_cast<EMiniGameId>(m_friendSessionInfo->m_gameSessionData.m_miniGameId);
            std::vector<HtmlString> descriptions;
            MiniGameDef* miniGame = MiniGameDef::GetCustomGameModeById(miniGameId, true);
            miniGame->GetGameSettingsDescription(descriptions,
                                                 m_friendSessionInfo->m_gameSessionData.m_customGameModeHostOptions);
            m_gameInfo.setLabel(UIString(SessionInfoHtml(m_friendSessionInfo, descriptions)), false, false);
        }
        else
        {
            unsigned int hostOptions = m_friendSessionInfo->m_gameSessionData.m_gameHostOptions;
            std::vector<HtmlString> descriptions;
            std::wstring gameModeName = L"";
            unsigned int gameModeId = m_friendSessionInfo->m_gameSessionData.m_gameModeId;
            if (gameModeId == static_cast<unsigned int>(GameType::CREATIVE->getId()))
                gameModeName = L10N::GetString(IDS_CREATIVE);
            else if (gameModeId == static_cast<unsigned int>(GameType::ADVENTURE->getId()))
                gameModeName = L10N::GetString(IDS_ADVENTURE);
            else
                gameModeName = L10N::GetString(IDS_SURVIVAL);
            descriptions.push_back(
                HtmlString(formatwstr(L"%ls %ls", L10N::GetString(IDS_LABEL_GAME_TYPE), gameModeName.c_str()),
                           eMinecraftColour_NOTSET, false, false, 0));
            AddDescriptionForOption(descriptions, eGameHostOption_Difficulty, hostOptions);
            AddDescriptionForOption(descriptions, eGameHostOption_Gamertags, hostOptions);
            AddDescriptionForOption(descriptions, eGameHostOption_LevelType, hostOptions);
            AddDescriptionForOption(descriptions, eGameHostOption_PvP, hostOptions);
            AddDescriptionForOption(descriptions, eGameHostOption_TrustPlayers, hostOptions);
            AddDescriptionForOption(descriptions, eGameHostOption_TNT, hostOptions);
            AddDescriptionForOption(descriptions, eGameHostOption_FireSpreads, hostOptions);
            m_gameInfo.setLabel(UIString(SessionInfoHtml(m_friendSessionInfo, descriptions)), false, false);
        }
        m_joining = false;
        gApp.SetLiveLinkRequired(true);
        addTimer(0, 30000);
    }
    if (m_error)
    {
        m_joinButton.init(UIString(L10N::GetString(IDS_JOIN_GAME)), 0);
        m_gamePlayers.init(1);
        m_gameInfo.setLabel(UIString(L""), false, false);
        m_error = false;
        int parameter = IDS_CONFIRM_OK;
        gConsoleUIController.RequestErrorMessage(IDS_ERROR_NETWORK_TITLE, IDS_ERROR_NETWORK, &parameter, 1, m_playerId,
                                                 ErrorDialogReturned, this, NULL, true);
    }
    UIScene::tick();
}
void UIScene_JoinMenu::updateTooltips()
{
    int tooltip = -1;
    if (getControlFocus() != 1)
        tooltip = IDS_TOOLTIPS_SELECT;
    gConsoleUIController.SetTooltips(0, tooltip, IDS_TOOLTIPS_BACK, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                                     -1, false, false);
}
void UIScene_JoinMenu::updateComponents()
{
    m_layer->showComponent(m_playerId, EUIScene_Panorama, true);
    m_layer->showComponent(m_playerId, EUIScene_Logo, true);
}
std::wstring UIScene_JoinMenu::getMoviePath()
{
    return std::wstring(L"JoinMenu");
}
void UIScene_JoinMenu::handleInput(int, int action, bool repeat, bool pressed, bool released, bool& handled)
{
    if (m_joining)
        return;
    gConsoleUIController.AnimateKeyPress(m_playerId, action, repeat, pressed, released);
    switch (action)
    {
    case EControllerActions_MenuCancel:
        if (pressed)
        {
            navigateBack();
            handled = true;
        }
        break;
    case EControllerActions_MenuOk:
        if (getControlFocus() == 1)
        {
            handled = true;
            break;
        }
    case EControllerActions_MenuUp:
    case EControllerActions_MenuDown:
    case EControllerActions_MenuPageUp:
    case EControllerActions_MenuPageDown:
        sendInputToMovie(action, repeat, pressed, released);
        handled = true;
        break;
    }
}
void UIScene_JoinMenu::handleFocusChange(int focus, int childFocus)
{
    if (focus == 1)
        m_gamePlayers.updateChildFocus(childFocus);
    updateTooltips();
}
void UIScene_JoinMenu::JoinGame(UIScene_JoinMenu* scene)
{
    unsigned int playerMask = 0;
    bool multiplayerRestricted = false;
    bool allPlayersSignedInLive = true;

    gProfile->SetLockedProfile(0);
    if (scene->m_friendSessionInfo != NULL)
        CGameNetworkManager::sInstance.m_joiningMiniGameId = static_cast<EMiniGameId>(
            scene->m_friendSessionInfo->m_gameSessionData.m_miniGameId);

    if (gApp.IsLocalMultiplayerAvailable())
    {
        for (int userIndex = 0; userIndex < 4; ++userIndex)
        {
            if (gProfile->IsSignedIn(userIndex))
            {
                if (allPlayersSignedInLive)
                    gProfile->IsSignedInLive(userIndex);
                if (!gProfile->AllowedToPlayMultiplayer(userIndex))
                    multiplayerRestricted = true;
                playerMask |= CGameNetworkManager::GetLocalPlayerMask(userIndex);
                allPlayersSignedInLive = allPlayersSignedInLive && gProfile->IsSignedInLive(userIndex);
            }
        }
    }
    else
    {
        if (gProfile->IsSignedIn(gProfile->GetPrimaryPad()))
        {
            if (!gProfile->AllowedToPlayMultiplayer(gProfile->GetPrimaryPad()))
                multiplayerRestricted = true;
            playerMask = CGameNetworkManager::GetLocalPlayerMask(gProfile->GetPrimaryPad());
            allPlayersSignedInLive = gProfile->IsSignedInLive(gProfile->GetPrimaryPad());
        }
    }

    if (!allPlayersSignedInLive)
    {
        scene->m_joining = false;
        int parameter = IDS_CONFIRM_OK;
        gConsoleUIController.RequestErrorMessage(IDS_PRO_NOTONLINE_TITLE, IDS_PRO_NOTONLINE_TEXT, &parameter, 1,
                                                 gProfile->GetPrimaryPad(), NULL, NULL, NULL, true);
        return;
    }
    if (scene->IsUGCRestrictedMode())
    {
        scene->setVisible(true);
        scene->m_joining = false;
        gConsoleUIController.RequestUGCMessageBox(IDS_CONNECTION_FAILED, STRING_JOIN_CONTENT_PRIVILEGE_TOO_RESTRICTIVE,
                                                  -1, NULL, NULL);
        return;
    }
    if (multiplayerRestricted)
    {
        scene->setVisible(true);
        scene->m_joining = false;
        int parameter = IDS_CONFIRM_OK;
        gConsoleUIController.RequestErrorMessage(IDS_NO_MULTIPLAYER_PRIVILEGE_TITLE,
                                                 IDS_NO_MULTIPLAYER_PRIVILEGE_JOIN_TEXT, &parameter, 1,
                                                 gProfile->GetPrimaryPad(), NULL, NULL, NULL, true);
        return;
    }

    if (scene->m_friendSessionInfo->m_gameSessionData.m_miniGameId != eMiniGameId_Any)
    {
        gApp.SetGameHostOption(eGameHostOption_MiniGame, 1);
        gApp.SetFirstMiniGameType(static_cast<EMiniGameId>(scene->m_friendSessionInfo->m_gameSessionData.m_miniGameId));
    }

    int result = CGameNetworkManager::sInstance.JoinGame(scene->m_friendSessionInfo, playerMask, scene->m_matchmaking);
    gApp.SetLiveLinkRequired(false);

    if (result != 0)
    {
        int exitReasonTitleId = IDS_CONNECTION_FAILED;
        int exitReasonStringId = -1;
        switch (result)
        {
        case 2:
            exitReasonStringId = IDS_DISCONNECTED_SERVER_FULL;
            break;
        case 3:
            exitReasonStringId = IDS_ERROR_NETWORK;
            break;
        case 4:
            exitReasonTitleId = IDS_NO_MULTIPLAYER_PRIVILEGE_TITLE;
            exitReasonStringId = IDS_NO_MULTIPLAYER_PRIVILEGE_JOIN_TEXT;
            break;
        }

        if (exitReasonStringId == -1)
        {
            gConsoleUIController.NavigateBack(scene->m_playerId, false, EUIScene_None, EUILayer_Count);
        }
        else
        {
            int parameter = IDS_CONFIRM_OK;
            gConsoleUIController.RequestErrorMessage(exitReasonTitleId, exitReasonStringId, &parameter, 1,
                                                     gProfile->GetPrimaryPad(), NULL, NULL, NULL, true);
            gConsoleUIController.NavigateToHomeMenu(false);
        }
    }
    else
    {
        telemetryManagerInstance->PrepareMiniGameHostOptions(scene->m_playerId, NULL, eTelemetryJoinMethod_FriendsList);
        Minecraft::GetInstance()->SetJoiningFromFriendsTabOrInvite(false);
    }
}
int UIScene_JoinMenu::WiiUDRCWarningReturned(void* data, int, C4JStorage::EMessageResult result)
{
    UIScene_JoinMenu* scene = static_cast<UIScene_JoinMenu*>(data);
    if (result != C4JStorage::EMessageResult_Decline)
    {
        if (result == C4JStorage::EMessageResult_Accept)
            JoinGame(scene);
    }
    scene->m_joining = false;
    return 0;
}
int UIScene_JoinMenu::StartGame_SignInReturned(void* context, bool signedIn, int userIndex)
{
    UIScene_JoinMenu* scene = static_cast<UIScene_JoinMenu*>(
        gConsoleUIController.GetSceneFromCallbackId(static_cast<unsigned int>(reinterpret_cast<uintptr_t>(context))));
    if (scene != NULL)
    {
        if (signedIn && gProfile->IsSignedIn(userIndex))
            JoinGame(scene);
        else
            scene->m_joining = false;
    }
    return 0;
}
void UIScene_JoinMenu::StartSharedLaunchFlow()
{
    if (!gApp.IsLocalMultiplayerAvailable())
    {
        JoinGame(this);
        return;
    }
    if (gApp.IsDrcPreventingSplitscreen())
    {
        int parameters[2] = {0x1a65d275, 0x7a458ed2};
        gConsoleUIController.RequestAlertMessage(0x337f2cfc, 0x6f86265b, parameters, 2, gProfile->GetPrimaryPad(),
                                                 WiiUDRCWarningReturned, this, NULL, true);
        return;
    }
    UIScene_QuadrantSignin::SignInInfo signInData;
    signInData.callback = StartGame_SignInReturned;
    signInData.context = reinterpret_cast<void*>(GetCallbackUniqueId());
    signInData.onlineGame = true;
    signInData.returnToHome = false;
    gConsoleUIController.NavigateToScene(gProfile->GetPrimaryPad(), EUIScene_QuadrantSignin, &signInData,
                                         EUILayer_Scene, EUIGroup_PlayerDefault);
}
void UIScene_JoinMenu::handlePress(int controlId, int)
{
    switch (controlId)
    {
    case eControl_JoinGame:
        m_joining = true;
        gConsoleUIController.PlayUISFX(SoundEvents::UI_PRESS);
        StartSharedLaunchFlow();
        break;
    case eControl_GamePlayers:
        break;
    }
}
int UIScene_JoinMenu::getSceneType()
{
    return EUIScene_LoadMenu;
}
