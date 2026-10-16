#include "platform/pc/audio/MilesPcHandles.hpp"

#include <stddef.h>

#include <mutex>
#include <vector>

namespace
{
struct HandleTable
{
    std::mutex mutex;
    std::vector<void*> objects;
    std::vector<unsigned int> freeSlots;
};

HandleTable& Table()
{
    static HandleTable table;
    return table;
}
}

unsigned int MilesPcRegisterHandle(void* pObject)
{
    HandleTable& table = Table();
    std::lock_guard<std::mutex> lock(table.mutex);
    unsigned int uSlot;

    if (pObject == NULL)
    {
        return 0;
    }

    if (!table.freeSlots.empty())
    {
        uSlot = table.freeSlots.back();
        table.freeSlots.pop_back();
        table.objects[uSlot] = pObject;
    }
    else
    {
        uSlot = static_cast<unsigned int>(table.objects.size());
        table.objects.push_back(pObject);
    }

    return uSlot + 1;
}

void* MilesPcLookupHandle(unsigned int uHandle)
{
    HandleTable& table = Table();
    std::lock_guard<std::mutex> lock(table.mutex);

    if (uHandle == 0 || uHandle > table.objects.size())
    {
        return NULL;
    }

    return table.objects[uHandle - 1];
}

void* MilesPcReleaseHandle(unsigned int uHandle)
{
    HandleTable& table = Table();
    std::lock_guard<std::mutex> lock(table.mutex);
    void* pObject;

    if (uHandle == 0 || uHandle > table.objects.size())
    {
        return NULL;
    }

    pObject = table.objects[uHandle - 1];
    if (pObject != NULL)
    {
        table.objects[uHandle - 1] = NULL;
        table.freeSlots.push_back(uHandle - 1);
    }

    return pObject;
}
