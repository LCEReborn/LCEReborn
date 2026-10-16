#ifndef PLATFORM_PC_AUDIO_MILESPCHANDLES_HPP
#define PLATFORM_PC_AUDIO_MILESPCHANDLES_HPP

unsigned int MilesPcRegisterHandle(void* pObject);
void* MilesPcLookupHandle(unsigned int uHandle);
void* MilesPcReleaseHandle(unsigned int uHandle);

#endif
