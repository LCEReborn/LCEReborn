#include "game/core/CConsoleMinecraftApp.hpp"
#include "game/core/StringIds.hpp"
#include "game/core/ConsoleUIController.hpp"
#include "game/core/ShutdownManager.hpp"
#include "game/network/CGameNetworkManager.hpp"
#include "game/ui/UIScene_AchievementsMenu.hpp"
#include "platform/C4JThread.hpp"
#include "platform/CInput.hpp"
#include "platform/CStorage.hpp"
#include "platform/C4JThreadImpl.hpp"
#include "platform/FileSystem.hpp"
#include "platform/FileApi.hpp"
#include "platform/Runtime.hpp"
#include <stdlib.h>
#include <string.h>
#include "platform/Platform.hpp"

#if LCE_PLATFORM_WIIU
#include "platform/wiiu/WiiU_Platform.hpp"
#include <sysapp/launch.h>

extern "C" void OSReport(const char*, ...);
extern "C" void ProcUIShutdown();
#elif LCE_PLATFORM_PC
#include <cstdarg>
#include <cstdio>

#include "platform/pc/Log.hpp"
#include "platform/pc/PcApplication.hpp"
#include "platform/pc/PcPlatform.hpp"

static const Logger sLog("App");
#endif

extern void altMain();
extern ConsoleUIController gConsoleUIController;
extern C4JStorage* gStorage;

static unsigned char s_requestBackground = 0;
static unsigned char s_signInRequested = 0;

unsigned char s_altMainPending = 0;
unsigned char s_altMainOKToRun = 0;
unsigned char s_altMainExit = 0;

void setAltMainOKToRun()
{
    s_altMainOKToRun = 1;
}

void RegisterAwardsWithProfileManager()
{
    for (int i = 0; i < UIScene_AchievementsMenu::DISPLAY_COUNT; ++i)
    {
        const AchievementInfo* info = UIScene_AchievementsMenu::GetInfo(UIScene_AchievementsMenu::sDisplayList[i]);
        gProfile->RegisterAward(info->id, info->registrationId, eAwardType_None, info->title, info->description);
    }

    gProfile->RichPresenceInit(-1, -1);
    gProfile->SetRichPresenceGetStringCallback(GetRichPresenceStringFromId);
    gProfile->RichPresenceRegisterContext(0, 0x0d21c173);

    gProfile->RichPresenceRegisterPresenceString(0, 0x0d371d4e);
    gProfile->RichPresenceRegisterPresenceString(1, 0xb0e2d942);
    gProfile->RichPresenceRegisterPresenceString(2, 0xeb1a4a80);
    gProfile->RichPresenceRegisterPresenceString(3, 0x7703ee79);
    gProfile->RichPresenceRegisterPresenceString(4, 0x32d9dd56);
    gProfile->RichPresenceRegisterPresenceString(5, 0x28b67b1f);
    gProfile->RichPresenceRegisterContextString(0, 0, 0x2292bf7f);
    gProfile->RichPresenceRegisterContextString(0, 1, 0x01e201d3);
    gProfile->RichPresenceRegisterContextString(0, 2, 0x05c4c95e);
    gProfile->RichPresenceRegisterContextString(0, 3, 0xb231c7f5);
    gProfile->RichPresenceRegisterContextString(0, 4, 0xc5036f69);
    gProfile->RichPresenceRegisterContextString(0, 5, 0x074e6d4f);
    gProfile->RichPresenceRegisterContextString(0, 6, 0x711f8b99);
    gProfile->RichPresenceRegisterContextString(0, 7, 0x3a2982d5);
    gProfile->RichPresenceRegisterContextString(0, 8, 0xa6dd54da);
    gProfile->RichPresenceRegisterContextString(0, 9, 0xbe2f93d1);
    gProfile->RichPresenceRegisterContextString(0, 10, 0x559d1b96);
    gProfile->RichPresenceRegisterContextString(0, 11, 0x123e6b5f);
    gProfile->RichPresenceRegisterContextString(0, 12, 0x59c6e2b1);
    gProfile->RichPresenceRegisterContextString(0, 13, 0xddd337be);
}

void ShutdownAndExit()
{
#if LCE_PLATFORM_WIIU
    gRenderer->WiiUWaitDrawDone();
    ShutdownManager::MainThreadHandleShutdown();
    ProcUIShutdown();
#elif LCE_PLATFORM_PC
    ShutdownManager::MainThreadHandleShutdown();
    Log::Stop();
#endif
    _Exit(0);
}

CConsoleMinecraftApp::CConsoleMinecraftApp() : CMinecraftApp()
{
    memset(m_productCode, 0, sizeof(m_productCode) + sizeof(m_productSKU));
    m_noSpaceForSaveQuota = false;
    m_drcCheckPlayerCountEnabled = true;
    m_signInResult = 0;
#if LCE_PLATFORM_WIIU
    m_drcOutputEnabled = true;
#elif LCE_PLATFORM_PC
    m_drcOutputEnabled = false;
#endif
    m_drcIconEnabled = false;
}
bool CConsoleMinecraftApp::IsAmericanSKU()
{
    return m_productSKU == 1;
}
void CConsoleMinecraftApp::RequestToBackground()
{
    s_requestBackground = 1;
}
uint8_t CConsoleMinecraftApp::ReadyToBackground()
{
    return (uint8_t)(s_requestBackground ^ 1);
}
bool CConsoleMinecraftApp::GetDRCIconEnabled()
{
    return m_drcIconEnabled;
}
void CConsoleMinecraftApp::SetDRCOuputEnabled(bool value)
{
    m_drcOutputEnabled = value;
}
bool CConsoleMinecraftApp::GetDRCOutputEnabled()
{
    return m_drcOutputEnabled;
}
uint8_t CConsoleMinecraftApp::GetDRCCheckPlayerCountEnabled()
{
    return m_drcCheckPlayerCountEnabled;
}
void CConsoleMinecraftApp::FatalLoadError(const wchar_t* message, ...)
{
#if LCE_PLATFORM_WIIU
    gRenderer->InitialiseContext();
    unsigned int errorCode = GetLastFileErrorCodeForViewer();
    g_pInput->RequestErrorViewer(1, errorCode, NULL, NULL);
    altMain();
    for (;;)
    {
        if (s_altMainPending != 0)
            s_altMainOKToRun = 1;
        Sleep(10);
        if (s_altMainExit != 0 && C4JThread::isMainThread())
            ShutdownAndExit();
    }
#elif LCE_PLATFORM_PC
    va_list arguments;
    va_start(arguments, message);
    sLog.WriteWideV(LogLevel::Fatal, message, arguments);
    va_end(arguments);
    ShutdownAndExit();
#endif
}
uint32_t CConsoleMinecraftApp::ReadProductCodes()
{
    int handle = CreateFileA("WiiU/WiiUProductCodes.bin", GENERIC_READ, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    if (handle == -1)
        return 0;
    unsigned int bytesRead = 0;
    const bool read = ReadFile(handle, m_productCode, 9, &bytesRead, 0) != 0 && bytesRead == 9;
    CloseHandle(handle);
    if (!read)
        return 0;

    if (strcmp(m_productCode, "AUMP01D75") == 0)
        m_productSKU = 0;
    else if (strcmp(m_productCode, "AUME01D9D") == 0)
        m_productSKU = 1;
    else if (strcmp(m_productCode, "AUMJ01DBE") == 0)
        m_productSKU = 2;
    return 1;
}
uint32_t CConsoleMinecraftApp::GetProductSKU()
{
    return (uint32_t)m_productSKU;
}
int CConsoleMinecraftApp::RequestSignInUIChoices(int playerId)
{
    gConsoleUIController.NavigateToScene(playerId, EUIScene_AvatarSelectMenu, NULL, EUILayer_Scene,
                                         EUIGroup_PlayerDefault);
    s_signInRequested = 1;
    return 0;
}
int CConsoleMinecraftApp::DisplaySavingMessageWiiU(void*, C4JStorage::ESavingMessage message, int playerId)
{
    if (s_requestBackground != 0 && message == C4JStorage::ESavingMessage_0)
        s_requestBackground = 0;
    gConsoleUIController.ShowSavingMessage((unsigned int)playerId, message);
    return 0;
}
void CConsoleMinecraftApp::SetNoSpaceForSaveQuota(bool value)
{
    m_noSpaceForSaveQuota = value;
}
int CConsoleMinecraftApp::Callback_SaveGameIncompleteMessageBoxReturned(void* context, int playerId,
                                                                        C4JStorage::EMessageResult result)
{
    CMinecraftApp* app = static_cast<CMinecraftApp*>(context);
    if (static_cast<unsigned int>(result) < C4JStorage::EMessageResult_Cancelled)
        return 0;
    if (result != C4JStorage::EMessageResult_Cancelled)
    {
        if (static_cast<unsigned int>(result) < C4JStorage::EMessageResult_Decline)
        {
            app->LeaveSaveNotificationSection();
            gStorage->ContinueIncompleteOperation();
            return 0;
        }
        if (result != C4JStorage::EMessageResult_Decline)
        {
            if (result != C4JStorage::EMessageResult_ThirdOption)
                return 0;
            gConsoleUIController.NavigateToScene(playerId, EUIScene_InGameSaveManagementMenu, NULL, EUILayer_Alert,
                                                 EUIGroup_Fullscreen);
            return 0;
        }
    }
    app->LeaveSaveNotificationSection();
    gStorage->CancelIncompleteOperation();
    return 0;
}
void CConsoleMinecraftApp::Callback_SaveGameIncomplete(void* context, C4JStorage::ESaveIncompleteType type)
{
    if (type == C4JStorage::ESaveIncompleteType_1 || type == C4JStorage::ESaveIncompleteType_2)
    {
        CMinecraftApp* app = static_cast<CMinecraftApp*>(context);
        app->EnterSaveNotificationSection();
        int messageParameters[] = {(int)IDS_SAVE_INCOMPLETE_RETRY_SAVING, (int)0xce9c596c,
                                   (int)IDS_SAVE_INCOMPLETE_DELETE_SAVES};
        if (gConsoleUIController.RequestErrorMessage(IDS_SAVE_INCOMPLETE_TITLE, 0x92727461, messageParameters, 3, 0xff,
                                                     Callback_SaveGameIncompleteMessageBoxReturned, context, NULL, true)
            != 1)
            return;
    }
    Callback_SaveGameIncompleteMessageBoxReturned(context, 0, C4JStorage::EMessageResult_Cancelled);
}
void CConsoleMinecraftApp::SetDRCIconEnabled(bool value)
{
    m_drcIconEnabled = value;
}
void CConsoleMinecraftApp::SetRichPresenceContext(int context, int value)
{
    gProfile->SetRichPresenceContextValue(context, 0, value);
}
uint8_t CConsoleMinecraftApp::HasNoSpaceForSaveQuota()
{
    return m_noSpaceForSaveQuota;
}
void CConsoleMinecraftApp::SetDRCCheckPlayerCountEnabled(bool value)
{
    m_drcCheckPlayerCountEnabled = value;
}
bool CConsoleMinecraftApp::getIgnoreMainMenuInputDuringInvite()
{
    return m_ignoreMainMenuInputDuringInvite;
}
void CConsoleMinecraftApp::ExitGame()
{
#if LCE_PLATFORM_WIIU
    SYSLaunchMenu();
#elif LCE_PLATFORM_PC
    PcRequestExit();
#endif
}

void CConsoleMinecraftApp::ReleaseSaveThumbnail()
{
    if (m_saveThumbnail.mData != NULL)
    {
        free(m_saveThumbnail.mData);
        m_saveThumbnail.mData = NULL;
    }
}
void CConsoleMinecraftApp::StoreLaunchData()
{
}
void CConsoleMinecraftApp::CaptureSaveThumbnail()
{
    gRenderer->CaptureThumbnail(&m_saveThumbnail);
}
void CConsoleMinecraftApp::GetSaveThumbnail(unsigned char** data, unsigned int* size)
{
    if (m_saveThumbnail.mData != NULL)
    {
        if (data != NULL)
        {
            *data = new unsigned char[m_saveThumbnail.mSize];
            *size = m_saveThumbnail.mSize;
            memcpy(*data, m_saveThumbnail.mData, *size);
            CGameNetworkManager::sInstance.SetWorldThumbnail(*data, *size);
        }
        free(m_saveThumbnail.mData);
        m_saveThumbnail.mData = NULL;
    }
    else if (data != NULL)
    {
        std::vector<unsigned char> world;
        if (CGameNetworkManager::sInstance.GetWorldThumbnail(world))
        {
            *data = new unsigned char[world.size()];
            *size = static_cast<unsigned int>(world.size());
            memcpy(*data, &world[0], world.size());
            return;
        }
        unsigned char* thumbnail;
        unsigned int thumbnailSize;
        gStorage->GetDefaultSaveThumbnail(&thumbnail, &thumbnailSize);
        *data = new unsigned char[thumbnailSize];
        *size = thumbnailSize;
        memcpy(*data, thumbnail, thumbnailSize);
    }
}
void CConsoleMinecraftApp::GetScreenshot(int, unsigned char**, unsigned int*)
{
}
uint32_t CConsoleMinecraftApp::LoadLocalTMSFile(wchar_t*)
{
    return 0xffffffffU;
}
uint32_t CConsoleMinecraftApp::LoadLocalTMSFile(wchar_t*, eFileExtensionType)
{
    return 0xffffffffU;
}
void CConsoleMinecraftApp::FreeLocalTMSFiles(eTMSFileType)
{
}
uint32_t CConsoleMinecraftApp::GetLocalTMSFileIndex(wchar_t*, bool, eFileExtensionType)
{
    return 0xffffffffU;
}
void CConsoleMinecraftApp::TemporaryCreateGameStart()
{
}
#if LCE_PLATFORM_WIIU
void CConsoleMinecraftApp::RequestSignInUIWiiUReturnedSelected(MiiDataEntry* mii, int choice, int pad)
{
    if (mii == NULL)
    {
        gProfile->SetSignInChoicesComplete(false, choice, pad);
    }
    else
    {
        gProfile->SignInUser(mii, pad);
        gProfile->SetSignInChoicesComplete(true, choice, pad);
    }
    m_signInResult = 0;
}
#endif
uint8_t CConsoleMinecraftApp::IsDrcPreventingSplitscreen()
{
    return m_drcOutputEnabled;
}
void CConsoleMinecraftApp::setIgnoreMainMenuInputDuringInvite(bool value)
{
    m_ignoreMainMenuInputDuringInvite = value;
}
void CConsoleMinecraftApp::ReadBannedList(int, eTMSAction, bool)
{
}
