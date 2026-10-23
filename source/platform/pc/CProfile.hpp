#ifndef PLATFORM_PC_CPROFILE_HPP
#define PLATFORM_PC_CPROFILE_HPP

#include <deque>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "game/core/CriticalSection.hpp"
#include "game/util/ACTUuid.hpp"
#include "game/world/generation/PlayerUID.hpp"

#include "platform/C4JProfile.hpp"

class C4JThread;
struct PcPlayerProfile;

class CProfile : public C_4JProfile
{
public:
    enum EProfileProcessState
    {
        eProfileProcessState_Idle = 0,
        eProfileProcessState_ShowSignIn = 1,
        eProfileProcessState_ProcessSignIn = 2,
        eProfileProcessState_QuerySignIn = 3,
        eProfileProcessState_SignOut = 4,
        eProfileProcessState_Unknown7 = 7,
        eProfileProcessState_OfflineProfile = 8,
        eProfileProcessState_Unknown11 = 11,
        eProfileProcessState_Unknown12 = 12,
        eProfileProcessState_Upsell = 13
    };

    enum
    {
        kMaxPlayerNameLength = 16,
        kAwardCount = 128,
        kAwardFileSize = 16,
        kAwardThreadInterval = 70,
        kAvatarSize = 64
    };

    struct AvatarRequest
    {
        int quadrant;
        void* context;
        AvatarHandler handler;
    };

    struct PROFILE_DETAILS
    {
        PROFILE_DETAILS();

        bool signedIn;
        bool signingIn;
        bool onlineAccount;
        unsigned char state;
        ACTUuid uuid;
        wchar_t name[32];
    };

    struct CONTEXTVAL : public std::unordered_map<int, int>
    {
        CONTEXTVAL();
    };

    struct AwardRegistration
    {
        int registrationId;
        int callbackValue;
        int callbackContext;
    };

    CProfile();
    ~CProfile();

    virtual void Initialise(const void*, const void*, unsigned short, unsigned int, unsigned int, unsigned int*, int,
                            unsigned int*);
    virtual int GetLockedProfile();
    virtual void SetLockedProfile(int);
    virtual bool IsSignedIn(int);
    virtual bool IsSignedInLive(int);
    virtual bool IsSigningIn(int);
    virtual bool IsGuest(int);
    virtual int RequestSignInUI(bool, bool, bool, bool, bool, SignInUIHandler, void*, int);
    int RequestSignInUI(bool, bool, bool, bool, bool, bool, SignInUIHandler, void*, int);
    int SignInUser(int);
    int SignInAs(int pad, const PcPlayerProfile* chosen);
    std::vector<PcPlayerProfile> UnusedLauncherProfiles();
    static ACTUuid UuidFromHex(const std::string& hex);
    virtual void SignOutUser(int);
    virtual void SetUserSigningIn(int, bool);
    virtual int DisplayOfflineProfile(OfflineProfileHandler, void*, int);
    virtual void RequestConvertOfflineToGuestUI(SignInUIHandler, void*, int);
    virtual void SetPrimaryPlayerChanged(bool);
    virtual int QuerySigninStatus();
    virtual void GetXUID(int, PlayerUID*, bool);
    virtual int AreXUIDSEqual(PlayerUID, PlayerUID);
    virtual bool AllowedToPlayMultiplayer(int);
    virtual void StartTrialGame();
    virtual void AllowedPlayerCreatedContent(int, bool, int*, int*);
    virtual bool CanViewPlayerCreatedContent(int, bool, PlayerUID*, unsigned int);
    virtual bool GetProfileAvatar(int, AvatarHandler, void*);
    virtual void CancelProfileAvatarRequest();
    virtual void SetGetStringFunc(GetStringHandler);
    virtual void SetPlayerListTitleID(int);
    virtual void SetFatalTrophyErrorID(int);
    virtual int GetPrimaryPad();
    virtual void SetPrimaryPad(int);
    virtual void ClearGameUsers();
    virtual wchar_t* GetMiiName(int);
    virtual wchar_t* GetGamertag(int);
    virtual std::wstring GetDisplayName(int);
    virtual char* GetNetworkAccountID();
    virtual bool IsFullVersion();
    virtual void SetFullVersion(bool);
    virtual void SetSignInChangeCallback(SignInChangeHandler, void*);
    virtual void SetSignInChoicesComplete(bool, int, int);
    virtual void SetNotificationsCallback(NotificationsHandler, void*);
    virtual bool RegionIsNorthAmerica();
    virtual bool LocaleIsUSorCanada();
    virtual int GetLiveConnectionStatus();
    virtual bool IsSystemUIDisplayed();
    virtual void SetProfileReadErrorCallback(ProfileReadErrorHandler, void*);
    virtual void ResetProfileProcessState();
    virtual void Tick();
    virtual void RegisterAward(int, int, eAwardType, int, int);
    virtual int GetAwardId(int);
    virtual eAwardType GetAwardType(int);
    virtual bool CanBeAwarded(int, int);
    virtual void Award(int, int, bool);
    virtual bool IsAwardsFlagSet(int, int);
    virtual void SetOnAwardHandler(AwardHandler, void*);
    virtual void Terminate();
    virtual void RichPresenceRegisterPresenceString(int, int);
    virtual void RichPresenceRegisterContext(int, int);
    virtual void RichPresenceRegisterContextString(int, int, int);
    virtual void RichPresenceInit(int, int);
    virtual void SetRichPresenceContextValue(int, int, int);
    virtual void SetCurrentGameActivity(int, int, bool);
    virtual void SetRichPresenceSettingFn(RichPresenceSettingHandler);
    virtual void SetFriendPresenceChangeCallback(FriendPresenceHandler, void*);
    virtual void SetRichPresenceGetStringCallback(RichPresenceStringHandler);
    virtual void SetFriendPresenceOnline(bool);
    virtual void DisplayFullVersionPurchase(bool, int, int);
    virtual void SetUpsellCallback(UpsellHandler, void*);
    virtual void SetDebugFullOverride(bool);
    virtual void SetNetworkStatus(bool);
    virtual void SetNetworkStatusWithoutSigninChange(bool);
    virtual void TryToConnectOnline();
    virtual unsigned int GetSignedInUsersMask();
    virtual void RefreshChatAndContentRestrictions(ProfileReadErrorHandler, void*);
    virtual bool GetChatAndContentRestrictions(int, bool, bool*, bool*, int*);
    virtual void SetServiceID(char*);
    virtual void HandleNetworkTicket(int, void*);
    virtual void SetMinimumAge(int);
    virtual unsigned int getUserID(int);
    virtual int getQuadrant(ACTUuid&);
    virtual void SetSignInChoicesCallback(SignInChoicesHandler);
    virtual bool IsHasAchievementsFile(int);

    void SetSignInChange(bool);
    PlayerUID QuadrantOfflineXUID(int);
    PlayerUID QuadrantOnlineXUID(int);
    bool GetChatAndContentRestrictions(int, bool*, bool*, int*);
    void CheckForSignInChange();
    bool GetSignInChange();
    bool PrimaryPlayerChanged();
    bool PrimaryPlayerHasSignedOutDialogBox(bool);
    void SetProfileProcessState(EProfileProcessState);
    bool IsValidPlayer(int);
    int GetSlotId(int);
    const wchar_t* getStringFromID(int);
    void CreatePresenceString(int, std::wstring&);
    void SetPresenceString(int);
    void RichPresenceOnCreate();
    void TickRichPresence();
    void SetUpsellType(eUpsellType, int);
    void DebugPrintf(const char*, ...);
    void SystemOnCreate();
    void SetLeaderboardDisable(bool);
    void CallSignInChangeCallback(bool);
    bool AddAwardManagerTask(CAchievements*);
    int GetRegistrationId(int) const;
    void OnSignout(int);
    void OnSignin(int);
    CAchievements* getAchievements(int);
    CAchievements* getAchievementsByPad(int) const;
    bool callAwardManagerCallback(int, int) const;
    CAchievements* nextAwardManagerTask();
    void EnterInitSaveDirCS();
    void LeaveInitSaveDirCS();
    bool initSaveDirectory(unsigned char);
    void readAwardManagerFile(CAchievements*);
    void writeAwardManagerFile(CAchievements*);
    static int AwardManagerThreadProc(void*);
    void AwardManagerOnCreate();
    void AwardManagerTick();
    void AwardManagerOnDestroy();
    int CreateAvatarTexture(int);
    static std::wstring LocalPlayerName();
    void SetPlayerName(const std::wstring& name);
    void ProfilePickerClosed(int pad, const PcPlayerProfile* chosen);
    static ACTUuid LocalPlayerUuid(const std::wstring&);

private:
    int mFatalTrophyErrorId;
    EProfileProcessState mProfileProcessState;
    std::wstring mPlayerName;
    PROFILE_DETAILS mProfiles[4];
    bool mSignInChange;
    bool mPrimaryPlayerChanged;
    unsigned int mSignedInUserCount;
    unsigned int mSignedInUsersMask;
    bool mRequestOnline;
    bool mRequestGuest;
    bool mRequestSubAccount;
    bool mRequestForce;
    bool mRequestTrial;
    bool mRequestFromGame;
    int mLockedProfile;
    std::deque<int> mSignOutQueue;
    SignInUIHandler mSignInUIHandler;
    int mPickerPad = -1;
    SignInChoicesHandler mSignInChoicesHandler;
    void* mSignInUIContext;
    bool mSignInChoicesPending;
    bool mSignInChoicesAccepted;
    int mSignInChoice;
    int mSignInChoiceExtra;
    OfflineProfileHandler mOfflineProfileHandler;
    void* mOfflineProfileContext;
    int mOfflineProfileQuadrant;
    GetStringHandler mGetStringHandler;
    int mPlayerListTitleId;
    char* mServiceId;
    int mMinimumAge;
    bool mNetworkAvailable;
    bool mNetworkStatus;
    bool mFriendPresenceOnline;
    char mNetworkAccountId[19];
    C4JCriticalSection mInitSaveDirCriticalSection;
    std::vector<AvatarRequest> mAvatarRequests;
    bool mFullVersion;
    bool mUpsellFromTrial;
    bool mSystemUIShowing;
    bool mLeaderboardDisable;
    bool mDebugFullOverride;
    int mPrimaryPad;
    int mLiveConnectionStatus;
    SignInChangeHandler mSignInChangeHandler;
    void* mSignInChangeContext;
    ProfileReadErrorHandler mProfileReadErrorHandler;
    void* mProfileReadErrorContext;
    NotificationsHandler mNotificationsHandler;
    void* mNotificationsContext;
    UpsellHandler mUpsellHandler;
    void* mUpsellContext;
    eUpsellType mUpsellType;
    int mUpsellParam;
    std::unordered_map<int, int> mPresenceContexts;
    std::unordered_map<int, int> mPresenceStrings;
    std::unordered_map<int, std::unordered_map<int, int> > mPresenceContextStrings;
    bool mPresenceUpdatePending;
    std::wstring mCurrentPresenceString;
    int mCurrentGameActivity[4];
    CONTEXTVAL mContextValues[4];
    int mRichPresenceTitleId;
    RichPresenceSettingHandler mRichPresenceSettingHandler;
    FriendPresenceHandler mFriendPresenceHandler;
    void* mFriendPresenceContext;
    RichPresenceStringHandler mRichPresenceStringHandler;
    AwardHandler mAwardHandler;
    void* mAwardContext;
    std::set<CAchievements*> mAchievements;
    CAchievements* mAchievementsByPad[4];
    unsigned int mInitialisedSaveDirectories;
    AwardRegistration mAwardRegistrations[kAwardCount];
    std::deque<CAchievements*> mAwardQueue;
    C4JThread* mAwardThread;
    bool mAwardThreadExit;
    C4JCriticalSection mAwardMutex;
};

extern C_4JProfile* gProfile;
extern CProfile* gProfileInstance;

#endif
