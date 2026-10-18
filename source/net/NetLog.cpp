#include "net/NetLog.hpp"

#if LCE_PLATFORM_PC
namespace lcenet
{
const Logger kLog("Network");
}
#elif LCE_PLATFORM_WIIU
#include <stdarg.h>
#include <stdio.h>

#include <coreinit/debug.h>

namespace lcenet
{
void ConsoleLog(const char* format, ...)
{
    char text[256];
    va_list arguments;
    va_start(arguments, format);
    vsnprintf(text, sizeof(text), format, arguments);
    va_end(arguments);
    OSReport("%s", text);
}
}
#endif
