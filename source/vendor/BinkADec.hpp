#ifndef SOURCE_VENDOR_BINKADEC_HPP
#define SOURCE_VENDOR_BINKADEC_HPP

#include "platform/Platform.hpp"
#include "vendor/MilesRib.hpp"

#if LCE_PLATFORM_PC
#include <stdint.h>
#endif

struct BINKAUDIODECOMP;

typedef void* (*BCF_ALLOC_CALLBACK)(unsigned int uSize, unsigned int uUser, const char* pFile, unsigned int uLine);
typedef void (*BCF_FREE_CALLBACK)(void* pBlock, unsigned int uUser, const char* pFile, unsigned int uLine);
#if LCE_PLATFORM_WIIU
typedef int (*BCF_FETCH_CALLBACK)(unsigned int uUser, void* pDestination, int iBytes, int iOffset);
#elif LCE_PLATFORM_PC
typedef int (*BCF_FETCH_CALLBACK)(uintptr_t uUser, void* pDestination, int iBytes, int iOffset);
#endif

static const unsigned int BCF_MAX_STREAMS = 4;

struct BcfHeader
{
    unsigned int uMagic;
    unsigned char ucVersion;
    unsigned char ucChannels;
    unsigned short usSampleRate;
    unsigned int uTotalSamples;
    unsigned int uMaxFrameBytes;
    unsigned int uFileBytes;
    unsigned int uSeekCount;
};

struct BcfStream
{
    unsigned int uReserved00;
    unsigned int uReserved04;
    unsigned char* pOutput;
    unsigned int uReserved0C;
    unsigned int uOutputBytes;
    unsigned int uOutputPosition;
#if LCE_PLATFORM_WIIU
    unsigned int uUser;
#elif LCE_PLATFORM_PC
    uintptr_t uUser;
#endif
    BCF_FETCH_CALLBACK pFetch;
    BCF_FREE_CALLBACK pFree;
    unsigned int uFileBytes;
    unsigned int uStreamPosition;
    int iSeekPending;
    int iNeedSync;
    BcfHeader header;
    unsigned int* pSeekOffsets;
    unsigned int uDurationMs;
    unsigned int uSeekGranularity;
    unsigned char* pInput;
    unsigned int uInputFill;
    unsigned int uInputPosition;
    unsigned int uInputCapacity;
    unsigned int uReserved68;
    unsigned int uOutputCapacity;
    unsigned int uFrameBytes;
    unsigned int uSkipSamples;
    unsigned int uLoopStartRemainder;
    unsigned int uLoopEndOffset;
    unsigned int uLoopEndRemainder;
    unsigned int uSamplesRemaining;
    unsigned int uDataOffset;
    unsigned int uPendingSkip;
    unsigned int uPendingSeek;
    int iResyncPending;
    unsigned int uStreamCount;
    unsigned int auStreamChannels[BCF_MAX_STREAMS];
    unsigned int auStreamBytes[BCF_MAX_STREAMS];
    BINKAUDIODECOMP* apDecoders[BCF_MAX_STREAMS];
    unsigned int uReservedCC;
};

int PROVIDER_property(unsigned int uProperty, void* pBefore, const void* pNewValue, void* pAfter);
const char* ASI_error();
int ASI_startup();
#if LCE_PLATFORM_WIIU
int ASI_stream_open(BCF_ALLOC_CALLBACK pAlloc, BCF_FREE_CALLBACK pFree, unsigned int uUser, BCF_FETCH_CALLBACK pFetch,
                    unsigned int uTotalSize);
int ASI_stream_close(int iStream);
#elif LCE_PLATFORM_PC
void* ASI_stream_open(BCF_ALLOC_CALLBACK pAlloc, BCF_FREE_CALLBACK pFree, uintptr_t uUser, BCF_FETCH_CALLBACK pFetch,
                      unsigned int uTotalSize);
int ASI_stream_close(void* iStream);
#endif
void decode_bink_buffer(BcfStream* pStream);
#if LCE_PLATFORM_WIIU
void FindFrameBoundary(int iStream);
#elif LCE_PLATFORM_PC
void FindFrameBoundary(void* iStream);
#endif
void SeekStreamForward(BcfStream* pStream, int iBytes);
#if LCE_PLATFORM_WIIU
int ASI_stream_process(int iStream, void* pBuffer, int iBytes);
int ASI_stream_seek(int iStream, int iPosition);
#elif LCE_PLATFORM_PC
int ASI_stream_process(void* iStream, void* pBuffer, int iBytes);
int ASI_stream_seek(void* iStream, int iPosition);
#endif
void SeekTableLookup(BcfStream* pStream, unsigned int uSample, int* piOffset, int* piRemainder, int* piBase);
#if LCE_PLATFORM_WIIU
int ASI_stream_property(int iStream, unsigned int uProperty, void* pBefore, const void* pNewValue, void* pAfter);
#elif LCE_PLATFORM_PC
int ASI_stream_property(void* iStream, unsigned int uProperty, void* pBefore, const void* pNewValue, void* pAfter);
#endif
int ASI_shutdown();

extern "C" int BinkADec_RIB_Main(unsigned int uProvider, int iUpDown,
                                 RIB_ALLOC_PROVIDER_HANDLE_CALLBACK pAllocProviderHandle,
                                 RIB_REGISTER_INTERFACE_CALLBACK pRegisterInterface,
                                 RIB_UNREGISTER_INTERFACE_CALLBACK pUnregisterInterface);

#endif
