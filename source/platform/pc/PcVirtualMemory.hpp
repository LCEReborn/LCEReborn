#ifndef PLATFORM_PC_PCVIRTUALMEMORY_HPP
#define PLATFORM_PC_PCVIRTUALMEMORY_HPP

#include <cstddef>

void* PcVirtualReserve(std::size_t size, bool commit);
bool PcVirtualCommit(void* address, std::size_t size);
bool PcVirtualDecommit(void* address, std::size_t size);
bool PcVirtualRelease(void* address, std::size_t size);

#endif
