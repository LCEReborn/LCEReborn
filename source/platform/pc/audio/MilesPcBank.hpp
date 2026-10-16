#ifndef PLATFORM_PC_AUDIO_MILESPCBANK_HPP
#define PLATFORM_PC_AUDIO_MILESPCBANK_HPP

#include "game/client/audio/SoundBank.hpp"

const int MILES_PC_BANK_PREFIX_SIZE = 16;

unsigned int MilesPcReadBigEndian32(const void* pSource);
SoundBank* MilesPcBankAllocate(int iMemoryUsage, const char* pOwnerName, int iOwnerLine);
void MilesPcBankFree(SoundBank* pBank);
void MilesPcBankSetPath(SoundBank* pBank, char* pPath);
char* MilesPcBankPath(SoundBank* pBank);
int MilesPcSwapSoundBank(SoundBank* pBank, int iMemoryUsage);
SoundBank::AssetEntry* MilesPcFindAsset(SoundBank* pBank, const char* pName, int iEntriesOffset, int iCount);

#endif
