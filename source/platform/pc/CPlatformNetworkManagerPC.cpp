#include "platform/pc/CPlatformNetworkManagerPC.hpp"

#include <string.h>

#include "game/core/CMinecraftApp.hpp"
#include "game/core/CConsoleMinecraftApp.hpp"
#include "game/core/GameModeOptions.hpp"
#include "game/core/Minecraft.hpp"
#include "game/core/Socket.hpp"
#include "game/minigame/MiniGameDef.hpp"
#include "game/network/CGameNetworkManager.hpp"
#include "game/network/ClientboundPreLoginPacket.hpp"
#include "platform/C4JEventImpl.hpp"
#include "platform/C4JThreadImpl.hpp"
#include "platform/CInput.hpp"
#include "platform/CProfile.hpp"
#include "platform/pc/NetworkPlayerPC.hpp"

namespace
{
const unsigned short cSessionDataVersion = 0x881;
const int cMaxLocalPlayers = 4;

void CopyGamertag(GameSessionData::Gamertag& gamertag, const wchar_t* name)
{
    memset(gamertag.m_name, 0, sizeof(gamertag.m_name));
    if (name == 0)
        return;
    for (unsigned int index = 0; index + 1 < sizeof(gamertag.m_name) && name[index] != 0; ++index)
        gamertag.m_name[index] = name[index] < 0x80 ? (char)name[index] : '?';
}
}

CPlatformNetworkManagerPC::CPlatformNetworkManagerPC()
    : m_gameNetworkManager(0),
      m_internalState(eState_Uninitialised),
      m_state(eState_Uninitialised),
      m_maxSystemFlags(0),
      m_nextSmallId(0),
      m_host(false),
      m_inSession(false),
      m_localGame(false),
      m_privateGame(false),
      m_matchmakingSession(false),
      m_matchmaking(false),
      m_leavingGame(false),
      m_leavePending(false),
      m_sessionData(),
      m_sessionsUpdatedCallback(0),
      m_sessionsUpdatedCallbackData(0),
      m_connectPending(false),
      m_connectShowProgress(false),
      m_connectCallback(0),
      m_connectCallbackData(0),
      m_sessionInfoPending(false),
      m_sessionInfoCallback(0),
      m_sessionInfoCallbackData(0)
{
    InitializeCriticalSection(&m_lock);
    for (int index = 0; index < cMaxLocalPlayers; ++index)
    {
        m_playerChangedCallbacks[index] = 0;
        m_playerChangedCallbackData[index] = 0;
    }
}

void CPlatformNetworkManagerPC::Initialise(CGameNetworkManager* manager, int maxSystemFlags)
{
    EnterCriticalSection(&m_lock);
    m_gameNetworkManager = manager;
    m_maxSystemFlags = maxSystemFlags < 0 ? 0 : (unsigned int)maxSystemFlags;
    m_leavingGame = false;
    m_leavePending = false;
    m_localGame = false;
    m_sessionsUpdatedCallback = 0;
    m_sessionsUpdatedCallbackData = 0;
    for (int index = 0; index < cMaxLocalPlayers; ++index)
    {
        m_playerChangedCallbacks[index] = 0;
        m_playerChangedCallbackData[index] = 0;
    }
    SetState(eState_Idle);
    LeaveCriticalSection(&m_lock);
}

void CPlatformNetworkManagerPC::Terminate()
{
}

int CPlatformNetworkManagerPC::GetJoiningReadyPercentage()
{
    return 100;
}

int CPlatformNetworkManagerPC::CorrectErrorIDS(int errorIds)
{
    return errorIds;
}

void CPlatformNetworkManagerPC::DoWork()
{
    bool leave = false;
    EnterCriticalSection(&m_lock);
    if (m_leavePending)
    {
        m_leavePending = false;
        leave = true;
    }
    LeaveCriticalSection(&m_lock);
    if (leave)
        LeaveRoom();

    EnterCriticalSection(&m_lock);
    bool sessionInfoPending = m_sessionInfoPending;
    void (*sessionInfoCallback)(bool, void*) = m_sessionInfoCallback;
    void* sessionInfoCallbackData = m_sessionInfoCallbackData;
    m_sessionInfoPending = false;
    m_sessionInfoCallback = 0;
    m_sessionInfoCallbackData = 0;
    LeaveCriticalSection(&m_lock);
    if (sessionInfoPending && sessionInfoCallback != 0)
        sessionInfoCallback(false, sessionInfoCallbackData);

    ProcessStateChanges();

    EnterCriticalSection(&m_lock);
    bool connectPending = m_connectPending;
    bool connectShowProgress = m_connectShowProgress;
    int (*connectCallback)(void*, bool, int) = m_connectCallback;
    void* connectCallbackData = m_connectCallbackData;
    m_connectPending = false;
    m_connectShowProgress = false;
    m_connectCallback = 0;
    m_connectCallbackData = 0;
    LeaveCriticalSection(&m_lock);
    if (connectPending && connectShowProgress && connectCallback != 0)
        connectCallback(connectCallbackData, false, 0);
}

int CPlatformNetworkManagerPC::GetPlayerCount()
{
    EnterCriticalSection(&m_lock);
    int count = m_roomPlayers.empty() ? 1 : (int)m_roomPlayers.size();
    LeaveCriticalSection(&m_lock);
    return count;
}

int CPlatformNetworkManagerPC::GetOnlinePlayerCount()
{
    EnterCriticalSection(&m_lock);
    int count = 0;
    for (unsigned int index = 0; index < m_roomPlayers.size(); ++index)
        if (!m_roomPlayers[index]->IsLocal())
            ++count;
    LeaveCriticalSection(&m_lock);
    return count;
}

int CPlatformNetworkManagerPC::GetLocalPlayerMask(int userIndex)
{
    return 1 << userIndex;
}

bool CPlatformNetworkManagerPC::AddLocalPlayerByUserIndex(int userIndex)
{
    if (userIndex < 0 || userIndex >= cMaxLocalPlayers)
        return false;
    EnterCriticalSection(&m_lock);
    if (!m_host || m_roomPlayers.size() >= CGameNetworkManager::sInstance.GetMaxPlayers()
        || FindLocalPlayer(userIndex) != 0)
    {
        LeaveCriticalSection(&m_lock);
        return false;
    }
    NetworkPlayerPC* player = AddRoomPlayer(userIndex, false);
    HandlePlayerJoined(player);
    ResyncSessionPlayers();
    UpdateLocalMultiplayer();
    LeaveCriticalSection(&m_lock);
    return true;
}

int CPlatformNetworkManagerPC::RemovePlayerOnSocketClosedThreadProc(void* data)
{
    PendingRemoval* removal = static_cast<PendingRemoval*>(data);
    removal->socket->mCloseEvent->WaitForSignal(-1);
    CPlatformNetworkManagerPC* manager = removal->manager;
    EnterCriticalSection(&manager->m_lock);
    bool present = manager->ContainsPlayer(removal->player);
    if (present && removal->player->GetSocket() == removal->socket)
        removal->player->SetSocket(0);
    LeaveCriticalSection(&manager->m_lock);
    delete removal->socket;
    int result = present ? manager->RemoveLocalPlayer(removal->player) : 1;
    delete removal;
    return result;
}

bool CPlatformNetworkManagerPC::RemoveLocalPlayerByUserIndex(int userIndex)
{
    EnterCriticalSection(&m_lock);
    NetworkPlayerPC* player = FindLocalPlayer(userIndex);
    Socket* socket = player != 0 ? player->GetSocket() : 0;
    LeaveCriticalSection(&m_lock);
    if (player == 0)
        return true;
    if (socket == 0)
        return RemoveRoomPlayer(userIndex);
    PendingRemoval* removal = new PendingRemoval;
    removal->manager = this;
    removal->player = player;
    removal->socket = socket;
    C4JThreadImpl* thread = new C4JThreadImpl(RemovePlayerOnSocketClosedThreadProc, removal,
                                              "RemovePlayerOnSocketClosed", 0);
    thread->SetProcessor(2);
    thread->SetDeleteOnExit(true);
    thread->Run();
    return true;
}

bool CPlatformNetworkManagerPC::AddLocalPlayerByPlayerUID(PlayerUID)
{
    return false;
}

INetworkPlayer* CPlatformNetworkManagerPC::GetLocalPlayerByUserIndex(int userIndex)
{
    EnterCriticalSection(&m_lock);
    INetworkPlayer* player = FindLocalPlayer(userIndex);
    LeaveCriticalSection(&m_lock);
    return player;
}

INetworkPlayer* CPlatformNetworkManagerPC::GetPlayerByIndex(int index)
{
    EnterCriticalSection(&m_lock);
    INetworkPlayer* player = index >= 0 && index < (int)m_roomPlayers.size() ? m_roomPlayers[index] : 0;
    LeaveCriticalSection(&m_lock);
    return player;
}

INetworkPlayer* CPlatformNetworkManagerPC::GetPlayerByXuid(PlayerUID uid)
{
    EnterCriticalSection(&m_lock);
    INetworkPlayer* player = 0;
    for (unsigned int index = 0; index < m_roomPlayers.size(); ++index)
    {
        if (m_roomPlayers[index]->GetUID() == uid)
        {
            player = m_roomPlayers[index];
            break;
        }
    }
    LeaveCriticalSection(&m_lock);
    return player;
}

INetworkPlayer* CPlatformNetworkManagerPC::GetPlayerBySmallId(unsigned char smallId)
{
    EnterCriticalSection(&m_lock);
    INetworkPlayer* player = 0;
    for (unsigned int index = 0; index < m_roomPlayers.size(); ++index)
    {
        if (m_roomPlayers[index]->GetSmallId() == smallId)
        {
            player = m_roomPlayers[index];
            break;
        }
    }
    LeaveCriticalSection(&m_lock);
    return player;
}

bool CPlatformNetworkManagerPC::ShouldMessageForFullSession()
{
    return false;
}

INetworkPlayer* CPlatformNetworkManagerPC::GetHostPlayer()
{
    EnterCriticalSection(&m_lock);
    INetworkPlayer* player = m_roomPlayers.empty() ? 0 : m_roomPlayers[0];
    LeaveCriticalSection(&m_lock);
    return player;
}

bool CPlatformNetworkManagerPC::IsHost()
{
    return m_host;
}

bool CPlatformNetworkManagerPC::JoinGameFromInviteInfo(int, int, const nn::fp::FriendPresence*)
{
    return false;
}

bool CPlatformNetworkManagerPC::LeaveGame(bool error)
{
    if (m_leavingGame)
        return true;
    m_leavingGame = true;
    CGameNetworkManager& manager = CGameNetworkManager::sInstance;
    INetworkPlayer* player = GetLocalPlayerByUserIndex(manager.GetPrimaryPad());
    if (player != 0)
    {
        Socket* socket = player->GetSocket();
        if (socket != 0)
        {
            socket->mCloseEvent->WaitForSignal(-1);
            if (IsInSession())
            {
                player = GetLocalPlayerByUserIndex(manager.GetPrimaryPad());
                if (player != 0)
                    player->SetSocket(0);
            }
            delete socket;
        }
    }
    if (m_host && manager.ServerStoppedValid())
    {
        manager.ServerStoppedWait();
        manager.ServerStoppedDestroy();
    }
    return _LeaveGame(error, true);
}

bool CPlatformNetworkManagerPC::IsInSession()
{
    return m_inSession;
}

bool CPlatformNetworkManagerPC::IsInGameplay()
{
    return m_state == eState_Playing;
}

bool CPlatformNetworkManagerPC::IsReadyToPlayOrIdle()
{
    EnterCriticalSection(&m_lock);
    bool ready = m_stateChanges.empty()
                 && (m_internalState == eState_Hosting || m_internalState == eState_Playing
                     || m_internalState == eState_Idle);
    LeaveCriticalSection(&m_lock);
    return ready;
}

bool CPlatformNetworkManagerPC::IsInStatsEnabledSession()
{
    return true;
}

bool CPlatformNetworkManagerPC::IsMatchmakingEnabled()
{
    return m_matchmaking;
}

bool CPlatformNetworkManagerPC::IsHandlingBootInvite()
{
    return false;
}

bool CPlatformNetworkManagerPC::SessionHasSpace(unsigned int extraPlayers)
{
    EnterCriticalSection(&m_lock);
    unsigned int count = m_roomPlayers.empty() ? 1 : (unsigned int)m_roomPlayers.size();
    LeaveCriticalSection(&m_lock);
    return count + extraPlayers <= CGameNetworkManager::sInstance.GetMaxPlayers();
}

bool CPlatformNetworkManagerPC::SessionHasMii(int userIndex)
{
    PlayerUID uid(0);
    gProfile->GetXUID(userIndex, &uid, true);
    EnterCriticalSection(&m_lock);
    bool found = false;
    for (unsigned int index = 0; index < m_roomPlayers.size(); ++index)
    {
        if (m_roomPlayers[index]->GetUID() == uid)
        {
            found = true;
            break;
        }
    }
    LeaveCriticalSection(&m_lock);
    return found;
}

void CPlatformNetworkManagerPC::SendInviteGUI(int)
{
}

bool CPlatformNetworkManagerPC::IsAddingPlayer()
{
    return false;
}

void CPlatformNetworkManagerPC::HostGame(int userIndexMask, bool, bool privateGame, unsigned char gameMode,
                                         EMiniGameId miniGameId, unsigned int worldId, unsigned int* attributes)
{
    SetLocalGame(true);
    SetPrivateGame(privateGame);
    SystemFlagReset();
    m_matchmakingSession = worldId != 0;
    userIndexMask |= GetLocalPlayerMask(CGameNetworkManager::sInstance.GetPrimaryPad());
    _HostGame(userIndexMask, gameMode, miniGameId, worldId, attributes);
}

int CPlatformNetworkManagerPC::JoinGame(FriendSessionInfo*, int, int, bool)
{
    return eJoinFailedReason_SessionUnavailable;
}

int CPlatformNetworkManagerPC::FindAndJoinGame(int, int, EMiniGameId, unsigned int, unsigned int*, unsigned int*)
{
    return 1;
}

bool CPlatformNetworkManagerPC::SetLocalGame(bool localGame)
{
    if (m_state == eState_Idle)
        m_localGame = localGame;
    return true;
}

bool CPlatformNetworkManagerPC::IsLocalGame()
{
    return m_localGame;
}

void CPlatformNetworkManagerPC::SetPrivateGame(bool privateGame)
{
    m_privateGame = privateGame;
}

bool CPlatformNetworkManagerPC::IsPrivateGame()
{
    return m_privateGame;
}

bool CPlatformNetworkManagerPC::IsLeavingGame()
{
    return m_leavingGame;
}

void CPlatformNetworkManagerPC::ResetLeavingGame()
{
    m_leavingGame = false;
}

void CPlatformNetworkManagerPC::RegisterPlayerChangedCallback(int userIndex,
                                                              void (*callback)(void*, INetworkPlayer*, bool),
                                                              void* callbackData)
{
    if (userIndex < 0 || userIndex >= cMaxLocalPlayers)
        return;
    EnterCriticalSection(&m_lock);
    m_playerChangedCallbacks[userIndex] = callback;
    m_playerChangedCallbackData[userIndex] = callbackData;
    LeaveCriticalSection(&m_lock);
}

void CPlatformNetworkManagerPC::UnRegisterPlayerChangedCallback(int userIndex, void (*)(void*, INetworkPlayer*, bool),
                                                                void* callbackData)
{
    if (userIndex < 0 || userIndex >= cMaxLocalPlayers)
        return;
    EnterCriticalSection(&m_lock);
    if (m_playerChangedCallbackData[userIndex] == callbackData)
    {
        m_playerChangedCallbacks[userIndex] = 0;
        m_playerChangedCallbackData[userIndex] = 0;
    }
    LeaveCriticalSection(&m_lock);
}

void CPlatformNetworkManagerPC::HandleSignInChange()
{
}

bool CPlatformNetworkManagerPC::_RunNetworkGame()
{
    if (IsHost())
    {
        StartGame();
        EnterCriticalSection(&m_lock);
        m_sessionData.m_bGameStarted = true;
        LeaveCriticalSection(&m_lock);
    }
    return true;
}

bool CPlatformNetworkManagerPC::_LeaveGame(bool, bool)
{
    EnterCriticalSection(&m_lock);
    m_leavePending = true;
    m_leavingGame = true;
    LeaveCriticalSection(&m_lock);
    return true;
}

void CPlatformNetworkManagerPC::_HostGame(int userIndexMask, unsigned char gameMode, EMiniGameId miniGameId,
                                          unsigned int, unsigned int*)
{
    EnterCriticalSection(&m_lock);
    memset(&m_sessionData, 0, sizeof(m_sessionData));
    m_sessionData.m_version = cSessionDataVersion;
    m_sessionData.m_bPublicGame = !IsPrivateGame();
    m_sessionData.m_bGameStarted = false;
    m_sessionData.m_numPlayers = 0;
    m_sessionData.m_gameHostOptions = gApp.GetGameHostOption(eGameHostOption_All);
    m_sessionData.m_customGameModeHostOptions = Minecraft::GetInstance()->GetMiniGame()->GetHostOption(
        eCustomGameModeHostOption_All);
    m_sessionData.m_gameModeId = gameMode;
    m_sessionData.m_miniGameId = miniGameId;
    for (int index = 0; index < cMaxLocalPlayers; ++index)
        if ((userIndexMask & (1 << index)) != 0)
            ++m_sessionData.m_numPlayers;
    CreateRoom(CGameNetworkManager::sInstance.GetPrimaryPad(), userIndexMask);
    LeaveCriticalSection(&m_lock);
}

bool CPlatformNetworkManagerPC::_StartGame()
{
    StartGame();
    return true;
}

void CPlatformNetworkManagerPC::UpdateAndSetGameSessionData(INetworkPlayer*)
{
    if (m_leavingGame)
        return;
    INetworkPlayer* host = GetHostPlayer();
    EnterCriticalSection(&m_lock);
    if (host != 0)
        CopyGamertag(m_sessionData.m_hostGamertag, host->GetOnlineName());
    m_sessionData.m_gameHostOptions = gApp.GetGameHostOption(eGameHostOption_All);
    LeaveCriticalSection(&m_lock);
}

bool CPlatformNetworkManagerPC::RemoveLocalPlayer(INetworkPlayer* player)
{
    if (player->IsLocal())
        return RemoveRoomPlayer(player->GetUserIndex());
    return true;
}

void CPlatformNetworkManagerPC::SystemFlagAddPlayer(INetworkPlayer* player)
{
    EnterCriticalSection(&m_lock);
    PlayerFlags entry;
    entry.player = player;
    entry.flags.assign((m_maxSystemFlags + 7) / 8, 0);
    for (unsigned int index = 0; index < m_playerFlags.size(); ++index)
    {
        if (player->IsSameSystem(m_playerFlags[index].player))
        {
            entry.flags = m_playerFlags[index].flags;
            break;
        }
    }
    m_playerFlags.push_back(entry);
    LeaveCriticalSection(&m_lock);
}

void CPlatformNetworkManagerPC::SystemFlagSet(INetworkPlayer* player, int flag)
{
    if (flag < 0 || flag >= (int)m_maxSystemFlags || player == 0)
        return;
    EnterCriticalSection(&m_lock);
    for (unsigned int index = 0; index < m_playerFlags.size(); ++index)
        if (player->IsSameSystem(m_playerFlags[index].player))
            m_playerFlags[index].flags[flag / 8] |= (unsigned char)(0x80 >> (flag % 8));
    LeaveCriticalSection(&m_lock);
}

bool CPlatformNetworkManagerPC::SystemFlagGet(INetworkPlayer* player, int flag)
{
    if (flag < 0 || flag >= (int)m_maxSystemFlags || player == 0)
        return false;
    EnterCriticalSection(&m_lock);
    bool set = false;
    for (unsigned int index = 0; index < m_playerFlags.size(); ++index)
    {
        if (m_playerFlags[index].player == player)
        {
            set = (m_playerFlags[index].flags[flag / 8] & (0x80 >> (flag % 8))) != 0;
            break;
        }
    }
    LeaveCriticalSection(&m_lock);
    return set;
}

void CPlatformNetworkManagerPC::SystemFlagClearAll(INetworkPlayer* player)
{
    if (player == 0)
        return;
    EnterCriticalSection(&m_lock);
    for (unsigned int index = 0; index < m_playerFlags.size(); ++index)
        if (m_playerFlags[index].player == player)
            m_playerFlags[index].flags.assign(m_playerFlags[index].flags.size(), 0);
    LeaveCriticalSection(&m_lock);
}

std::wstring CPlatformNetworkManagerPC::GatherStats()
{
    return std::wstring(L"");
}

std::wstring CPlatformNetworkManagerPC::GatherRTTStats()
{
    return std::wstring(L"");
}

void CPlatformNetworkManagerPC::SetSessionTexturePackParentId(int id)
{
    EnterCriticalSection(&m_lock);
    m_sessionData.m_texturePackParentId = id;
    LeaveCriticalSection(&m_lock);
}

void CPlatformNetworkManagerPC::SetSessionSubTexturePackId(int id)
{
    EnterCriticalSection(&m_lock);
    m_sessionData.m_subTexturePackId = (unsigned char)id;
    LeaveCriticalSection(&m_lock);
}

void CPlatformNetworkManagerPC::SetSessionGameModeId(int id)
{
    EnterCriticalSection(&m_lock);
    m_sessionData.m_gameModeId = id;
    LeaveCriticalSection(&m_lock);
}

void CPlatformNetworkManagerPC::Notify(int, unsigned long)
{
}

std::vector<FriendSessionInfo*>* CPlatformNetworkManagerPC::GetSessionList(int, int, bool)
{
    return new std::vector<FriendSessionInfo*>();
}

bool CPlatformNetworkManagerPC::GetGameSessionInfo(int, unsigned int, FriendSessionInfo*)
{
    return false;
}

void CPlatformNetworkManagerPC::SetSessionsUpdatedCallback(void (*callback)(void*), void* callbackData)
{
    EnterCriticalSection(&m_lock);
    m_sessionsUpdatedCallback = callback;
    m_sessionsUpdatedCallbackData = callbackData;
    LeaveCriticalSection(&m_lock);
}

void CPlatformNetworkManagerPC::GetFullFriendSessionInfo(FriendSessionInfo*, void (*callback)(bool, void*),
                                                         void* callbackData)
{
    EnterCriticalSection(&m_lock);
    m_sessionInfoPending = true;
    m_sessionInfoCallback = callback;
    m_sessionInfoCallbackData = callbackData;
    LeaveCriticalSection(&m_lock);
}

void CPlatformNetworkManagerPC::ForceFriendsSessionRefresh()
{
}

void CPlatformNetworkManagerPC::ClearFriendsSessions()
{
}

bool CPlatformNetworkManagerPC::AlreadyInSession(const nn::fp::FriendPresence*)
{
    return false;
}

bool CPlatformNetworkManagerPC::GetAgeRestricted()
{
    return false;
}

bool CPlatformNetworkManagerPC::usingAdhocMode()
{
    return false;
}

bool CPlatformNetworkManagerPC::setAdhocMode(bool)
{
    return false;
}

void CPlatformNetworkManagerPC::startAdhocMatching()
{
}

bool CPlatformNetworkManagerPC::isAdhocConnected()
{
    return false;
}

bool CPlatformNetworkManagerPC::connectToAdhoc()
{
    return false;
}

bool CPlatformNetworkManagerPC::isAdhocModeChangeInProgress()
{
    return false;
}

void CPlatformNetworkManagerPC::AttemptAdhocSignIn(int (*)(void*, bool, int), void*, bool)
{
}

bool CPlatformNetworkManagerPC::setLanMode(bool mode)
{
    return !mode;
}

bool CPlatformNetworkManagerPC::usingLanMode()
{
    return false;
}

bool CPlatformNetworkManagerPC::isLanConnected()
{
    return false;
}

bool CPlatformNetworkManagerPC::isLanModeChangeInProgress()
{
    return false;
}

bool CPlatformNetworkManagerPC::checkValidInviteData(const nn::fp::FriendPresence*)
{
    return true;
}

void CPlatformNetworkManagerPC::SetExitAndJoinFromInviteInProgress(bool)
{
}

void CPlatformNetworkManagerPC::CancelDisconnectFromExitAndJoin()
{
}

std::wstring CPlatformNetworkManagerPC::GetDisplayNameByGamertag(std::wstring gamertag)
{
    EnterCriticalSection(&m_lock);
    std::map<std::wstring, std::wstring>::const_iterator found = m_displayNames.find(gamertag);
    std::wstring name = found != m_displayNames.end() ? found->second : gamertag;
    LeaveCriticalSection(&m_lock);
    return name;
}

int CPlatformNetworkManagerPC::GetOutstandingAckCount(INetworkPlayer*)
{
    return 0;
}

void CPlatformNetworkManagerPC::TryToConnectOnline(int (*callback)(void*, bool, int), void* callbackData,
                                                   bool showProgress)
{
    CGameNetworkManager::sInstance.ResetLeavingGame();
    EnterCriticalSection(&m_lock);
    m_connectPending = true;
    m_connectShowProgress = showProgress;
    m_connectCallback = callback;
    m_connectCallbackData = callbackData;
    LeaveCriticalSection(&m_lock);
}

void CPlatformNetworkManagerPC::AttemptPSNSignIn(int (*)(void*, bool, int), void*, bool, int)
{
}

void CPlatformNetworkManagerPC::RecvInviteGUI()
{
}

bool CPlatformNetworkManagerPC::UpdateInviteData(void*)
{
    return false;
}

void CPlatformNetworkManagerPC::CheckFriendsCanPlay(int userIndex, int& friendCount,
                                                    boost::shared_ptr<ClientboundPreLoginPacket> packet,
                                                    int titleVersion, int& canPlayCount, int& cannotPlayCount, int&,
                                                    int&)
{
    CGameNetworkManager& manager = CGameNetworkManager::sInstance;
    bool isPrimaryPad = userIndex == 0;
    if (!manager.IsLocalGame())
        friendCount = !manager.IsUGCRestrictedMode(isPrimaryPad ? 0xff : userIndex, packet->mMiniGameId);

    if (friendCount != 0 && titleVersion == 0 && !manager.IsHost()
        && gApp.GetGameHostOption(eGameHostOption_FriendsOfFriends) == 0 && isPrimaryPad)
    {
        canPlayCount = 0;
        cannotPlayCount = 0;
    }
}

void CPlatformNetworkManagerPC::LocalDataSend(NetworkPlayerPC* from, NetworkPlayerPC* to, const void* data,
                                              unsigned int size)
{
    if (m_state == eState_Ending)
        return;
    const unsigned char* bytes = static_cast<const unsigned char*>(data);
    if (to->IsHost())
    {
        Socket* socket = from->GetSocket();
        if (socket != 0)
            socket->pushDataToQueue(bytes, size, false);
    }
    else
    {
        Socket* socket = to->GetSocket();
        if (socket != 0)
            socket->pushDataToQueue(bytes, size, true);
    }
}

int CPlatformNetworkManagerPC::GetSessionIndex(NetworkPlayerPC* player)
{
    EnterCriticalSection(&m_lock);
    int sessionIndex = 0;
    for (unsigned int index = 0; index < m_roomPlayers.size(); ++index)
    {
        if (m_roomPlayers[index] == player)
        {
            sessionIndex = (int)index;
            break;
        }
    }
    LeaveCriticalSection(&m_lock);
    return sessionIndex;
}

void CPlatformNetworkManagerPC::SetState(EState state)
{
    EnterCriticalSection(&m_lock);
    if (state == eState_Idle)
        m_matchmaking = false;
    if (state != m_internalState)
    {
        StateChange change;
        change.oldState = m_internalState;
        change.newState = state;
        m_stateChanges.push_back(change);
        m_internalState = state;
    }
    LeaveCriticalSection(&m_lock);
}

void CPlatformNetworkManagerPC::ProcessStateChanges()
{
    while (true)
    {
        EnterCriticalSection(&m_lock);
        if (m_stateChanges.empty())
        {
            LeaveCriticalSection(&m_lock);
            return;
        }
        StateChange change = m_stateChanges.front();
        LeaveCriticalSection(&m_lock);

        HandleStateChange(change.oldState, change.newState);

        EnterCriticalSection(&m_lock);
        if (change.newState == eState_Idle)
            m_inSession = false;
        m_state = change.newState;
        m_stateChanges.pop_front();
        LeaveCriticalSection(&m_lock);
    }
}

void CPlatformNetworkManagerPC::HandleStateChange(EState oldState, EState newState)
{
    CGameNetworkManager& manager = CGameNetworkManager::sInstance;
    if (newState == eState_Hosting)
    {
        m_leavingGame = false;
        m_leavePending = false;
        manager.StateChange_AnyToHosting();
    }
    else if (newState == eState_Idle && oldState == eState_Hosting)
    {
        m_leavingGame = true;
    }
    else if (newState == eState_Starting)
    {
        manager.StateChange_AnyToStarting();
    }
    else if (newState == eState_Ending)
    {
        manager.StateChange_AnyToEnding(oldState == eState_Playing, false);
        if (m_host)
            m_leavingGame = true;
    }
}

void CPlatformNetworkManagerPC::CreateRoom(int primaryUserIndex, int userIndexMask)
{
    EnterCriticalSection(&m_lock);
    if (!m_roomPlayers.empty())
        DestroyRoomPlayers();
    m_inSession = true;
    m_host = true;
    m_matchmaking = m_matchmakingSession;
    SetState(eState_Hosting);
    HandlePlayerJoined(AddRoomPlayer(primaryUserIndex, true));
    for (int index = 0; index < cMaxLocalPlayers; ++index)
    {
        if (index == primaryUserIndex || (userIndexMask & (1 << index)) == 0)
            continue;
        HandlePlayerJoined(AddRoomPlayer(index, false));
    }
    ResyncSessionPlayers();
    LeaveCriticalSection(&m_lock);
}

NetworkPlayerPC* CPlatformNetworkManagerPC::AddRoomPlayer(int userIndex, bool host)
{
    PlayerUID uid = GetPlayerUID(userIndex);
    std::wstring onlineName = gProfile->GetDisplayName(userIndex);
    std::wstring displayName(gProfile->GetMiiName(userIndex));
    NetworkPlayerPC* player = new NetworkPlayerPC(this, host ? NetworkPlayerPC::eType_Host : NetworkPlayerPC::eType_Local,
                                                  cLocalSystemId, userIndex, m_nextSmallId++, uid, onlineName,
                                                  displayName);
    m_roomPlayers.push_back(player);
    m_displayNames[onlineName] = displayName;
    return player;
}

bool CPlatformNetworkManagerPC::RemoveRoomPlayer(int userIndex)
{
    EnterCriticalSection(&m_lock);
    NetworkPlayerPC* player = 0;
    for (std::vector<NetworkPlayerPC*>::iterator it = m_roomPlayers.begin(); it != m_roomPlayers.end(); ++it)
    {
        if ((*it)->IsLocal() && (*it)->GetUserIndex() == userIndex)
        {
            player = *it;
            m_roomPlayers.erase(it);
            break;
        }
    }
    if (player == 0)
    {
        LeaveCriticalSection(&m_lock);
        UpdateLocalMultiplayer();
        return false;
    }
    HandlePlayerLeaving(player);
    delete player;
    ResyncSessionPlayers();
    UpdateLocalMultiplayer();
    LeaveCriticalSection(&m_lock);
    return true;
}

void CPlatformNetworkManagerPC::DestroyRoomPlayers()
{
    EnterCriticalSection(&m_lock);
    for (unsigned int index = 0; index < m_roomPlayers.size(); ++index)
    {
        SystemFlagRemovePlayer(m_roomPlayers[index]);
        delete m_roomPlayers[index];
    }
    m_roomPlayers.clear();
    LeaveCriticalSection(&m_lock);
}

void CPlatformNetworkManagerPC::StartGame()
{
    SetState(eState_Starting);
    SetState(eState_Playing);
}

void CPlatformNetworkManagerPC::LeaveRoom()
{
    EnterCriticalSection(&m_lock);
    if (m_internalState == eState_Hosting || m_internalState == eState_Starting || m_internalState == eState_Playing)
    {
        SetState(eState_Leaving);
        SetState(eState_Ending);
        ResetToIdle();
    }
    LeaveCriticalSection(&m_lock);
}

void CPlatformNetworkManagerPC::ResetToIdle()
{
    EnterCriticalSection(&m_lock);
    m_host = false;
    m_nextSmallId = 0;
    DestroyRoomPlayers();
    SetState(eState_Idle);
    LeaveCriticalSection(&m_lock);
}

void CPlatformNetworkManagerPC::HandlePlayerJoined(NetworkPlayerPC* player)
{
    CGameNetworkManager& manager = CGameNetworkManager::sInstance;
    bool localConnection = player->IsLocal();
    bool createSocket = localConnection && !player->IsHost();

    manager.PlayerJoining(player);
    if (createSocket)
        manager.CreateSocket(player, localConnection);

    if (m_host)
    {
        manager.UpdateAndSetGameSessionData(0);
        SystemFlagAddPlayer(player);
    }

    for (int index = 0; index < cMaxLocalPlayers; ++index)
        if (m_playerChangedCallbacks[index] != 0)
            m_playerChangedCallbacks[index](m_playerChangedCallbackData[index], player, false);
}

void CPlatformNetworkManagerPC::HandlePlayerLeaving(NetworkPlayerPC* player)
{
    CGameNetworkManager& manager = CGameNetworkManager::sInstance;
    Socket* socket = player->GetSocket();
    if (socket != 0)
    {
        if (m_host)
            manager.CloseConnection(player);
        player->SetSocket(0);
    }

    if (m_host)
    {
        manager.UpdateAndSetGameSessionData(player);
        SystemFlagRemovePlayer(player);
    }

    manager.PlayerLeaving(player);
    for (int index = 0; index < cMaxLocalPlayers; ++index)
        if (m_playerChangedCallbacks[index] != 0)
            m_playerChangedCallbacks[index](m_playerChangedCallbackData[index], player, true);
}

void CPlatformNetworkManagerPC::ResyncSessionPlayers()
{
    EnterCriticalSection(&m_lock);
    m_sessionData.m_numPlayers = 0;
    for (int index = 0; index < GameSessionData::MAX_SESSION_PLAYERS; ++index)
    {
        if (index < (int)m_roomPlayers.size())
        {
            CopyGamertag(m_sessionData.m_playerGamertags[index], m_roomPlayers[index]->GetOnlineName());
            ++m_sessionData.m_numPlayers;
        }
        else
        {
            memset(m_sessionData.m_playerGamertags[index].m_name, 0, GameSessionData::MAX_GAMERTAG_LENGTH);
        }
    }
    LeaveCriticalSection(&m_lock);
}

void CPlatformNetworkManagerPC::UpdateLocalMultiplayer()
{
    int localPlayerCount = 0;
    for (int index = 0; index < cMaxLocalPlayers; ++index)
        if (GetLocalPlayerByUserIndex(index) != 0)
            ++localPlayerCount;
    g_pInput->SetLocalMultiplayer(localPlayerCount > 1);
}

void CPlatformNetworkManagerPC::SystemFlagRemovePlayer(INetworkPlayer* player)
{
    EnterCriticalSection(&m_lock);
    for (unsigned int index = 0; index < m_playerFlags.size(); ++index)
    {
        if (m_playerFlags[index].player == player)
        {
            m_playerFlags[index] = m_playerFlags.back();
            m_playerFlags.pop_back();
            break;
        }
    }
    LeaveCriticalSection(&m_lock);
}

void CPlatformNetworkManagerPC::SystemFlagReset()
{
    EnterCriticalSection(&m_lock);
    m_playerFlags.clear();
    LeaveCriticalSection(&m_lock);
}

PlayerUID CPlatformNetworkManagerPC::GetPlayerUID(int userIndex)
{
    PlayerUID uid(0);
    gProfile->GetXUID(userIndex, &uid, true);
    if (uid == PlayerUID(0))
        gProfile->GetXUID(userIndex, &uid, false);
    for (unsigned int index = 0; index < m_roomPlayers.size(); ++index)
    {
        if (m_roomPlayers[index]->GetUID() == uid)
        {
            uid.flags |= PlayerUID::FlagMiiPrincipal;
            break;
        }
    }
    return uid;
}

bool CPlatformNetworkManagerPC::ContainsPlayer(NetworkPlayerPC* player)
{
    for (unsigned int index = 0; index < m_roomPlayers.size(); ++index)
        if (m_roomPlayers[index] == player)
            return true;
    return false;
}

NetworkPlayerPC* CPlatformNetworkManagerPC::FindLocalPlayer(int userIndex)
{
    for (unsigned int index = 0; index < m_roomPlayers.size(); ++index)
        if (m_roomPlayers[index]->IsLocal() && m_roomPlayers[index]->GetUserIndex() == userIndex)
            return m_roomPlayers[index];
    return 0;
}
