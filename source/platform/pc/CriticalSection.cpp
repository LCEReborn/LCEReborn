#include "game/core/CriticalSection.hpp"

void InitializeCriticalSection(C4JCriticalSection*)
{
}

void InitializeCriticalSectionAndSpinCount(C4JCriticalSection* criticalSection, unsigned long)
{
    InitializeCriticalSection(criticalSection);
}

void EnterCriticalSection(C4JCriticalSection* criticalSection)
{
    criticalSection->mutex.lock();
}

int TryEnterCriticalSection(C4JCriticalSection* criticalSection)
{
    return criticalSection->mutex.try_lock() ? 1 : 0;
}

void LeaveCriticalSection(C4JCriticalSection* criticalSection)
{
    criticalSection->mutex.unlock();
}

void DeleteCriticalSection(C4JCriticalSection*)
{
}
