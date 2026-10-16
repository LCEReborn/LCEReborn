#include "platform/pc/audio/MilesPcFileSystem.hpp"

#include <stdlib.h>
#include <string.h>

#include <new>

#include "platform/FileApi.hpp"
#include "platform/pc/audio/MilesPcHandles.hpp"

namespace
{
const int FILE_NAME_LIMIT = 260;
const char EMBEDDED_FILE_MARKER = '*';
const char EMBEDDED_FILE_EXTENSION = '.';
const unsigned int SEEK_ORIGIN_CURRENT = 1;
const unsigned int SEEK_ORIGIN_END = 2;

struct MilesPcFile
{
    int fileHandle;
    unsigned int startOffset;
    unsigned int length;
    unsigned int currentPosition;
    unsigned int fileSize;
};

bool CopyField(const char** cursor, char terminator, char* destination)
{
    int length = 0;

    while (**cursor != terminator)
    {
        if (**cursor == '\0' || length == FILE_NAME_LIMIT - 1)
        {
            return false;
        }

        destination[length] = **cursor;
        ++length;
        ++*cursor;
    }

    destination[length] = '\0';
    ++*cursor;
    return true;
}

int OpenFile(const char* fileName, unsigned int* fileSize)
{
    int fileHandle = CreateFileA(fileName, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, 0);
    if (fileHandle == INVALID_HANDLE_VALUE)
    {
        return INVALID_HANDLE_VALUE;
    }

    *fileSize = GetFileSize(fileHandle, NULL);
    return fileHandle;
}

unsigned int ClampPosition(unsigned int target, unsigned int limit)
{
    if (target > limit)
    {
        return limit;
    }

    return target;
}
}

unsigned int file_open_callback(const char* filename, unsigned int* handleOut)
{
    MilesPcFile* file;
    char localName[FILE_NAME_LIMIT];
    char lengthText[FILE_NAME_LIMIT];
    char offsetText[FILE_NAME_LIMIT];
    unsigned int fileSize;
    int fileHandle;

    *handleOut = 0;

    if (filename[0] == EMBEDDED_FILE_MARKER)
    {
        const char* cursor = filename + 1;

        if (!CopyField(&cursor, EMBEDDED_FILE_MARKER, localName)
            || !CopyField(&cursor, EMBEDDED_FILE_MARKER, lengthText)
            || !CopyField(&cursor, EMBEDDED_FILE_EXTENSION, offsetText))
        {
            return 0;
        }

        fileHandle = OpenFile(localName, &fileSize);
        if (fileHandle == INVALID_HANDLE_VALUE)
        {
            return 0;
        }

        file = new (std::nothrow) MilesPcFile();
        if (file == NULL)
        {
            CloseHandle(fileHandle);
            return 0;
        }

        file->fileHandle = fileHandle;
        file->fileSize = fileSize;
        file->length = static_cast<unsigned int>(atoi(lengthText));
        file->startOffset = static_cast<unsigned int>(atoi(offsetText));
        file->currentPosition = file->startOffset;
    }
    else
    {
        fileHandle = OpenFile(filename, &fileSize);
        if (fileHandle == INVALID_HANDLE_VALUE)
        {
            return 0;
        }

        file = new (std::nothrow) MilesPcFile();
        if (file == NULL)
        {
            CloseHandle(fileHandle);
            return 0;
        }

        file->fileHandle = fileHandle;
        file->fileSize = fileSize;
        file->length = 0;
        file->startOffset = 0;
        file->currentPosition = 0;
    }

    *handleOut = MilesPcRegisterHandle(file);
    if (*handleOut == 0)
    {
        CloseHandle(file->fileHandle);
        delete file;
        return 0;
    }

    return 1;
}

void file_close_callback(unsigned int handle)
{
    MilesPcFile* file = static_cast<MilesPcFile*>(MilesPcReleaseHandle(handle));
    if (file == NULL)
    {
        return;
    }

    CloseHandle(file->fileHandle);
    delete file;
}

int file_seek_callback(unsigned int handle, int offset, unsigned int origin)
{
    MilesPcFile* file = static_cast<MilesPcFile*>(MilesPcLookupHandle(handle));
    unsigned int target;
    unsigned int end;

    if (file == NULL)
    {
        return 0;
    }

    if (file->length == 0)
    {
        if (origin == SEEK_ORIGIN_CURRENT)
        {
            target = file->currentPosition + offset;
        }
        else if (origin == SEEK_ORIGIN_END)
        {
            target = file->fileSize + offset;
        }
        else
        {
            target = offset;
        }

        file->currentPosition = ClampPosition(target, file->fileSize);
        return static_cast<int>(file->currentPosition);
    }

    end = file->startOffset + file->length;

    if (origin == SEEK_ORIGIN_CURRENT)
    {
        target = file->currentPosition + offset;
    }
    else if (origin == SEEK_ORIGIN_END)
    {
        target = end + offset;
    }
    else
    {
        target = file->startOffset + offset;
    }

    file->currentPosition = ClampPosition(target, end);
    return static_cast<int>(file->currentPosition - file->startOffset);
}

int file_read_callback(unsigned int handle, void* buffer, unsigned int bytes)
{
    MilesPcFile* file = static_cast<MilesPcFile*>(MilesPcLookupHandle(handle));
    unsigned int bytesRead;

    if (file == NULL)
    {
        return 0;
    }

    if (file->length != 0)
    {
        unsigned int end = file->startOffset + file->length;
        if (file->currentPosition + bytes > end)
        {
            bytes = end - file->currentPosition;
        }
    }

    if (bytes == 0 || file->currentPosition >= file->fileSize)
    {
        return 0;
    }

    SetFilePointer(file->fileHandle, static_cast<long>(file->currentPosition), NULL, 0);

    bytesRead = 0;
    if (ReadFile(file->fileHandle, buffer, bytes, &bytesRead, NULL) == 0)
    {
        return 0;
    }

    file->currentPosition = file->currentPosition + bytesRead;
    return static_cast<int>(bytesRead);
}
