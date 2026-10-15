#include "platform/pc/TLSStorage.hpp"

#include <atomic>

static const unsigned int TLS_SLOT_COUNT = 64;
static const unsigned int TLS_OUT_OF_INDEXES = 0xFFFFFFFF;

static std::atomic<bool> s_allocated[TLS_SLOT_COUNT];
static thread_local void* s_values[TLS_SLOT_COUNT];

unsigned int TlsAlloc()
{
    for (unsigned int index = 0; index < TLS_SLOT_COUNT; ++index)
    {
        bool expected = false;
        if (s_allocated[index].compare_exchange_strong(expected, true))
        {
            s_values[index] = nullptr;
            return index;
        }
    }
    return TLS_OUT_OF_INDEXES;
}

void* TlsGetValue(unsigned int index)
{
    if (index >= TLS_SLOT_COUNT || !s_allocated[index])
        return nullptr;
    return s_values[index];
}

int TlsSetValue(unsigned int index, void* value)
{
    if (index >= TLS_SLOT_COUNT || !s_allocated[index])
        return 0;
    s_values[index] = value;
    return 1;
}
