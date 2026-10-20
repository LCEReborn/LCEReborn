#include "game/core/FileHeader.hpp"
#include "game/core/StringConversion.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

static void Require(bool condition, const char* message)
{
    if (!condition)
    {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}

int main()
{
    const unsigned short parameter[] = {'X', 'M', 'L', 'V', 'E', 'R', 'S', 'I', 'O', 'N'};
    unsigned char unaligned[sizeof(parameter) + 1];
    std::memcpy(unaligned + 1, parameter, sizeof(parameter));
    Require(convUtf16ToWstring(unaligned + 1, 10) == L"XMLVERSION", "unaligned, bounded DLC parameter text");
    const std::wstring unicode = L"\u4e16\u754c\U0001F600";
    unsigned short encoded[5] = {};
    convWstringToUtf16(unicode, encoded, 5);
    Require(encoded[0] == 0x4e16 && encoded[1] == 0x754c && encoded[2] == 0xd83d && encoded[3] == 0xde00,
            "UTF-16 surrogate encoding");
    Require(convUtf16ToWstring(encoded, 5) == unicode, "native wide character round trip");
    unsigned short truncated[4] = {};
    convWstringToUtf16(unicode, truncated, 4);
    Require(truncated[2] == 0, "truncate without splitting a surrogate pair");

    const std::wstring name = L"world/\u4e16\u754c.dat";
    FileHeader header;
    FileEntry* entry = header.AddFile(name, 8);
    entry->m_data.m_timestamp = 0x0102030405060708LL;
    std::vector<unsigned char> data(header.GetFileSize(), 0);
    header.WriteHeader(data.data());
    unsigned int index = 0;
    std::memcpy(&index, data.data(), 4);
    Require(index == 20 && data.size() == 20 + 144, "fixed save entry size, independent of wchar_t");
    unsigned int size = 0;
    std::memcpy(&size, data.data() + index + 128, 4);
    Require(size == 8, "save metadata follows 128 bytes of UTF-16 filename");
    FileHeader loaded;
    loaded.ReadHeader(data.data(), eSavePlatform_WIN);
    Require(loaded.m_fileEntries.size() == 1 && loaded.fileExists(name), "read little endian save filename");
    Require(loaded.m_fileEntries[0]->m_data.m_fileSize == 8 && loaded.m_fileEntries[0]->m_data.m_startOffset == 12
                && loaded.m_fileEntries[0]->m_data.m_timestamp == entry->m_data.m_timestamp,
            "read little endian save metadata");

    std::vector<unsigned char> foreign = data;
    std::reverse(foreign.begin(), foreign.begin() + 4);
    std::reverse(foreign.begin() + 4, foreign.begin() + 8);
    std::reverse(foreign.begin() + 8, foreign.begin() + 10);
    std::reverse(foreign.begin() + 10, foreign.begin() + 12);
    for (unsigned int offset = 0; offset < 128; offset += 2)
        std::swap(foreign[index + offset], foreign[index + offset + 1]);
    std::reverse(foreign.begin() + index + 128, foreign.begin() + index + 132);
    std::reverse(foreign.begin() + index + 132, foreign.begin() + index + 136);
    std::reverse(foreign.begin() + index + 136, foreign.begin() + index + 144);
    FileHeader imported;
    imported.ReadHeader(foreign.data(), eSavePlatform_WIIU);
    Require(imported.m_fileEntries.size() == 1 && imported.fileExists(name), "read Wii U UTF-16 save filename");
    Require(imported.m_fileEntries[0]->m_data.m_fileSize == 8 && imported.m_fileEntries[0]->m_data.m_startOffset == 12
                && imported.m_fileEntries[0]->m_data.m_timestamp == entry->m_data.m_timestamp,
            "read Wii U big endian save metadata");
    std::puts("PASS: bounded DLC text, Unicode, surrogate-safe truncation, PC and Wii U save file entries");
    return 0;
}
