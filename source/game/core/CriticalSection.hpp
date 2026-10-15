#ifndef GAME_CORE_CRITICALSECTION_HPP
#define GAME_CORE_CRITICALSECTION_HPP

#include "platform/Platform.hpp"

#if LCE_PLATFORM_WIIU
#include <coreinit/fastmutex.h>

typedef OSFastMutex C4JCriticalSection;
#elif LCE_PLATFORM_PC
#include <mutex>

struct C4JCriticalSection
{
    std::recursive_mutex mutex;
};
#endif

void InitializeCriticalSection(C4JCriticalSection* pCriticalSection);
void InitializeCriticalSectionAndSpinCount(C4JCriticalSection* pCriticalSection, unsigned long spinCount);
void EnterCriticalSection(C4JCriticalSection* pCriticalSection);
int TryEnterCriticalSection(C4JCriticalSection* pCriticalSection);
void LeaveCriticalSection(C4JCriticalSection* pCriticalSection);
void DeleteCriticalSection(C4JCriticalSection* pCriticalSection);

#endif
