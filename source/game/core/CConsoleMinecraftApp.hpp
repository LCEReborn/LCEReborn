#ifndef GAME_CORE_CCONSOLEMINECRAFTAPP_HPP
#define GAME_CORE_CCONSOLEMINECRAFTAPP_HPP

#include "game/core/CMinecraftApp.hpp"
#include "game/client/render/Renderer.hpp"
#include "platform/C4JStorage.hpp"
#include "platform/Platform.hpp"
#include <stdint.h>
#include <string>

class ConsoleUIController;
#if LCE_PLATFORM_WIIU
struct MiiDataEntry;
#endif

class CConsoleMinecraftApp : public CMinecraftApp
{
public:
    CConsoleMinecraftApp();
    bool IsAmericanSKU();
    static void RequestToBackground();
    static uint8_t ReadyToBackground();
    bool GetDRCIconEnabled();
    void SetDRCOuputEnabled(bool);
    bool GetDRCOutputEnabled();
    uint8_t GetDRCCheckPlayerCountEnabled();
    void FatalLoadError(const wchar_t*, ...);
    uint32_t ReadProductCodes();
    uint32_t GetProductSKU();
    static int RequestSignInUIChoices(int);
    static int DisplaySavingMessageWiiU(void*, C4JStorage::ESavingMessage, int);
    void SetNoSpaceForSaveQuota(bool);
    static int Callback_SaveGameIncompleteMessageBoxReturned(void*, int, C4JStorage::EMessageResult);
    static void Callback_SaveGameIncomplete(void*, C4JStorage::ESaveIncompleteType);
    void SetDRCIconEnabled(bool);
    void SetRichPresenceContext(int, int);
    uint8_t HasNoSpaceForSaveQuota();
    void SetDRCCheckPlayerCountEnabled(bool);
    bool getIgnoreMainMenuInputDuringInvite();
    void ReleaseSaveThumbnail();
    void ExitGame();
    void StoreLaunchData();
    void CaptureSaveThumbnail();
    void GetSaveThumbnail(unsigned char**, unsigned int*);
    void GetScreenshot(int, unsigned char**, unsigned int*);
    uint32_t LoadLocalTMSFile(wchar_t*);
    uint32_t LoadLocalTMSFile(wchar_t*, eFileExtensionType);
    void FreeLocalTMSFiles(eTMSFileType);
    uint32_t GetLocalTMSFileIndex(wchar_t*, bool, eFileExtensionType);
    virtual void TemporaryCreateGameStart();
#if LCE_PLATFORM_WIIU
    void RequestSignInUIWiiUReturnedSelected(MiiDataEntry*, int, int);
#endif
    uint8_t IsDrcPreventingSplitscreen();
    void setIgnoreMainMenuInputDuringInvite(bool);
    void ReadBannedList(int, eTMSAction, bool);
    uint32_t LoadLocalTMSFile(char*);
    uint32_t GetProductCode();
    const wchar_t* GetSaveFolderPrefix();
    bool IsJapaneseSKU();
    bool IsEuropeanSKU();
    bool IsChinaSKU();
    void SystemServiceTick();
    void SaveDataDialogTick();
    void ShowPatchAvailableError();
    void PatchAvailableDialogTick();

private:
    friend class ConsoleUIController;
    ImageFileBuffer m_saveThumbnail;
    unsigned char m_signInReserved[12];
    unsigned char m_signInResult;
    bool m_ignoreMainMenuInputDuringInvite;
    unsigned char m_inviteReserved[3];
    unsigned char m_noSpaceForSaveQuota;
    bool m_drcIconEnabled;
    bool m_drcOutputEnabled;
    unsigned char m_drcCheckPlayerCountEnabled;
    char m_productCode[12];
    int m_productSKU;
    unsigned char m_instanceReserved[8];
};

extern CConsoleMinecraftApp theApp;

void RegisterAwardsWithProfileManager();
void ShutdownAndExit();

#endif
