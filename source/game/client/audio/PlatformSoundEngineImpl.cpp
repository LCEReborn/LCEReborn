#include "game/client/audio/PlatformSoundEngineImpl.hpp"

#include <stdio.h>
#include <string.h>

#include "platform/C4JThreadImpl.hpp"

#include "vendor/MilesRib.hpp"

#if LCE_PLATFORM_WIIU
extern "C" void AXInit();
extern "C" void MIXInit();
#endif

extern "C" int BinkADec_RIB_Main(unsigned int uiProvider, int iUpDown,
                                 RIB_ALLOC_PROVIDER_HANDLE_CALLBACK pAllocProviderHandle,
                                 RIB_REGISTER_INTERFACE_CALLBACK pRegisterInterface,
                                 RIB_UNREGISTER_INTERFACE_CALLBACK pUnregisterInterface);

#if LCE_PLATFORM_WIIU
#include "platform/wiiu/WiiUMilesFileSystem.hpp"
#elif LCE_PLATFORM_PC
#include "platform/pc/audio/MilesPcFileSystem.hpp"
#include "vendor/MilesRadSS.hpp"
#endif
#include "game/client/audio/FalloffGraph.hpp"
#include "vendor/MilesApi.hpp"
#include "vendor/MilesEventExec.hpp"

extern "C" void AIL_startup();
extern "C" void AIL_shutdown();
extern "C" const char* AIL_last_error();
extern "C" int AIL_set_sample_is_3D(_SAMPLE* pSample, int iIs3D);
extern "C" unsigned int AIL_enqueue_event_start();
extern "C" unsigned int AIL_enqueue_event_buffer(unsigned int* puiQueue, void* pData, int iDataSize, int iFlags);
extern "C" void AIL_enqueue_event_end_named(unsigned int uiQueue, const char* pEventName);

extern "C" void MilesSetEventErrorCallback(void (*pCallback)(long long, const char*));

static char szSoundPath[] = "WiiU/Sound/";

#if LCE_PLATFORM_WIIU
#include <coreinit/memdefaultheap.h>

extern "C" void* platform_alloc(unsigned int uiSize)
{
    return MEMAllocFromDefaultHeapEx(uiSize, 0x40);
}

extern "C" void platform_free(void* pBlock)
{
    MEMFreeToDefaultHeap(pBlock);
}
#elif LCE_PLATFORM_PC
#include <stdlib.h>

#if defined(_WIN32)
#include <malloc.h>
#endif

static const unsigned int PLATFORM_ALLOC_ALIGNMENT = 0x40;

extern "C" void* platform_alloc(unsigned int uiSize)
{
#if defined(_WIN32)
    return _aligned_malloc(uiSize, PLATFORM_ALLOC_ALIGNMENT);
#else
    void* pBlock;

    if (posix_memalign(&pBlock, PLATFORM_ALLOC_ALIGNMENT, uiSize) != 0)
    {
        return NULL;
    }

    return pBlock;
#endif
}

extern "C" void platform_free(void* pBlock)
{
#if defined(_WIN32)
    _aligned_free(pBlock);
#else
    free(pBlock);
#endif
}
#endif

PlatformSoundEngineImpl::PlatformSoundEngineImpl()
{
    m_pStream = NULL;
    m_pStreamDRC = NULL;
    m_pDigitalDriver = NULL;
}

PlatformSoundEngineImpl::~PlatformSoundEngineImpl()
{
}

#if LCE_PLATFORM_WIIU
int PlatformSoundEngineImpl::initAX(void* pArg)
{
    AXInit();
    return 0;
}
#endif

void PlatformSoundEngineImpl::ErrorCallback(long long iErrorCode, const char* pErrorText)
{
    AIL_last_error();
}

float PlatformSoundEngineImpl::custom_falloff_function(_SAMPLE* pSample, float fDistance, float fRolloffFactor,
                                                       float fMinDistance, float fMaxDistance)
{
    if (fMaxDistance == 10000.0f)
    {
        return 1.0f;
    }

    float fVolume = 1.0f - fDistance / fMaxDistance;

    if (fVolume < 0.0f)
    {
        fVolume = 0.0f;
    }
    else if (fVolume > 1.0f)
    {
        fVolume = 1.0f;
    }

    return fVolume;
}

void PlatformSoundEngineImpl::init(char* pSoundBankPath, SoundBankLoadedCallback pSoundBankLoaded)
{
#if LCE_PLATFORM_WIIU
    C4JThreadImpl* pAXInitThread;
#endif
    RADSS_OPEN_DRIVER_CALLBACK pProvider;
    char szSoundBankFile[256];

    RIB_load_static_provider_library(BinkADec_RIB_Main, "BinkADec");

#if LCE_PLATFORM_WIIU
    pAXInitThread = new C4JThreadImpl(initAX, NULL, "AXInitThread", 0);
    pAXInitThread->SetProcessor(2);
    pAXInitThread->Run();
    pAXInitThread->WaitForCompletion(-1);
    delete pAXInitThread;

    MIXInit();
#endif

    AIL_set_file_callbacks(file_open_callback, file_close_callback, file_seek_callback, file_read_callback);
    AIL_startup();

#if LCE_PLATFORM_WIIU
    pProvider = AIL_output_ax(3);
#elif LCE_PLATFORM_PC
    pProvider = RADSS_SDLInstallDriver(3);
#endif

    m_pDigitalDriver = AIL_open_generic_digital_driver(32000, 16, 128, 0, pProvider);

    if (m_pDigitalDriver == NULL)
    {
        AIL_shutdown();
        return;
    }

    MilesSetEventErrorCallback(ErrorCallback);
    AIL_set_3D_rolloff_factor(m_pDigitalDriver, 1.0f);

    if (MilesStartupEventSystem(m_pDigitalDriver, 0x20000, 0, 0x40000) == 0)
    {
        AIL_close_digital_driver(m_pDigitalDriver);
#if LCE_PLATFORM_PC
        m_pDigitalDriver = NULL;
#endif
        AIL_shutdown();
        return;
    }

    sprintf(szSoundBankFile, "%s", szSoundPath);
    strcat(szSoundBankFile, "Minecraft.msscmp");

    m_pSoundBank = MilesAddSoundBank(szSoundBankFile, 0);

    if (m_pSoundBank == NULL)
    {
        AIL_last_error();
        AIL_close_digital_driver(m_pDigitalDriver);
#if LCE_PLATFORM_PC
        m_pDigitalDriver = NULL;
#endif
        AIL_shutdown();
        return;
    }

    MilesEnqueueEventByName("Minecraft/CacheSounds");
}

void PlatformSoundEngineImpl::tick(void* pUserData, float fListenerX, float fListenerY, float fListenerZ,
                                   float fListenerForwardX, float fListenerForwardY, float fListenerForwardZ,
                                   ConfigureSoundCallback pConfigureSound, UpdateSoundCallback pUpdateSound,
                                   RouteToDRCCallback pRouteToDRC, bool* pbSystemMusicPlaying)
{
    SoundInstanceInfo info;
    unsigned int uiNext;

    *pbSystemMusicPlaying = false;

    AIL_set_listener_3D_position(m_pDigitalDriver, fListenerX, fListenerY, fListenerZ);
    AIL_set_listener_3D_orientation(m_pDigitalDriver, fListenerForwardX, fListenerForwardY, fListenerForwardZ, 0.0f,
                                    1.0f, 0.0f);

    MilesBeginEventQueueProcessing();

    uiNext = 0xFFFFFFFF;

    while (MilesEnumerateSoundInstances(NULL, &uiNext, 0, NULL, 0, 0, &info) != 0)
    {
        info.m_sample->outputToDRC = pRouteToDRC(pUserData, const_cast<void*>(info.m_selectionData));

        switch (info.m_state)
        {
        case 1:
        {
            bool bIs3D;
            float fMinDistance;
            float fPlaybackRate;
            float fX;
            float fY;
            float fZ;
            float fVolume;

            pConfigureSound(pUserData, const_cast<void*>(info.m_selectionData), &bIs3D, &fMinDistance, &fPlaybackRate,
                            &fX, &fY, &fZ, &fVolume);

            AIL_register_falloff_function_callback(info.m_sample, custom_falloff_function);

            if (fPlaybackRate != 0.0f)
            {
                AIL_set_sample_playback_rate_factor(info.m_sample, fPlaybackRate);
            }

            if (bIs3D)
            {
                AIL_set_sample_is_3D(info.m_sample, 1);
                AIL_set_sample_3D_distances(info.m_sample, fMinDistance, 1.0f, 0);
                AIL_set_sample_3D_position(info.m_sample, fX, fY, fZ);
            }
            else
            {
                AIL_set_sample_is_3D(info.m_sample, 0);
            }

            AIL_set_sample_volume_levels(info.m_sample, fVolume, fVolume);

            pUpdateSound(pUserData, const_cast<void*>(info.m_selectionData), &bIs3D, 0, &fX, &fY, &fZ, &fVolume);
            break;
        }
        case 4:
        {
            bool bIs3D;
            float fX;
            float fY;
            float fZ;
            float fVolume;

            pUpdateSound(pUserData, const_cast<void*>(info.m_selectionData), &bIs3D, 2, &fX, &fY, &fZ, &fVolume);
            break;
        }
        default:
        {
            bool bIs3D;
            float fX;
            float fY;
            float fZ;
            float fVolume;

            pUpdateSound(pUserData, const_cast<void*>(info.m_selectionData), &bIs3D, 1, &fX, &fY, &fZ, &fVolume);

            if (bIs3D)
            {
                AIL_set_sample_3D_position(info.m_sample, fX, fY, fZ);
            }

            AIL_set_sample_volume_levels(info.m_sample, fVolume, fVolume);
            break;
        }
        }
    }

    MilesCompleteEventQueueProcessing();
}

void PlatformSoundEngineImpl::startSound(void* pSoundData, int iSoundDataSize, char* pEventName)
{
    unsigned int uiQueue;

    uiQueue = AIL_enqueue_event_start();
    AIL_enqueue_event_buffer(&uiQueue, pSoundData, iSoundDataSize, 0);
    AIL_enqueue_event_end_named(uiQueue, pEventName);
}

bool PlatformSoundEngineImpl::openStream(char* pFileName)
{
    m_pStreamDRC = AIL_open_stream(m_pDigitalDriver, pFileName, 0);

    if (m_pStreamDRC == NULL)
    {
        return false;
    }

    m_pStream = AIL_open_stream(m_pDigitalDriver, pFileName, 0);

    if (m_pStream == NULL)
    {
        return false;
    }

    return true;
}

void PlatformSoundEngineImpl::startStream(void* pUserData, ConfigureSoundCallback pConfigureSound)
{
    _SAMPLE* pSample;
    _SAMPLE* pSampleDRC;
    bool bIs3D;
    float fMinDistance;
    float fPlaybackRate;
    float fX;
    float fY;
    float fZ;
    float fVolume;

    pConfigureSound(pUserData, NULL, &bIs3D, &fMinDistance, &fPlaybackRate, &fX, &fY, &fZ, &fVolume);

    pSample = AIL_stream_sample_handle(m_pStream);
    pSampleDRC = AIL_stream_sample_handle(m_pStreamDRC);
    pSampleDRC->outputToDRC = true;

    AIL_register_falloff_function_callback(pSample, custom_falloff_function);

    if (bIs3D)
    {
        AIL_set_sample_is_3D(pSample, 1);
        AIL_set_sample_3D_distances(pSample, fMinDistance, 1.0f, 0);
        AIL_set_sample_3D_position(pSample, fX, fY, fZ);
        AIL_set_sample_is_3D(pSampleDRC, 1);
        AIL_set_sample_3D_distances(pSampleDRC, fMinDistance, 1.0f, 0);
        AIL_set_sample_3D_position(pSampleDRC, fX, fY, fZ);
    }
    else
    {
        AIL_set_sample_is_3D(pSample, 0);
        AIL_set_sample_is_3D(pSampleDRC, 0);
    }

    AIL_set_sample_playback_rate_factor(pSample, fPlaybackRate);
    AIL_set_sample_volume_levels(pSample, fVolume, fVolume);
    AIL_start_stream(m_pStream);

    AIL_set_sample_playback_rate_factor(pSampleDRC, fPlaybackRate);
    AIL_set_sample_volume_levels(pSampleDRC, fVolume, fVolume);
    AIL_start_stream(m_pStreamDRC);
}

void PlatformSoundEngineImpl::setStreamVol(float fVolume)
{
    _SAMPLE* pSample;
    _SAMPLE* pSampleDRC;

    pSample = AIL_stream_sample_handle(m_pStream);
    AIL_set_sample_volume_levels(pSample, fVolume, fVolume);

    pSampleDRC = AIL_stream_sample_handle(m_pStreamDRC);
    pSampleDRC->outputToDRC = true;
    AIL_set_sample_volume_levels(pSampleDRC, fVolume, fVolume);
}

void PlatformSoundEngineImpl::stopStream()
{
    AIL_pause_stream(m_pStream, 1);
    AIL_close_stream(m_pStream);
    m_pStream = NULL;

    AIL_pause_stream(m_pStreamDRC, 1);
    AIL_close_stream(m_pStreamDRC);
    m_pStreamDRC = NULL;
}

void PlatformSoundEngineImpl::updateStreamPos(void* pUserData, UpdateSoundCallback pUpdateSound)
{
    bool bIs3D;
    float fX;
    float fY;
    float fZ;
    float fVolume;

    pUpdateSound(pUserData, NULL, &bIs3D, 1, &fX, &fY, &fZ, &fVolume);

    if (bIs3D)
    {
        _SAMPLE* pSample = AIL_stream_sample_handle(m_pStream);
        AIL_set_sample_3D_position(pSample, fX, fY, fZ);
    }
}

bool PlatformSoundEngineImpl::isStreamComplete()
{
    if (m_pStream != NULL && AIL_stream_status(m_pStream) == 2)
    {
        AIL_close_stream(m_pStream);
        m_pStream = NULL;
        AIL_close_stream(m_pStreamDRC);
        m_pStreamDRC = NULL;
        return true;
    }

    return false;
}

void PlatformSoundEngineImpl::shutdown()
{
    AIL_shutdown();
}
