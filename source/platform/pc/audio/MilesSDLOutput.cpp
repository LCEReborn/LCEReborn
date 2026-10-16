#include "vendor/MilesRadSS.hpp"

#include <stddef.h>
#include <string.h>

#include <SDL3/SDL.h>

#include "vendor/MilesTime.hpp"

static const short RADSS_UNITY_VOLUME = 0x7FFF;
static const float RADSS_VOLUME_SCALE = 32767.0f;
static const float RADSS_SAMPLE_SCALE = 1.0f / 32768.0f;
static const float RADSS_DOWNMIX_LEVEL = 0.70710678f;
static const float RADSS_OUTPUT_LIMIT = 1.0f;

static const int RADSS_ROUTING_STRIDE = 8;
static const int RADSS_OUTPUT_CHANNELS = 2;

static const unsigned char RADSS_FLAG_MODE_MASK = 3;
static const unsigned char RADSS_FLAG_ROUTED = 4;
static const unsigned char RADSS_FLAG_PLAYING = 8;

static const unsigned char RADSS_DEVICE_SUBSYSTEM = 1;
static const unsigned char RADSS_DEVICE_STREAM = 2;

static const unsigned int RADSS_MODE_STEREO = 0;
static const unsigned int RADSS_MODE_SURROUND = 1;
static const unsigned int RADSS_MODE_MATRIX = 3;

static const int RADSS_ROW_FRONT_LEFT = 0;
static const int RADSS_ROW_FRONT_RIGHT = 1;
static const int RADSS_ROW_CENTER = 2;
static const int RADSS_ROW_LOW_FREQUENCY = 3;
static const int RADSS_ROW_BACK_LEFT = 4;
static const int RADSS_ROW_BACK_RIGHT = 5;
static const int RADSS_TV_ROW_COUNT = 6;

static const unsigned int RADSS_MAXIMUM_CHUNK_MILLISECONDS = 24;
static const unsigned int RADSS_MINIMUM_CHUNKS = 8;
static const unsigned int RADSS_SAMPLE_ALIGNMENT_MASK = 0xFFFFFFF0;
static const unsigned int RADSS_STAGING_ALIGNMENT = 16;
static const unsigned int RADSS_BYTE_ALIGNMENT_MASK = 0xFFFFFFE0;
static const unsigned int RADSS_MILLISECONDS_PER_SECOND = 1000;
static const unsigned int RADSS_READY_LIMIT_NUMERATOR = 9;
static const unsigned int RADSS_READY_LIMIT_DENOMINATOR = 10;
static const unsigned int RADSS_MINIMUM_READY_BYTES = 0x400;
static const unsigned long long RADSS_NANOSECONDS_PER_SECOND = 1000000000ULL;

static const char RADSS_INVALID_LOCK_SIZE[] = "Invalid lock size.";
static const char RADSS_TOO_MANY_CHANNELS[] = "Too many output channels.";

static unsigned int RadSS_defaultMode = 0;

static int RADSS_SDLOpenDriver(_RadSoundSystem* pSystem, unsigned int uLatencyMilliseconds, unsigned int uRate,
                               unsigned int uChannels, unsigned int uLockBytes, unsigned int uMode);

static void InitRouting(int iMode, int iChannels, short* pRouting)
{
    if (iChannels == 1)
    {
        if (iMode == RADSS_MODE_STEREO || iMode == RADSS_MODE_MATRIX)
        {
            pRouting[0 * RADSS_ROUTING_STRIDE] = RADSS_UNITY_VOLUME;
            pRouting[1 * RADSS_ROUTING_STRIDE] = RADSS_UNITY_VOLUME;
            pRouting[5 * RADSS_ROUTING_STRIDE] = RADSS_UNITY_VOLUME;
            pRouting[3 * RADSS_ROUTING_STRIDE] = RADSS_UNITY_VOLUME;
            pRouting[2 * RADSS_ROUTING_STRIDE] = RADSS_UNITY_VOLUME;
            pRouting[4 * RADSS_ROUTING_STRIDE] = RADSS_UNITY_VOLUME;
        }

        if (iMode == RADSS_MODE_MATRIX || iMode == RADSS_MODE_SURROUND)
        {
            pRouting[6 * RADSS_ROUTING_STRIDE] = RADSS_UNITY_VOLUME;
            pRouting[7 * RADSS_ROUTING_STRIDE] = RADSS_UNITY_VOLUME;
        }

        return;
    }

    if (iMode == RADSS_MODE_MATRIX && iChannels == 4)
    {
        pRouting[1 * RADSS_ROUTING_STRIDE + 1] = RADSS_UNITY_VOLUME;
        pRouting[6 * RADSS_ROUTING_STRIDE + 2] = RADSS_UNITY_VOLUME;
        pRouting[0 * RADSS_ROUTING_STRIDE + 0] = RADSS_UNITY_VOLUME;
        pRouting[7 * RADSS_ROUTING_STRIDE + 3] = RADSS_UNITY_VOLUME;
        return;
    }

    if (iMode == RADSS_MODE_MATRIX || iMode == RADSS_MODE_STEREO)
    {
        pRouting[1 * RADSS_ROUTING_STRIDE + 1] = RADSS_UNITY_VOLUME;
        pRouting[5 * RADSS_ROUTING_STRIDE + 5] = RADSS_UNITY_VOLUME;
        pRouting[3 * RADSS_ROUTING_STRIDE + 3] = RADSS_UNITY_VOLUME;
        pRouting[4 * RADSS_ROUTING_STRIDE + 4] = RADSS_UNITY_VOLUME;
        pRouting[0 * RADSS_ROUTING_STRIDE + 0] = RADSS_UNITY_VOLUME;
        pRouting[2 * RADSS_ROUTING_STRIDE + 2] = RADSS_UNITY_VOLUME;
    }

    if (iMode == RADSS_MODE_SURROUND && iChannels == 2)
    {
        pRouting[7 * RADSS_ROUTING_STRIDE + 1] = RADSS_UNITY_VOLUME;
        pRouting[6 * RADSS_ROUTING_STRIDE + 0] = RADSS_UNITY_VOLUME;
        return;
    }

    if (iChannels == 7)
    {
        pRouting[6 * RADSS_ROUTING_STRIDE + 6] = RADSS_UNITY_VOLUME;
        pRouting[7 * RADSS_ROUTING_STRIDE + 6] = RADSS_UNITY_VOLUME;
    }
    else if (iChannels == 8)
    {
        pRouting[6 * RADSS_ROUTING_STRIDE + 6] = RADSS_UNITY_VOLUME;
        pRouting[7 * RADSS_ROUTING_STRIDE + 7] = RADSS_UNITY_VOLUME;
    }
}

static float ClampOutput(float fSample)
{
    if (fSample > RADSS_OUTPUT_LIMIT)
    {
        return RADSS_OUTPUT_LIMIT;
    }

    if (fSample < -RADSS_OUTPUT_LIMIT)
    {
        return -RADSS_OUTPUT_LIMIT;
    }

    return fSample;
}

static unsigned int FrameBytes(_RadSoundSystem* pSystem)
{
    return pSystem->ucChannels * sizeof(short);
}

static void CloseStream(_RadSoundSystem* pSystem)
{
    if ((pSystem->priv.ucDeviceOpen & RADSS_DEVICE_STREAM) != 0)
    {
        SDL_DestroyAudioStream(pSystem->priv.pStream);
        pSystem->priv.pStream = NULL;
        pSystem->priv.ucDeviceOpen = pSystem->priv.ucDeviceOpen & ~RADSS_DEVICE_STREAM;
    }
}

static void OpenStream(_RadSoundSystem* pSystem)
{
    SDL_AudioSpec spec;

    if ((pSystem->priv.ucDeviceOpen & RADSS_DEVICE_SUBSYSTEM) == 0)
    {
        return;
    }

    spec.format = SDL_AUDIO_F32;
    spec.channels = RADSS_OUTPUT_CHANNELS;
    spec.freq = pSystem->usRate;

    pSystem->priv.pStream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
    if (pSystem->priv.pStream != NULL)
    {
        pSystem->priv.ucDeviceOpen = pSystem->priv.ucDeviceOpen | RADSS_DEVICE_STREAM;
    }
}

static unsigned int QueuedFrames(_RadSoundSystem* pSystem)
{
    unsigned long long uNow;
    unsigned long long uConsumed;
    int iQueued;

    if ((pSystem->priv.ucDeviceOpen & RADSS_DEVICE_STREAM) != 0)
    {
        iQueued = SDL_GetAudioStreamQueued(pSystem->priv.pStream);
        if (iQueued < 0)
        {
            return 0;
        }

        return static_cast<unsigned int>(iQueued) / (RADSS_OUTPUT_CHANNELS * sizeof(float));
    }

    uNow = rrGetTime();

    if ((pSystem->priv.ucFlags & RADSS_FLAG_PLAYING) != 0 && pSystem->ucPaused == 0)
    {
        uConsumed = (uNow - pSystem->priv.uStartTime) * pSystem->usRate / RADSS_NANOSECONDS_PER_SECOND;
        if (uConsumed != 0)
        {
            if (uConsumed >= pSystem->priv.uFramesWritten)
            {
                pSystem->priv.uFramesWritten = 0;
            }
            else
            {
                pSystem->priv.uFramesWritten = pSystem->priv.uFramesWritten - uConsumed;
            }

            pSystem->priv.uStartTime = pSystem->priv.uStartTime
                                       + uConsumed * RADSS_NANOSECONDS_PER_SECOND / pSystem->usRate;
        }
    }
    else
    {
        pSystem->priv.uStartTime = uNow;
    }

    return static_cast<unsigned int>(pSystem->priv.uFramesWritten);
}

static void QueueFrames(_RadSoundSystem* pSystem, const float* pFrames, unsigned int uFrames)
{
    if (uFrames == 0)
    {
        return;
    }

    if ((pSystem->priv.ucDeviceOpen & RADSS_DEVICE_STREAM) != 0)
    {
        SDL_PutAudioStreamData(pSystem->priv.pStream, pFrames,
                               static_cast<int>(uFrames * RADSS_OUTPUT_CHANNELS * sizeof(float)));
        return;
    }

    if (pSystem->priv.uFramesWritten == 0)
    {
        pSystem->priv.uStartTime = rrGetTime();
    }

    pSystem->priv.uFramesWritten = pSystem->priv.uFramesWritten + uFrames;
}

static void QueueSilence(_RadSoundSystem* pSystem, unsigned int uFrames)
{
    unsigned int uMixFrames;
    unsigned int uChunk;

    uMixFrames = pSystem->priv.uStagingBytes / FrameBytes(pSystem);
    if (uMixFrames == 0)
    {
        return;
    }

    memset(pSystem->priv.pMix, 0, uMixFrames * RADSS_OUTPUT_CHANNELS * sizeof(float));

    while (uFrames != 0)
    {
        uChunk = uFrames;
        if (uChunk > uMixFrames)
        {
            uChunk = uMixFrames;
        }

        QueueFrames(pSystem, pSystem->priv.pMix, uChunk);
        uFrames = uFrames - uChunk;
    }
}

static void StartPlayback(_RadSoundSystem* pSystem)
{
    if ((pSystem->priv.ucDeviceOpen & RADSS_DEVICE_STREAM) != 0)
    {
        SDL_ResumeAudioStreamDevice(pSystem->priv.pStream);
    }
    else
    {
        pSystem->priv.uStartTime = rrGetTime();
    }
}

static void StopPlayback(_RadSoundSystem* pSystem)
{
    if ((pSystem->priv.ucDeviceOpen & RADSS_DEVICE_STREAM) != 0)
    {
        SDL_PauseAudioStreamDevice(pSystem->priv.pStream);
    }
}

static int RADSS_SDLCloseDriver(_RadSoundSystem* pSystem)
{
    CloseStream(pSystem);

    if ((pSystem->priv.ucDeviceOpen & RADSS_DEVICE_SUBSYSTEM) != 0)
    {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        pSystem->priv.ucDeviceOpen = pSystem->priv.ucDeviceOpen & ~RADSS_DEVICE_SUBSYSTEM;
    }

    pSystem->priv.ucFlags = pSystem->priv.ucFlags & ~RADSS_FLAG_PLAYING;
    return 1;
}

static int RADSS_SDLGetReadyBytes(_RadSoundSystem* pSystem)
{
    unsigned int uQueued;
    unsigned int uFree;
    unsigned int uReady;

    if (pSystem->ucEnabled == 0 || pSystem->ucPaused != 0)
    {
        return 0;
    }

    for (;;)
    {
        uQueued = QueuedFrames(pSystem);
        uFree = 0;

        if (uQueued < pSystem->priv.uBufferFrames)
        {
            uFree = pSystem->priv.uBufferFrames - uQueued;
        }

        uReady = 0;

        if (uFree >= pSystem->priv.uReserveFrames)
        {
            uReady = uFree - pSystem->priv.uReserveFrames;
        }

        if (uReady < pSystem->priv.uReadyLimitFrames)
        {
            return static_cast<int>(uReady * FrameBytes(pSystem));
        }

        if ((pSystem->priv.ucFlags & RADSS_FLAG_PLAYING) != 0)
        {
            pSystem->usUnderruns = pSystem->usUnderruns + 1;
        }

        QueueSilence(pSystem, pSystem->priv.uChunkFrames);
    }
}

static void RADSS_SDLSetMem(_RadSoundSystem* pSystem, void* pMemory)
{
    unsigned int uMixFrames;

    pSystem->pMemory = pMemory;
    pSystem->priv.pStaging = pMemory;

    uMixFrames = pSystem->priv.uStagingBytes / FrameBytes(pSystem);
    pSystem->priv.pMix = reinterpret_cast<float*>(static_cast<unsigned char*>(pMemory) + pSystem->priv.uStagingBytes);
    pSystem->priv.pRouting = reinterpret_cast<short*>(reinterpret_cast<unsigned char*>(pSystem->priv.pMix)
                                                      + uMixFrames * RADSS_OUTPUT_CHANNELS * sizeof(float));

    memset(pSystem->priv.pRouting, 0, RADSS_PC_ROUTING_BYTES);
    InitRouting(pSystem->priv.ucFlags & RADSS_FLAG_MODE_MASK, pSystem->ucChannels, pSystem->priv.pRouting);

    if ((pSystem->priv.ucDeviceOpen & RADSS_DEVICE_STREAM) != 0)
    {
        SDL_ClearAudioStream(pSystem->priv.pStream);
    }

    pSystem->priv.uFramesWritten = 0;
    QueueSilence(pSystem, pSystem->priv.uChunkFrames);
}

static void RADSS_SDLChannelRoute(_RadSoundSystem* pSystem, short* pRouting, int iCount)
{
    int iChannels;
    int iVoice;
    int iRow;

    iChannels = pSystem->ucChannels;

    if (iCount == RADSS_PC_ROUTING_ENTRIES)
    {
        if (pRouting != pSystem->priv.pRouting)
        {
            memcpy(pSystem->priv.pRouting, pRouting, RADSS_PC_ROUTING_BYTES);
        }
    }
    else
    {
        if (iCount != iChannels * RADSS_ROUTING_STRIDE)
        {
            return;
        }

        memset(pSystem->priv.pRouting, 0, RADSS_PC_ROUTING_BYTES);

        for (iVoice = 0; iVoice < RADSS_PC_CHANNEL_LIMIT && iVoice < iChannels; iVoice++)
        {
            for (iRow = 0; iRow < RADSS_ROUTING_STRIDE; iRow++)
            {
                pSystem->priv.pRouting[iRow * RADSS_ROUTING_STRIDE + iVoice] = pRouting[iRow * iChannels + iVoice];
            }
        }
    }

    if ((pSystem->priv.ucFlags & RADSS_FLAG_ROUTED) == 0)
    {
        pSystem->priv.sVolumeSecond = RADSS_UNITY_VOLUME;
        pSystem->priv.sVolumeFirst = RADSS_UNITY_VOLUME;
    }

    pSystem->priv.ucFlags = pSystem->priv.ucFlags | RADSS_FLAG_ROUTED;
}

static void RADSS_SDLVolume(_RadSoundSystem* pSystem, float fFirst, float fSecond)
{
    pSystem->priv.sVolumeSecond = (short)(fSecond * RADSS_VOLUME_SCALE);
    pSystem->priv.sVolumeFirst = (short)(fFirst * RADSS_VOLUME_SCALE);

    if ((pSystem->priv.ucFlags & RADSS_FLAG_ROUTED) == 0)
    {
        memset(pSystem->priv.pRouting, 0, RADSS_PC_ROUTING_BYTES);
        InitRouting(pSystem->priv.ucFlags & RADSS_FLAG_MODE_MASK, pSystem->ucChannels, pSystem->priv.pRouting);
    }

    pSystem->priv.ucFlags = pSystem->priv.ucFlags | RADSS_FLAG_ROUTED;
}

static int RADSS_SDLSetOnOff(_RadSoundSystem* pSystem, int iOn)
{
    if (iOn == 0)
    {
        CloseStream(pSystem);
        pSystem->priv.ucFlags = pSystem->priv.ucFlags & ~RADSS_FLAG_PLAYING;
        pSystem->priv.uFramesWritten = 0;
        return 1;
    }

    if ((pSystem->priv.ucDeviceOpen & RADSS_DEVICE_STREAM) == 0)
    {
        OpenStream(pSystem);
        pSystem->priv.uFramesWritten = 0;
        QueueSilence(pSystem, pSystem->priv.uChunkFrames);
    }

    return 1;
}

static int RADSS_SDLSetPaused(_RadSoundSystem* pSystem, int iPaused)
{
    if ((unsigned int)iPaused != pSystem->ucPaused)
    {
        if ((pSystem->priv.ucFlags & RADSS_FLAG_PLAYING) != 0)
        {
            if (iPaused == 0)
            {
                StartPlayback(pSystem);
            }
            else
            {
                StopPlayback(pSystem);
            }
        }

        pSystem->ucPaused = (unsigned char)iPaused;

        if (iPaused == 0)
        {
            pSystem->priv.uStartTime = rrGetTime();
        }
    }

    return 1;
}

static int RADSS_SDLLock(_RadSoundSystem* pSystem, void** ppBuffer, unsigned int uBytes)
{
    if (uBytes > pSystem->priv.uStagingBytes)
    {
        pSystem->pError = RADSS_INVALID_LOCK_SIZE;
        return 0;
    }

    *ppBuffer = pSystem->priv.pStaging;
    pSystem->priv.uLockBytes = uBytes;
    return 1;
}

static int RADSS_SDLUnlock(_RadSoundSystem* pSystem)
{
    float afGains[RADSS_TV_ROW_COUNT][RADSS_PC_CHANNEL_LIMIT];
    float afRows[RADSS_TV_ROW_COUNT];
    const short* pSource;
    float* pOutput;
    float fFirst;
    float fSecond;
    float fCenter;
    float fScale;
    unsigned int uChannels;
    unsigned int uFrames;
    unsigned int uFrame;
    unsigned int uChannel;
    int iRow;

    uChannels = pSystem->ucChannels;
    uFrames = pSystem->priv.uLockBytes / FrameBytes(pSystem);

    fFirst = 1.0f;
    fSecond = 1.0f;

    if ((pSystem->priv.ucFlags & RADSS_FLAG_ROUTED) != 0)
    {
        fFirst = (float)pSystem->priv.sVolumeFirst / RADSS_VOLUME_SCALE;
        fSecond = (float)pSystem->priv.sVolumeSecond / RADSS_VOLUME_SCALE;
    }

    fCenter = fSecond;

    if (fSecond < fFirst)
    {
        fCenter = fFirst;
    }

    for (iRow = 0; iRow < RADSS_TV_ROW_COUNT; iRow++)
    {
        switch (iRow)
        {
        case RADSS_ROW_FRONT_LEFT:
        case RADSS_ROW_BACK_LEFT:
            fScale = fFirst;
            break;
        case RADSS_ROW_FRONT_RIGHT:
        case RADSS_ROW_BACK_RIGHT:
            fScale = fSecond;
            break;
        default:
            fScale = fCenter;
            break;
        }

        for (uChannel = 0; uChannel < uChannels; uChannel++)
        {
            afGains[iRow][uChannel] = (float)pSystem->priv.pRouting[iRow * RADSS_ROUTING_STRIDE + uChannel]
                                      / RADSS_VOLUME_SCALE * fScale * RADSS_SAMPLE_SCALE;
        }
    }

    pSource = static_cast<const short*>(pSystem->priv.pStaging);
    pOutput = pSystem->priv.pMix;

    for (uFrame = 0; uFrame < uFrames; uFrame++)
    {
        for (iRow = 0; iRow < RADSS_TV_ROW_COUNT; iRow++)
        {
            afRows[iRow] = 0.0f;

            for (uChannel = 0; uChannel < uChannels; uChannel++)
            {
                afRows[iRow] = afRows[iRow] + afGains[iRow][uChannel] * (float)pSource[uChannel];
            }
        }

        pOutput[0] = ClampOutput(afRows[RADSS_ROW_FRONT_LEFT] + RADSS_DOWNMIX_LEVEL * afRows[RADSS_ROW_CENTER]
                                 + RADSS_DOWNMIX_LEVEL * afRows[RADSS_ROW_BACK_LEFT]);
        pOutput[1] = ClampOutput(afRows[RADSS_ROW_FRONT_RIGHT] + RADSS_DOWNMIX_LEVEL * afRows[RADSS_ROW_CENTER]
                                 + RADSS_DOWNMIX_LEVEL * afRows[RADSS_ROW_BACK_RIGHT]);

        pSource = pSource + uChannels;
        pOutput = pOutput + RADSS_OUTPUT_CHANNELS;
    }

    QueueFrames(pSystem, pSystem->priv.pMix, uFrames);

    if ((pSystem->priv.ucFlags & RADSS_FLAG_PLAYING) == 0 && pSystem->ucPaused == 0 && pSystem->ucEnabled != 0)
    {
        StartPlayback(pSystem);
        pSystem->priv.ucFlags = pSystem->priv.ucFlags | RADSS_FLAG_PLAYING;
    }

    return 1;
}

static unsigned int RADSS_SDLGetProperty(_RadSoundSystem*, RADSS_PROPERTY, unsigned int*)
{
    return 0;
}

static int RADSS_SDLOpenDriver(_RadSoundSystem* pSystem, unsigned int uLatencyMilliseconds, unsigned int uRate,
                               unsigned int uChannels, unsigned int uLockBytes, unsigned int uMode)
{
    unsigned int uChunks;
    unsigned int uChunkMilliseconds;
    unsigned int uSamples;
    unsigned int uChunkSamples;
    unsigned int uBufferBytes;
    unsigned int uMixFrames;

    memset(pSystem, 0, sizeof(_RadSoundSystem));

    if (uMode != 0)
    {
        RadSS_defaultMode = uMode;
    }

    pSystem->pClose = RADSS_SDLCloseDriver;
    pSystem->pGetReadyBytes = RADSS_SDLGetReadyBytes;
    pSystem->pSetMemory = RADSS_SDLSetMem;
    pSystem->pSetOnOff = RADSS_SDLSetOnOff;
    pSystem->pSetPaused = RADSS_SDLSetPaused;
    pSystem->pLock = RADSS_SDLLock;
    pSystem->pUnlock = RADSS_SDLUnlock;
    pSystem->pGetProperty = RADSS_SDLGetProperty;
    pSystem->pChannelRoute = RADSS_SDLChannelRoute;
    pSystem->ucChannels = (unsigned char)uChannels;
    pSystem->usUnderruns = 0;
    pSystem->usRate = (unsigned short)uRate;
    pSystem->ucEnabled = 1;
    pSystem->ucReserved34 = 0;
    pSystem->ucPaused = 1;
    pSystem->pVolume = RADSS_SDLVolume;

    if (uChannels == 0 || uChannels > RADSS_PC_CHANNEL_LIMIT)
    {
        pSystem->pError = RADSS_TOO_MANY_CHANNELS;
        return 0;
    }

    uChunks = uLatencyMilliseconds / RADSS_MAXIMUM_CHUNK_MILLISECONDS;
    pSystem->priv.usLatency = (unsigned short)uLatencyMilliseconds;

    if (uChunks * RADSS_MAXIMUM_CHUNK_MILLISECONDS < uLatencyMilliseconds)
    {
        uChunks = uChunks + 1;
    }

    if (uChunks < RADSS_MINIMUM_CHUNKS)
    {
        uChunks = RADSS_MINIMUM_CHUNKS;
    }

    uChunkMilliseconds = uLatencyMilliseconds / uChunks;
    uSamples = (uRate * uChunks) / RADSS_MILLISECONDS_PER_SECOND + 15;
    uChunkSamples = (uSamples * 2) & RADSS_BYTE_ALIGNMENT_MASK;

    if (uChunkMilliseconds * uChunks < uLatencyMilliseconds)
    {
        uChunkMilliseconds = uChunkMilliseconds + 1;
    }

    if (uChunkMilliseconds > RADSS_MAXIMUM_CHUNK_MILLISECONDS)
    {
        uChunkMilliseconds = RADSS_MAXIMUM_CHUNK_MILLISECONDS;
    }

    uBufferBytes = uChunkMilliseconds * uChunkSamples;
    pSystem->priv.uBufferFrames = uBufferBytes / sizeof(short);
    pSystem->priv.uChunkFrames = uChunkSamples;
    pSystem->priv.uReadyLimitFrames = (uBufferBytes * RADSS_READY_LIMIT_NUMERATOR) / RADSS_READY_LIMIT_DENOMINATOR
                                      / sizeof(short);
    pSystem->priv.uReserveFrames = RADSS_MINIMUM_READY_BYTES / sizeof(short);

    pSystem->priv.uStagingBytes = (uLockBytes + RADSS_STAGING_ALIGNMENT - 1) & RADSS_SAMPLE_ALIGNMENT_MASK;
    uMixFrames = pSystem->priv.uStagingBytes / (uChannels * sizeof(short));
    pSystem->uMemoryBytes = pSystem->priv.uStagingBytes + uMixFrames * RADSS_OUTPUT_CHANNELS * sizeof(float)
                            + RADSS_PC_ROUTING_BYTES;
    pSystem->priv.ucFlags = (pSystem->priv.ucFlags & ~RADSS_FLAG_MODE_MASK) | (unsigned char)RadSS_defaultMode;

    if (SDL_InitSubSystem(SDL_INIT_AUDIO))
    {
        pSystem->priv.ucDeviceOpen = RADSS_DEVICE_SUBSYSTEM;
        OpenStream(pSystem);
    }

    return 1;
}

extern "C" RADSS_OPEN_DRIVER_CALLBACK RADSS_SDLInstallDriver(int iDevice)
{
    RadSS_defaultMode = iDevice;
    return RADSS_SDLOpenDriver;
}
