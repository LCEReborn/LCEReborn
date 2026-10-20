#include "game/core/L10N.hpp"

#include "game/core/CMinecraftApp.hpp"
#include "game/core/CConsoleMinecraftApp.hpp"
#include "game/core/ExtraStrings.hpp"
#include "game/data/StringTable.hpp"

static StringTable* g_stringTable = NULL;
extern CConsoleMinecraftApp gApp;

const wchar_t* ExtraString(int id)
{
    switch (id)
    {
    case IDS_LCE_CHAT:
        return L"Chat";
    case IDS_LCE_CHAT_SEND:
        return L"Send";
    case IDS_LCE_CHAT_CLOSE:
        return L"Close";
    case IDS_LCE_CHAT_COMPLETE:
        return L"Complete";
    case IDS_LCE_CHAT_TYPE:
        return L"Type";
    case IDS_LCE_CHAT_DELETE:
        return L"Delete";
    case IDS_LCE_CHAT_SPACE:
        return L"Space";
    case IDS_LCE_CHAT_SHIFT:
        return L"Shift";
    case IDS_LCE_CHAT_QUICK:
        return L"Quick Chat";
    case IDS_LCE_CHAT_KEYBOARD:
        return L"Keyboard";
    case IDS_LCE_CHAT_CHOOSE:
        return L"Choose";
    case IDS_LCE_CHAT_HISTORY:
        return L"History";
    case IDS_LCE_CHAT_SCROLL:
        return L"Scroll";
    case IDS_LCE_DEBUG_CHANGE:
        return L"Change";
    case IDS_LCE_DEBUG_PREVIEW:
        return L"Preview";
    case IDS_LCE_DEBUG_PRESET:
        return L"Preset";
    case IDS_LCE_DEBUG_RESET:
        return L"Reset";
    default:
        return NULL;
    }
}

const wchar_t* L10N::GetString(int id)
{
    const wchar_t* extra = ExtraString(id);
    if (extra != NULL)
        return extra;
    if (g_stringTable == NULL)
        return L"";
    return g_stringTable->getString(id);
}

void L10N::loadStringTable()
{
    delete g_stringTable;
    std::wstring path = L"languages.loc";
    if (gApp.hasArchiveFile(path, true))
    {
        arrayWithLength<unsigned char> data = gApp.getArchiveFile(path, true);
        g_stringTable = new StringTable(data, false, false);
        delete[] data.data;
    }
    else
    {
        g_stringTable = NULL;
    }
}
