#ifndef PLATFORM_PC_TLSSTORAGE_HPP
#define PLATFORM_PC_TLSSTORAGE_HPP

unsigned int TlsAlloc();
void* TlsGetValue(unsigned int index);
int TlsSetValue(unsigned int index, void* value);

#endif
