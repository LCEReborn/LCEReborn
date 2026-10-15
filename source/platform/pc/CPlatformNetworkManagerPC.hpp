#ifndef PLATFORM_PC_CPLATFORMNETWORKMANAGERPC_HPP
#define PLATFORM_PC_CPLATFORMNETWORKMANAGERPC_HPP

#include <deque>
#include <map>
#include <string>
#include <vector>

#include "game/core/CriticalSection.hpp"
#include "game/core/GameSessionData.hpp"
#include "game/network/CPlatformNetworkManager.hpp"
#include "game/world/generation/PlayerUID.hpp"

class CGameNetworkManager;
class NetworkPlayerPC;
class Socket;

class CPlatformNetworkManagerPC : public CPlatformNetworkManager
{
public:
    static const unsigned int cLocalSystemId = 0;

    enum EState
    {
        eState_Uninitialised,
        eState_Idle,
        eState_Hosting,
        eState_Starting,
        eState_Playing,
        eState_Leaving,
        eState_Ending
    };

    CPlatformNetworkManagerPC();

    virtual void Initialise(CGameNetworkManager* manager, int maxSystemFlags);
    virtual void Terminate();
    virtual int GetJoiningReadyPercentage();
    virtual int CorrectErrorIDS(int errorIds);
    virtual void DoWork();
    virtual int GetPlayerCount();
    virtual int GetOnlinePlayerCount();
    virtual int GetLocalPlayerMask(int userIndex);
    virtual bool AddLocalPlayerByUserIndex(int userIndex);
    virtual bool RemoveLocalPlayerByUserIndex(int userIndex);
    virtual bool AddLocalPlayerByPlayerUID(PlayerUID uid);
    virtual INetworkPlayer* GetLocalPlayerByUserIndex(int userIndex);
    virtual INetworkPlayer* GetPlayerByIndex(int index);
    virtual INetworkPlayer* GetPlayerByXuid(PlayerUID uid);
    virtual INetworkPlayer* GetPlayerBySmallId(unsigned char smallId);
    virtual bool ShouldMessageForFullSession();
    virtual INetworkPlayer* GetHostPlayer();
    virtual bool IsHost();
    virtual bool JoinGameFromInviteInfo(int userIndex, int userIndexMask, const nn::fp::FriendPresence* presence);
    virtual bool LeaveGame(bool error);
    virtual bool IsInSession();
    virtual bool IsInGameplay();
    virtual bool IsReadyToPlayOrIdle();
    virtual bool IsInStatsEnabledSession();
    virtual bool IsMatchmakingEnabled();
    virtual bool IsHandlingBootInvite();
    virtual bool SessionHasSpace(unsigned int extraPlayers);
    virtual bool SessionHasMii(int userIndex);
    virtual void SendInviteGUI(int userIndex);
    virtual bool IsAddingPlayer();
    virtual void HostGame(int userIndexMask, bool onlineGame, bool privateGame, unsigned char gameMode,
                          EMiniGameId miniGameId, unsigned int worldId, unsigned int* attributes);
    virtual int JoinGame(FriendSessionInfo* sessionInfo, int userIndex, int userIndexMask, bool matchmaking);
    virtual int FindAndJoinGame(int userIndexMask, int searchFlags, EMiniGameId miniGameId, unsigned int attributeCount,
                                unsigned int* attributeValues, unsigned int* attributeIds);
    virtual bool SetLocalGame(bool localGame);
    virtual bool IsLocalGame();
    virtual void SetPrivateGame(bool privateGame);
    virtual bool IsPrivateGame();
    virtual bool IsLeavingGame();
    virtual void ResetLeavingGame();
    virtual void RegisterPlayerChangedCallback(int userIndex, void (*callback)(void*, INetworkPlayer*, bool),
                                               void* callbackData);
    virtual void UnRegisterPlayerChangedCallback(int userIndex, void (*callback)(void*, INetworkPlayer*, bool),
                                                 void* callbackData);
    virtual void HandleSignInChange();
    virtual bool _RunNetworkGame();
    virtual bool _LeaveGame(bool error, bool endSession);
    virtual void _HostGame(int userIndexMask, unsigned char gameMode, EMiniGameId miniGameId, unsigned int worldId,
                           unsigned int* attributes);
    virtual bool _StartGame();
    virtual void UpdateAndSetGameSessionData(INetworkPlayer* player);
    virtual bool RemoveLocalPlayer(INetworkPlayer* player);
    virtual void SystemFlagAddPlayer(INetworkPlayer* player);
    virtual void SystemFlagSet(INetworkPlayer* player, int flag);
    virtual bool SystemFlagGet(INetworkPlayer* player, int flag);
    virtual void SystemFlagClearAll(INetworkPlayer* player);
    virtual std::wstring GatherStats();
    virtual std::wstring GatherRTTStats();
    virtual void SetSessionTexturePackParentId(int id);
    virtual void SetSessionSubTexturePackId(int id);
    virtual void SetSessionGameModeId(int id);
    virtual void Notify(int notification, unsigned long param);
    virtual std::vector<FriendSessionInfo*>* GetSessionList(int userIndex, int maxSessions, bool friendsOnly);
    virtual bool GetGameSessionInfo(int userIndex, unsigned int sessionId, FriendSessionInfo* info);
    virtual void SetSessionsUpdatedCallback(void (*callback)(void*), void* callbackData);
    virtual void GetFullFriendSessionInfo(FriendSessionInfo* info, void (*callback)(bool, void*), void* callbackData);
    virtual void ForceFriendsSessionRefresh();
    virtual void ClearFriendsSessions();
    virtual bool AlreadyInSession(const nn::fp::FriendPresence* presence);
    virtual bool GetAgeRestricted();
    virtual bool usingAdhocMode();
    virtual bool setAdhocMode(bool mode);
    virtual void startAdhocMatching();
    virtual bool isAdhocConnected();
    virtual bool connectToAdhoc();
    virtual bool isAdhocModeChangeInProgress();
    virtual void AttemptAdhocSignIn(int (*callback)(void*, bool, int), void* callbackData, bool showProgress);
    virtual bool setLanMode(bool mode);
    virtual bool usingLanMode();
    virtual bool isLanConnected();
    virtual bool isLanModeChangeInProgress();
    virtual bool checkValidInviteData(const nn::fp::FriendPresence* presence);
    virtual void SetExitAndJoinFromInviteInProgress(bool inProgress);
    virtual void CancelDisconnectFromExitAndJoin();
    virtual std::wstring GetDisplayNameByGamertag(std::wstring gamertag);
    virtual int GetOutstandingAckCount(INetworkPlayer* player);
    virtual void TryToConnectOnline(int (*callback)(void*, bool, int), void* callbackData, bool showProgress);
    virtual void AttemptPSNSignIn(int (*callback)(void*, bool, int), void* callbackData, bool showProgress,
                                  int userIndex);
    virtual void RecvInviteGUI();
    virtual bool UpdateInviteData(void* inviteData);
    virtual void CheckFriendsCanPlay(int userIndex, int& friendCount,
                                     boost::shared_ptr<ClientboundPreLoginPacket> packet, int titleVersion,
                                     int& canPlayCount, int& cannotPlayCount, int& blockedCount, int& unknownCount);

    void LocalDataSend(NetworkPlayerPC* from, NetworkPlayerPC* to, const void* data, unsigned int size);
    int GetSessionIndex(NetworkPlayerPC* player);

private:
    struct StateChange
    {
        EState oldState;
        EState newState;
    };

    struct PlayerFlags
    {
        INetworkPlayer* player;
        std::vector<unsigned char> flags;
    };

    struct PendingRemoval
    {
        CPlatformNetworkManagerPC* manager;
        NetworkPlayerPC* player;
        Socket* socket;
    };

    static int RemovePlayerOnSocketClosedThreadProc(void* data);

    void SetState(EState state);
    void ProcessStateChanges();
    void HandleStateChange(EState oldState, EState newState);
    void CreateRoom(int primaryUserIndex, int userIndexMask);
    NetworkPlayerPC* AddRoomPlayer(int userIndex, bool host);
    bool RemoveRoomPlayer(int userIndex);
    void DestroyRoomPlayers();
    void StartGame();
    void LeaveRoom();
    void ResetToIdle();
    void HandlePlayerJoined(NetworkPlayerPC* player);
    void HandlePlayerLeaving(NetworkPlayerPC* player);
    void ResyncSessionPlayers();
    void UpdateLocalMultiplayer();
    void SystemFlagRemovePlayer(INetworkPlayer* player);
    void SystemFlagReset();
    PlayerUID GetPlayerUID(int userIndex);
    bool ContainsPlayer(NetworkPlayerPC* player);
    NetworkPlayerPC* FindLocalPlayer(int userIndex);

    C4JCriticalSection m_lock;
    CGameNetworkManager* m_gameNetworkManager;
    EState m_internalState;
    EState m_state;
    std::deque<StateChange> m_stateChanges;
    std::vector<NetworkPlayerPC*> m_roomPlayers;
    std::map<std::wstring, std::wstring> m_displayNames;
    std::vector<PlayerFlags> m_playerFlags;
    unsigned int m_maxSystemFlags;
    unsigned char m_nextSmallId;
    bool m_host;
    bool m_inSession;
    bool m_localGame;
    bool m_privateGame;
    bool m_matchmakingSession;
    bool m_matchmaking;
    bool m_leavingGame;
    bool m_leavePending;
    GameSessionData m_sessionData;
    void (*m_playerChangedCallbacks[4])(void*, INetworkPlayer*, bool);
    void* m_playerChangedCallbackData[4];
    void (*m_sessionsUpdatedCallback)(void*);
    void* m_sessionsUpdatedCallbackData;
    bool m_connectPending;
    bool m_connectShowProgress;
    int (*m_connectCallback)(void*, bool, int);
    void* m_connectCallbackData;
    bool m_sessionInfoPending;
    void (*m_sessionInfoCallback)(bool, void*);
    void* m_sessionInfoCallbackData;
};

#endif
