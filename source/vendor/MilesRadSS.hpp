#ifndef SOURCE_VENDOR_MILES_RADSS_HPP
#define SOURCE_VENDOR_MILES_RADSS_HPP

#include "platform/Platform.hpp"

#if LCE_PLATFORM_WIIU
#include <sndcore2/voice.h>
#endif

#include "vendor/MilesApi.hpp"


struct _RadSoundSystem;

enum RADSS_PROPERTY
{
    RADSS_PROPERTY_AX_QUERY = 5000
};

typedef void (*RADSS_SET_MEMORY_FUNCTION)(_RadSoundSystem* pSystem, void* pMemory);
typedef int (*RADSS_GET_READY_BYTES_FUNCTION)(_RadSoundSystem* pSystem);
typedef int (*RADSS_SET_PAUSED_FUNCTION)(_RadSoundSystem* pSystem, int iPaused);
typedef int (*RADSS_SET_ON_OFF_FUNCTION)(_RadSoundSystem* pSystem, int iOn);
typedef int (*RADSS_CLOSE_FUNCTION)(_RadSoundSystem* pSystem);
typedef int (*RADSS_LOCK_FUNCTION)(_RadSoundSystem* pSystem, void** ppBuffer, unsigned int uBytes);
typedef int (*RADSS_UNLOCK_FUNCTION)(_RadSoundSystem* pSystem);
typedef unsigned int (*RADSS_GET_PROPERTY_FUNCTION)(_RadSoundSystem* pSystem, RADSS_PROPERTY property,
                                                    unsigned int* puValue);
typedef void (*RADSS_VOLUME_FUNCTION)(_RadSoundSystem* pSystem, float fFirst, float fSecond);
typedef void (*RADSS_CHANNEL_ROUTE_FUNCTION)(_RadSoundSystem* pSystem, short* pRouting, int iCount);

#if LCE_PLATFORM_WIIU
static const int RADSS_AX_VOICE_COUNT = 8;
static const int RADSS_AX_ROUTING_ENTRIES = 64;
static const unsigned int RADSS_AX_ROUTING_BYTES = 0x80;

struct RadSS_WiiUPrivate
{
    AXVoice* pVoices[RADSS_AX_VOICE_COUNT];
    void* pChannelBuffers[RADSS_AX_VOICE_COUNT];
    void* pStaging;
    short* pRouting;
    unsigned int uLockBytes;
    unsigned int uLastTick;
    unsigned int uFreshTicks;
    unsigned int uPollTicks;
    unsigned int uReadyLimit;
    unsigned int uChunkBytes;
    unsigned int uBufferBytes;
    unsigned int uChunkStride;
    unsigned int uStagingBytes;
    unsigned int uPlayOffset;
    unsigned int uWriteOffset;
    unsigned short usLatency;
    short sVolumeFirst;
    short sVolumeSecond;
    unsigned char ucChunkCount;
    unsigned char ucFlags;
};

#elif LCE_PLATFORM_PC
struct SDL_AudioStream;

static const int RADSS_PC_CHANNEL_LIMIT = 8;
static const int RADSS_PC_ROUTING_ENTRIES = 64;
static const unsigned int RADSS_PC_ROUTING_BYTES = 0x80;

struct RadSS_PCPrivate
{
    SDL_AudioStream* pStream;
    void* pStaging;
    short* pRouting;
    float* pMix;
    unsigned long long uStartTime;
    unsigned long long uFramesWritten;
    unsigned int uLockBytes;
    unsigned int uStagingBytes;
    unsigned int uBufferFrames;
    unsigned int uReadyLimitFrames;
    unsigned int uReserveFrames;
    unsigned int uChunkFrames;
    unsigned short usLatency;
    short sVolumeFirst;
    short sVolumeSecond;
    unsigned char ucFlags;
    unsigned char ucDeviceOpen;
};
#endif

struct _RadSoundSystem
{
    const char* pError;
    RADSS_SET_MEMORY_FUNCTION pSetMemory;
    RADSS_GET_READY_BYTES_FUNCTION pGetReadyBytes;
    RADSS_SET_PAUSED_FUNCTION pSetPaused;
    RADSS_SET_ON_OFF_FUNCTION pSetOnOff;
    RADSS_CLOSE_FUNCTION pClose;
    RADSS_LOCK_FUNCTION pLock;
    RADSS_UNLOCK_FUNCTION pUnlock;
    RADSS_GET_PROPERTY_FUNCTION pGetProperty;
    RADSS_VOLUME_FUNCTION pVolume;
    RADSS_CHANNEL_ROUTE_FUNCTION pChannelRoute;
    void* pMemory;
    unsigned int uMemoryBytes;
    unsigned char ucReserved34;
    unsigned char ucChannels;
    unsigned char ucPaused;
    unsigned char ucEnabled;
    unsigned short usRate;
    unsigned short usUnderruns;
    unsigned int uReserved3C;
#if LCE_PLATFORM_WIIU
    RadSS_WiiUPrivate priv;
#elif LCE_PLATFORM_PC
    RadSS_PCPrivate priv;
#endif
};

#if LCE_PLATFORM_WIIU
static const unsigned int RADSS_GENERIC_SIZE = 0xB8;
#elif LCE_PLATFORM_PC
extern "C" RADSS_OPEN_DRIVER_CALLBACK RADSS_SDLInstallDriver(int iDevice);
#endif


#endif
