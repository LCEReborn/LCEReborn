#ifndef PLATFORM_PC_CSTORAGE_HPP
#define PLATFORM_PC_CSTORAGE_HPP

#include <atomic>
#include <mutex>
#include <stdint.h>
#include <string>
#include <thread>
#include <vector>

#include "platform/C4JStorage.hpp"

const unsigned int SAVE_DATA_QUOTA = 0x1E000000;
const unsigned int PNG_CHUNK_TYPE_IEND = 0x49454e44U;
const unsigned int PNG_CHUNK_TYPE_TEXT = 0x74455874U;

struct CONTENT_DATA
{
    unsigned int state;
    unsigned char source;
    unsigned char reserved[7];
    char name[256];
    char path[640];
    int index;
};

class CStorage : public C4JStorage
{
public:
    struct DriveMapping
    {
        std::string drive;
        std::string path;

        DriveMapping();
        DriveMapping(const std::string&, const std::string&);
    };

    CStorage();
    ~CStorage();

    virtual void Tick();
    virtual void Init(unsigned int, const wchar_t*, const char*, int, SavingMessageCallback, void*, const char*, bool*);
    virtual void SetGameSaveFolderTitle(const wchar_t*);
    virtual void SetSaveCacheFolderTitle(const wchar_t*);
    virtual void SetOptionsFolderTitle(const wchar_t*);
    virtual void SetCorruptSaveName(const wchar_t*);
    virtual void SetGameSaveFolderPrefix(const char*);
    virtual void SetMaxSaves(int);
    virtual void SetDefaultImages(unsigned char*, unsigned int, unsigned char*, unsigned int, unsigned char*,
                                  unsigned int);
    virtual void SetIncompleteSaveCallback(IncompleteSaveCallback, void*);
    virtual void SetSaveDisabled(bool);
    virtual unsigned char GetSaveDisabled();
    virtual void ResetSaveData();
    virtual int DoesSaveExist(bool*);
    virtual int EnoughSpaceForAMinSaveGame();
    virtual C4JStorage::ESaveGameState GetSaveState();
    virtual int GetSavesInfo(int, SavesInfoCallback, void*, const char*);
    virtual SAVE_DETAILS* ReturnSavesInfo();
    virtual void ClearSavesInfo();
    virtual C4JStorage::ESaveGameState LoadSaveDataThumbnail(SAVE_INFO*, int (*)(void*, unsigned char*, unsigned int),
                                                             void*);
    virtual C4JStorage::ESaveGameState LoadSaveData(SAVE_INFO*, LoadCallback, void*);
    virtual int GetSaveSize();
    virtual void GetSaveData(void*, unsigned int*);
    virtual int GetSaveUniqueNumber(int*);
    virtual int GetSaveUniqueFilename(char*);
    virtual void SetSaveUniqueFilename(char*);
    virtual int GetSaveUniqueFileDir(char*);
    virtual void SetSaveTitle(const wchar_t*);
    virtual void* AllocateSaveData(unsigned int);
    virtual void SetSaveDataSize(unsigned int);
    virtual void GetDefaultSaveImage(unsigned char**, unsigned int*);
    virtual void GetDefaultSaveThumbnail(unsigned char**, unsigned int*);
    virtual void SetSaveImages(unsigned char*, unsigned int, unsigned char*, unsigned int, unsigned char*,
                               unsigned int);
    virtual C4JStorage::ESaveGameState SaveSaveData(SaveCallback, void*);
    virtual void ContinueIncompleteOperation();
    virtual void CancelIncompleteOperation();
    virtual C4JStorage::ESaveGameState DeleteSaveData(SAVE_INFO*, SaveCallback, void*);
    virtual C4JStorage::ESaveGameState CopySaveData(SAVE_INFO*, CopyCallback, CopyProgressCallback, void*);
    virtual C4JStorage::ESaveGameState RenameSaveData(int, wchar_t*, SaveCallback, void*);
    virtual void GetSaveImage(unsigned char**, int*);
    virtual void GetSaveThumbnail(unsigned char**, int*);
    virtual void InitialiseProfileData(unsigned short, unsigned int, unsigned int, unsigned int*, int, unsigned int*);
    virtual int SetDefaultOptionsCallback(DefaultOptionsCallback, void*);
    virtual void SetOptionsDataCallback(OptionsDataCallback, void*);
    virtual int SetOldProfileVersionCallback(OldProfileVersionCallback, void*);
    virtual C4JStorage::PROFILESETTINGS* GetDashboardProfileSettings(int);
    virtual unsigned char* GetGameDefinedProfileData(int);
    virtual void ReadFromProfile(int, int);
    virtual void WriteToProfile(int, bool, bool);
    virtual void DeleteOptionsData(int);
    virtual void ForceQueuedProfileWrites(int);
    virtual int IsOptionsWriteInProgressOrPending(int);
    virtual void InitialiseGenericData();
    virtual int ReadFromGenericData(int, const std::wstring&, void**, unsigned int*, GenericDataCallback, void*);
    virtual int WriteToGenericData(int, const std::wstring&, void*, unsigned int, GenericDataCallback, void*);
    virtual void DeleteGenericData(int, const std::wstring&);
    virtual int GetSaveDeviceSelected(unsigned int);
    virtual int ReadTMSFile(int, C4JStorage::eGlobalStorage, C4JStorage::eTMS_FileType, wchar_t*, unsigned char**,
                            unsigned int*, int (*)(void*, wchar_t*, int, bool, C4JStorage::eTMS_FileType), void*, int);
    virtual int WriteTMSFile(int, C4JStorage::eGlobalStorage, wchar_t*, unsigned char*, unsigned int);
    virtual int DeleteTMSFile(int, C4JStorage::eGlobalStorage, wchar_t*);
    virtual void SetOpenDirFn(DirectoryOpenCallback);
    virtual void SetReadDirFn(DirectoryReadCallback);
    virtual void SetCloseDirFn(DirectoryCloseCallback);
    virtual void SetDLCPackageRoot(const char*);
    virtual int GetInstalledDLC(int, InstalledDLCCallback, void*);
    virtual intptr_t GetDLC(unsigned int);
    virtual int MountInstalledDLC(int, unsigned int, DLCMountCallback, void*, const char*);
    virtual int UnmountInstalledDLC(const char*);
    virtual int GetMountedDLCFileList(const char*, std::vector<std::string>&);
    virtual int GetMountedDLCIndex();
    virtual std::string GetMountedPath(std::string);
    virtual std::string GetPacksFolder();
    virtual std::string GetPackCacheFolder();
    virtual int AddInstalledPack(const char* path, const char* name);
    virtual void ForgetServerPacks();
    virtual bool ListFiles(const std::string& root, std::vector<std::string>& paths,
                           std::vector<unsigned long long>& sizes);
    virtual bool DeletePath(const std::string& path);
    virtual bool RenamePath(const std::string& from, const std::string& to);
    virtual bool CreateDirectories(const std::string& path);

    void DisplaySavingMessages();
    void Init(SavingMessageCallback, void*, const char*);
    void MakeCRCTable();
    unsigned int UpdateCRC(unsigned int, unsigned char*, int);
    unsigned int CRC(unsigned char*, int);
    void ClearProfileWriteTimer(int);
    void SetOptionsIcon(unsigned char*, unsigned int);
    std::string GetOptionsFilename(int);
    unsigned int SaveOptionsThreadProc(int);
    unsigned int LoadOptionsThreadProc(int);
    void CheckForProfileUpdatesRequired();
    void ProcessOptionsUpdate();
    void TickOptions();
    int IsOptionsWriteInProgress();
    void DeleteOptionsFile(int);
    static int saveInfoSortfn(const void*, const void*);
    unsigned int GetSavesInfoThreadProc();
    unsigned int LoadGameThreadProc();
    void CreateSaveUniqueName();
    void AddTextFieldToPNG(unsigned char*, unsigned int, unsigned char*, unsigned int, unsigned int);
    unsigned int SaveGameThreadProc();
    unsigned int DeleteGameThreadProc();
    unsigned int CopyGameThreadProc();
    unsigned int RenameGameThreadProc();
    void InitSaveGame(unsigned int, const wchar_t*, const char*, int, bool*);
    void SetDefaultImages(unsigned char*, unsigned int, unsigned char*, unsigned int);
    void TickSaveGame();
    void AddInstalled(CONTENT_DATA&);
    void AddPack(const std::string&, unsigned char, const char* label = NULL);
    unsigned int EnumerateLocalDLCPackages();
    void MountInstalledContent();
    void DLCOnCreate();
    void TickDLC();
    unsigned int LoadGenericDataThreadProc(int);
    unsigned int SaveGenericDataThreadProc(int);
    void GenericDataTick();
    unsigned int GetOptionsIconSize();
    unsigned char* GetOptionsIconData();

private:
    struct DirectoryEntry
    {
        std::string name;
        bool isDirectory;
        unsigned long long size;
    };

    bool ListDirectory(const std::string&, std::vector<DirectoryEntry>&);
    bool ListFilesUnder(const std::string&, const std::string&, std::vector<std::string>&,
                        std::vector<unsigned long long>&);
    static void ReleaseSaveEntries(SAVE_DETAILS*);
    void StartThread(std::thread&, unsigned int (CStorage::*)());
    void StartThread(std::thread&, unsigned int (CStorage::*)(int), int);
    static void JoinThread(std::thread&);

    unsigned int m_crcTable[256];
    bool m_crcReady;
    SavingMessageCallback m_savingMessageCallback;
    void* m_savingMessageContext;
    std::mutex m_storageMutex;
    std::thread m_saveThreads[6];
    std::atomic<C4JStorage::ESaveGameState> m_saveState;
    C4JStorage::ESaveGameState m_previousSaveState;
    std::atomic<unsigned int> m_savesInfoState;
    bool m_savesInfoAvailable;
    SAVE_DETAILS* m_savesInfo;
    std::vector<SAVE_DETAILS*> m_retiredSavesInfo;
    SAVE_INFO* m_selectedSave;
    char m_saveUniqueFilename[128];
    wchar_t m_saveTitle[128];
    wchar_t m_gameSaveFolderTitle[128];
    wchar_t m_saveCacheFolderTitle[128];
    wchar_t m_optionsFolderTitle[128];
    wchar_t m_corruptSaveName[128];
    IncompleteSaveCallback m_incompleteSaveCallback;
    void* m_incompleteSaveContext;
    SavesInfoCallback m_savesInfoCallback;
    void* m_savesInfoContext;
    SaveCallback m_saveCallback;
    void* m_saveCallbackContext;
    LoadCallback m_loadCallback;
    void* m_loadCallbackContext;
    SaveCallback m_deleteCallback;
    void* m_deleteCallbackContext;
    SaveCallback m_renameCallback;
    void* m_renameCallbackContext;
    CopyCallback m_copyCallback;
    CopyProgressCallback m_copyProgressCallback;
    void* m_copyCallbackContext;
    wchar_t m_renameTitle[128];
    int m_renameSaveIndex;
    char m_gameSaveFolderPrefix[12];
    int m_maxSaves;
    unsigned char* m_saveData;
    unsigned int m_saveSize;
    unsigned char* m_saveImage;
    unsigned int m_saveImageSize;
    unsigned char* m_saveThumbnail;
    unsigned int m_saveThumbnailSize;
    bool m_saveDisabled;
    unsigned char* m_defaultImage;
    unsigned int m_defaultImageSize;
    unsigned char* m_defaultThumbnail;
    unsigned int m_defaultThumbnailSize;
    unsigned int m_incompleteReason;
    std::thread m_profileSaveThreads[4];
    std::thread m_profileLoadThreads[4];
    unsigned char* m_profileReadBuffers[4];
    unsigned char* m_profileBuffers[4];
    unsigned short m_profileVersion;
    unsigned int m_profileCount;
    unsigned int m_optionCount;
    bool m_profileUpdateRequired[4];
    unsigned char* m_optionsIcon;
    unsigned int m_optionsIconSize;
    unsigned int* m_optionDefaults;
    struct ProfileWriteState
    {
        std::atomic<int> state;
        unsigned int timer;
    } m_profileWriteState[4];
    struct ProfileReadState
    {
        std::atomic<int> state;
        unsigned int value;
    } m_profileReadState[4];
    C4JStorage::PROFILESETTINGS m_dashboardProfileSettings[4];
    unsigned int* m_gameProfileSizes;
    int m_gameProfileStride;
    unsigned char* m_gameProfileData;
    int m_profileResult;
    DefaultOptionsCallback m_defaultOptionsCallback;
    void* m_defaultOptionsContext;
    OptionsDataCallback m_optionsDataCallback;
    void* m_optionsDataContext;
    OldProfileVersionCallback m_oldProfileVersionCallback;
    void* m_oldProfileVersionContext;
    std::atomic<int> m_genericState;
    int m_genericSlot;
    unsigned int m_genericDataSize;
    void* m_genericWriteData;
    unsigned char* m_genericLoadedData;
    unsigned int m_genericLoadedSize;
    unsigned int* m_genericReadSize;
    void** m_genericReadData;
    std::wstring m_genericFilename;
    std::thread m_genericThread;
    GenericDataCallback m_genericCallback;
    void* m_genericContext;
    InstalledDLCCallback m_installedCallback;
    void* m_installedContext;
    int m_dlcState;
    std::vector<CONTENT_DATA> m_installedContent;
    int m_dlcMountState;
    DLCMountCallback m_dlcMountCallback;
    void* m_dlcMountContext;
    std::string m_dlcMountName;
    unsigned int m_mountedDLCIndex;
    unsigned int m_dlcMountResult;
    char m_defaultDLCMountName[40];
    std::mutex m_mountedDrivesMutex;
    std::vector<DriveMapping> m_mountedDrives;
    DirectoryOpenCallback m_openDir;
    DirectoryReadCallback m_readDir;
    DirectoryCloseCallback m_closeDir;
    int m_dlcDirectory;
    FSDirEntry m_dlcDirectoryEntry;
};

#endif
