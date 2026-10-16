#include "platform/pc/audio/MilesPcBank.hpp"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <set>

#include "vendor/MilesBank.hpp"
#include "vendor/MilesEventExec.hpp"
#include "vendor/MilesMarker.hpp"
#include "vendor/MilesMem.hpp"

namespace
{
const int WORD_BYTES = 4;
const int PRESET_SIZE_WORD = 3;
const int PRESET_HEADER_WORDS = 4;
const int ASSET_ENTRY_WORDS = 2;
const int SOUND_RECORD_WORDS = 14;
const int MARKER_ENTRY_WORDS = 3;
const int SETTINGS_HEADER_WORDS = 8;
const int SETTINGS_BUS_WORDS = 3;
const int SETTINGS_CURVE_HEADER_WORDS = 2;
const int SETTINGS_VARIABLE_WORDS = 2;

struct BankPrefix
{
    char* path;
};

class BankSwapper
{
public:
    BankSwapper(unsigned char* pImage, int iSize) : m_image(pImage), m_size(iSize) {}

    bool Contains(long long llOffset, long long llBytes) const
    {
        return llOffset >= 0 && llBytes >= 0 && llOffset + llBytes <= m_size;
    }

    bool SwapWords(long long llOffset, long long llWords)
    {
        if (!Contains(llOffset, llWords * WORD_BYTES))
        {
            return false;
        }

        unsigned char* pWord = m_image + llOffset;
        for (long long llWord = 0; llWord < llWords; ++llWord)
        {
            unsigned char byte0 = pWord[0];
            unsigned char byte1 = pWord[1];
            pWord[0] = pWord[3];
            pWord[1] = pWord[2];
            pWord[2] = byte1;
            pWord[3] = byte0;
            pWord += WORD_BYTES;
        }

        return true;
    }

    int Word(long long llOffset) const
    {
        int iValue;
        memcpy(&iValue, m_image + llOffset, sizeof(iValue));
        return iValue;
    }

    bool SwapAssetTable(int iTableOffset, int iCount)
    {
        if (iCount < 0)
        {
            return false;
        }

        return iCount == 0 || SwapWords(iTableOffset, static_cast<long long>(iCount) * ASSET_ENTRY_WORDS);
    }

    bool SwapPreset(int iOffset)
    {
        if (!m_presets.insert(iOffset).second)
        {
            return true;
        }

        if (!SwapWords(iOffset, PRESET_HEADER_WORDS))
        {
            return false;
        }

        int iSize = Word(iOffset + PRESET_SIZE_WORD * WORD_BYTES);
        if (iSize < PRESET_HEADER_WORDS * WORD_BYTES || (iSize % WORD_BYTES) != 0)
        {
            return false;
        }

        return SwapWords(iOffset + PRESET_HEADER_WORDS * WORD_BYTES, iSize / WORD_BYTES - PRESET_HEADER_WORDS);
    }

    bool SwapPresets(int iTableOffset, int iCount)
    {
        for (int iEntry = 0; iEntry < iCount; ++iEntry)
        {
            int iValue = Word(iTableOffset + (iEntry * ASSET_ENTRY_WORDS + 1) * WORD_BYTES);
            if (!SwapPreset(iValue))
            {
                return false;
            }
        }

        return true;
    }

    bool SwapSound(int iOffset)
    {
        if (!m_sounds.insert(iOffset).second)
        {
            return true;
        }

        if (!SwapWords(iOffset, SOUND_RECORD_WORDS + 1))
        {
            return false;
        }

        int iMarkers = Word(iOffset + SOUND_RECORD_WORDS * WORD_BYTES);
        if (iMarkers < 0)
        {
            return false;
        }

        return iMarkers == 0
               || SwapWords(iOffset + (SOUND_RECORD_WORDS + 1) * WORD_BYTES,
                            static_cast<long long>(iMarkers) * MARKER_ENTRY_WORDS);
    }

    bool SwapSounds(int iTableOffset, int iCount)
    {
        for (int iEntry = 0; iEntry < iCount; ++iEntry)
        {
            int iValue = Word(iTableOffset + (iEntry * ASSET_ENTRY_WORDS + 1) * WORD_BYTES);
            if (!SwapSound(iValue))
            {
                return false;
            }
        }

        return true;
    }

    bool SwapSettings(int iOffset)
    {
        if (!SwapWords(iOffset, SETTINGS_HEADER_WORDS))
        {
            return false;
        }

        const EventSettings* pSettings = reinterpret_cast<const EventSettings*>(m_image + iOffset);
        if (pSettings->magic != EVENT_SETTINGS_MAGIC)
        {
            return true;
        }

        long long llSteps = static_cast<long long>(pSettings->limitCount) + pSettings->blendCount
                            + pSettings->rampCount + pSettings->lfoCount;
        if (llSteps < 0 || pSettings->busCount < 0 || pSettings->curveVariableCount < 0
            || pSettings->variableCount < 0)
        {
            return false;
        }

        long long llCursor = iOffset + SETTINGS_HEADER_WORDS * WORD_BYTES;
        if (!SwapWords(llCursor, llSteps))
        {
            return false;
        }

        llCursor += llSteps * WORD_BYTES;
        if (!SwapWords(llCursor, static_cast<long long>(pSettings->busCount) * SETTINGS_BUS_WORDS))
        {
            return false;
        }

        llCursor += static_cast<long long>(pSettings->busCount) * SETTINGS_BUS_WORDS * WORD_BYTES;
        for (int iCurve = 0; iCurve < pSettings->curveVariableCount; ++iCurve)
        {
            if (!SwapWords(llCursor, SETTINGS_CURVE_HEADER_WORDS))
            {
                return false;
            }

            int iBytes = Word(llCursor + WORD_BYTES);
            if (iBytes < 0 || (iBytes % WORD_BYTES) != 0)
            {
                return false;
            }

            llCursor += SETTINGS_CURVE_HEADER_WORDS * WORD_BYTES;
            if (!SwapWords(llCursor, iBytes / WORD_BYTES))
            {
                return false;
            }

            llCursor += iBytes;
        }

        return SwapWords(llCursor, static_cast<long long>(pSettings->variableCount) * SETTINGS_VARIABLE_WORDS);
    }

private:
    unsigned char* m_image;
    long long m_size;
    std::set<int> m_presets;
    std::set<int> m_sounds;
};

char UpperCase(char cValue)
{
    if (cValue >= 'a' && cValue <= 'z')
    {
        return cValue - ('a' - 'A');
    }

    return cValue;
}

BankPrefix* Prefix(SoundBank* pBank)
{
    return reinterpret_cast<BankPrefix*>(reinterpret_cast<unsigned char*>(pBank) - MILES_PC_BANK_PREFIX_SIZE);
}
}

unsigned int MilesPcReadBigEndian32(const void* pSource)
{
    const unsigned char* pBytes = static_cast<const unsigned char*>(pSource);
    return (static_cast<unsigned int>(pBytes[0]) << 24) | (static_cast<unsigned int>(pBytes[1]) << 16)
           | (static_cast<unsigned int>(pBytes[2]) << 8) | static_cast<unsigned int>(pBytes[3]);
}

SoundBank* MilesPcBankAllocate(int iMemoryUsage, const char* pOwnerName, int iOwnerLine)
{
    unsigned char* pBlock;

    if (iMemoryUsage <= 0)
    {
        return NULL;
    }

    pBlock = static_cast<unsigned char*>(
        AIL_mem_alloc_lock_info(iMemoryUsage + MILES_PC_BANK_PREFIX_SIZE, pOwnerName, iOwnerLine));
    if (pBlock == NULL)
    {
        return NULL;
    }

    memset(pBlock, 0, MILES_PC_BANK_PREFIX_SIZE);
    return reinterpret_cast<SoundBank*>(pBlock + MILES_PC_BANK_PREFIX_SIZE);
}

void MilesPcBankFree(SoundBank* pBank)
{
    if (pBank == NULL)
    {
        return;
    }

    AIL_mem_free_lock(Prefix(pBank)->path);
    Prefix(pBank)->path = NULL;
    AIL_mem_free_lock(Prefix(pBank));
}

void MilesPcBankSetPath(SoundBank* pBank, char* pPath)
{
    Prefix(pBank)->path = pPath;
}

char* MilesPcBankPath(SoundBank* pBank)
{
    return Prefix(pBank)->path;
}

int MilesPcSwapSoundBank(SoundBank* pBank, int iMemoryUsage)
{
    SoundBankImage* pImage = reinterpret_cast<SoundBankImage*>(pBank);
    BankSwapper swapper(reinterpret_cast<unsigned char*>(pBank), iMemoryUsage);

    if (!swapper.SwapWords(0, offsetof(SoundBankImage, name) / WORD_BYTES))
    {
        return 0;
    }

    if (pImage->eventCount < 0 || pImage->environmentCount < 0 || pImage->presetCount < 0
        || pImage->soundCount < 0)
    {
        return 0;
    }

    if (!swapper.SwapAssetTable(pImage->events, pImage->eventCount)
        || !swapper.SwapAssetTable(pImage->environments, pImage->environmentCount)
        || !swapper.SwapAssetTable(pImage->presets, pImage->presetCount)
        || !swapper.SwapAssetTable(pImage->sounds, pImage->soundCount))
    {
        return 0;
    }

    if (!swapper.SwapPresets(pImage->environments, pImage->environmentCount)
        || !swapper.SwapPresets(pImage->presets, pImage->presetCount)
        || !swapper.SwapSounds(pImage->sounds, pImage->soundCount))
    {
        return 0;
    }

    if (pImage->settings != 0 && !swapper.SwapSettings(pImage->settings))
    {
        return 0;
    }

    return 1;
}

SoundBank::AssetEntry* MilesPcFindAsset(SoundBank* pBank, const char* pName, int iEntriesOffset, int iCount)
{
    SoundBank::AssetEntry* pEntries;

    pEntries = reinterpret_cast<SoundBank::AssetEntry*>(reinterpret_cast<char*>(pBank) + iEntriesOffset);

    for (;;)
    {
        if (iCount == 0)
        {
            return NULL;
        }

        int iMidpoint = iCount / 2;
        SoundBank::AssetEntry* pEntry = pEntries + iMidpoint;
        const char* pEntryName = reinterpret_cast<const char*>(pBank) + pEntry->nameOffset;
        const char* pSearchName = pName;
        int iComparison;

        for (;;)
        {
            int iEntryCharacter = static_cast<unsigned char>(UpperCase(*pEntryName));
            int iSearchCharacter = static_cast<unsigned char>(UpperCase(*pSearchName));
            if (iEntryCharacter < iSearchCharacter)
            {
                iComparison = -1;
                break;
            }

            if (iEntryCharacter > iSearchCharacter)
            {
                iComparison = 1;
                break;
            }

            if (iEntryCharacter == 0)
            {
                iComparison = 0;
                break;
            }

            ++pEntryName;
            ++pSearchName;
        }

        if (iComparison == 0)
        {
            return pEntry;
        }

        if (iCount == 1)
        {
            return NULL;
        }

        if (iComparison > 0)
        {
            iCount = iMidpoint;
        }
        else
        {
            pEntries = pEntry + 1;
            iCount = iCount - iMidpoint - 1;
        }
    }
}
