#ifndef PLATFORM_C4JPROFILE_HPP
#define PLATFORM_C4JPROFILE_HPP

#include <string>
#include <vector>

#include "game/util/ACTUuid.hpp"
#include "game/world/generation/PlayerUID.hpp"
#include "platform/Platform.hpp"

#if LCE_PLATFORM_WIIU
#include <nn/ffl/miidata.h>
#include <nn/fp/fp_cpp.h>

#include "platform/wiiu/MiiData.hpp"
#endif

class CAchievements;

enum eAwardType
{
    eAwardType_None = 0
};

#if LCE_PLATFORM_WIIU
struct ProfileSignInEntry
{
    int quadrant;
    MiiDataEntry* mii;
    bool online;
    bool guest;
};
#endif

class C_4JProfile
{
public:
    typedef int (*SignInUIHandler)(void*, bool, int, int);
    typedef int (*OfflineProfileHandler)(void*, bool, int);
    typedef int (*AvatarHandler)(void*, unsigned char*, unsigned int);
    typedef const wchar_t* (*GetStringHandler)(int);
    typedef void (*SignInChangeHandler)(void*, bool, unsigned int);
    typedef void (*NotificationsHandler)(void*, unsigned int, unsigned int);
    typedef void (*ProfileReadErrorHandler)(void*);
    typedef void (*AwardHandler)(void*, int, int, int, int);
    typedef void (*FriendPresenceHandler)(unsigned int, void*);
    typedef wchar_t* (*RichPresenceStringHandler)(int);
    typedef int (*RichPresenceSettingHandler)(const void*);
    typedef int (*SignInChoicesHandler)(int);

    virtual void Initialise(const void*, const void*, unsigned short, unsigned int, unsigned int, unsigned int*, int,
                            unsigned int*) = 0;
    virtual int GetLockedProfile() = 0;
    virtual void SetLockedProfile(int) = 0;
    virtual bool IsSignedIn(int) = 0;
    virtual bool IsSignedInLive(int) = 0;
    virtual bool IsSigningIn(int) = 0;
    virtual bool IsGuest(int) = 0;
    virtual int RequestSignInUI(bool, bool, bool, bool, bool, SignInUIHandler, void*, int) = 0;
#if LCE_PLATFORM_WIIU
    virtual int SignInUser(MiiDataEntry*, int) = 0;
    virtual void SignOutUser(FFLCreateID&) = 0;
#endif
    virtual void SignOutUser(int) = 0;
    virtual void SetUserSigningIn(int, bool) = 0;
    virtual int DisplayOfflineProfile(OfflineProfileHandler, void*, int) = 0;
    virtual void RequestConvertOfflineToGuestUI(SignInUIHandler, void*, int) = 0;
    virtual void SetPrimaryPlayerChanged(bool) = 0;
    virtual int QuerySigninStatus() = 0;
    virtual void GetXUID(int, PlayerUID*, bool) = 0;
    virtual int AreXUIDSEqual(PlayerUID, PlayerUID) = 0;
    virtual bool AllowedToPlayMultiplayer(int) = 0;
    virtual void AllowedPlayerCreatedContent(int, bool, int*, int*) = 0;
    virtual bool CanViewPlayerCreatedContent(int, bool, PlayerUID*, unsigned int) = 0;
    virtual bool GetProfileAvatar(int, AvatarHandler, void*) = 0;
    virtual void CancelProfileAvatarRequest() = 0;
    virtual void SetGetStringFunc(GetStringHandler) = 0;
    virtual void SetPlayerListTitleID(int) = 0;
    virtual void SetFatalTrophyErrorID(int) = 0;
    virtual int GetPrimaryPad() = 0;
    virtual void SetPrimaryPad(int) = 0;
    virtual void ClearGameUsers() = 0;
    virtual wchar_t* GetMiiName(int) = 0;
    virtual wchar_t* GetGamertag(int) = 0;
    virtual std::wstring GetDisplayName(int) = 0;
    virtual char* GetNetworkAccountID() = 0;
    virtual void SetSignInChangeCallback(SignInChangeHandler, void*) = 0;
    virtual void SetSignInChoicesComplete(bool, int, int) = 0;
    virtual void SetNotificationsCallback(NotificationsHandler, void*) = 0;
    virtual bool RegionIsNorthAmerica() = 0;
    virtual bool LocaleIsUSorCanada() = 0;
    virtual int GetLiveConnectionStatus() = 0;
    virtual bool IsSystemUIDisplayed() = 0;
    virtual void SetProfileReadErrorCallback(ProfileReadErrorHandler, void*) = 0;
    virtual void ResetProfileProcessState() = 0;
    virtual void Tick() = 0;
    virtual void RegisterAward(int, int, eAwardType, int, int) = 0;
    virtual int GetAwardId(int) = 0;
    virtual eAwardType GetAwardType(int) = 0;
    virtual bool CanBeAwarded(int, int) = 0;
    virtual void Award(int, int, bool) = 0;
    virtual bool IsAwardsFlagSet(int, int) = 0;
    virtual void SetOnAwardHandler(AwardHandler, void*) = 0;
    virtual void Terminate() = 0;
    virtual void RichPresenceRegisterPresenceString(int, int) = 0;
    virtual void RichPresenceRegisterContext(int, int) = 0;
    virtual void RichPresenceRegisterContextString(int, int, int) = 0;
    virtual void RichPresenceInit(int, int) = 0;
    virtual void SetRichPresenceContextValue(int, int, int) = 0;
    virtual void SetCurrentGameActivity(int, int, bool) = 0;
    virtual void SetRichPresenceSettingFn(RichPresenceSettingHandler) = 0;
#if LCE_PLATFORM_WIIU
    virtual void SetRichPresenceGameModeData(nn::fp::GameMode&) = 0;
#endif
    virtual void SetFriendPresenceChangeCallback(FriendPresenceHandler, void*) = 0;
    virtual void SetRichPresenceGetStringCallback(RichPresenceStringHandler) = 0;
    virtual void SetFriendPresenceOnline(bool) = 0;
    virtual void SetNetworkStatus(bool) = 0;
    virtual void SetNetworkStatusWithoutSigninChange(bool) = 0;
    virtual void TryToConnectOnline() = 0;
    virtual unsigned int GetSignedInUsersMask() = 0;
    virtual void RefreshChatAndContentRestrictions(ProfileReadErrorHandler, void*) = 0;
    virtual bool GetChatAndContentRestrictions(int, bool, bool*, bool*, int*) = 0;
    virtual void SetServiceID(char*) = 0;
    virtual void HandleNetworkTicket(int, void*) = 0;
    virtual void SetMinimumAge(int) = 0;
    virtual unsigned int getUserID(int) = 0;
    virtual int getQuadrant(ACTUuid&) = 0;
    virtual void SetSignInChoicesCallback(SignInChoicesHandler) = 0;
#if LCE_PLATFORM_WIIU
    virtual std::vector<ProfileSignInEntry>* GetSignInEntries() = 0;
    virtual void LoadAllMiiData() = 0;
    virtual void FreeAllMiiData() = 0;
    virtual int GetNumSysUserMiis() = 0;
    virtual int GetNumFavouriteMiis() = 0;
    virtual int GetNumGuestMiis() = 0;
    virtual MiiDataEntry* GetSysUserMiiData(int) = 0;
    virtual MiiDataEntry* GetFavouriteMiiData(int) = 0;
    virtual MiiDataEntry* GetGuestMiiData(int) = 0;
#endif
    virtual bool IsHasAchievementsFile(int) = 0;
};

#endif
