#ifndef PLATFORM_PC_FILEAPI_HPP
#define PLATFORM_PC_FILEAPI_HPP

#include <stdint.h>

#include "platform/C4JStorage.hpp"
#include "platform/Runtime.hpp"

unsigned int GetFileSize(int file, unsigned int* fileSizeHigh);
int ReadFile(int file, void* buffer, unsigned int bytesToRead, unsigned int* bytesRead, _OVERLAPPED* overlapped);
int WriteFile(int file, const void* buffer, unsigned int bytesToWrite, unsigned int* bytesWritten,
              _OVERLAPPED* overlapped);
int SetFilePointer(int file, long distanceToMove, int* distanceToMoveHigh, unsigned int moveMethod);
unsigned int GetFileAttributesA(const char* fileName);
void* VirtualAlloc(void* address, unsigned long size, unsigned int allocationType, unsigned int protect);
int VirtualFree(void* address, unsigned long size, unsigned int freeType);

int OpenDir(const char* path, int* directory);
int ReadDir(int* directory, FSDirEntry* entry);
int CloseDir(int* directory);
void CloseHandle(int file);
int CreateDirectoryA(const char* pathName, void* securityAttributes);
int CreateFileA(const char* fileName, unsigned int desiredAccess, unsigned int shareMode, void* securityAttributes,
                unsigned int creationDisposition, unsigned int flagsAndAttributes, int templateFile);

#endif
