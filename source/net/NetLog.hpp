#ifndef NET_NETLOG_HPP
#define NET_NETLOG_HPP

#include "platform/Platform.hpp"

#if LCE_PLATFORM_PC
#include "platform/pc/Log.hpp"

namespace lcenet
{
extern const Logger kLog;
}

#define NET_LOG_DEBUG(...) LOG_DEBUG(lcenet::kLog, __VA_ARGS__)
#define NET_LOG_INFO(...) LOG_INFO(lcenet::kLog, __VA_ARGS__)
#define NET_LOG_WARN(...) LOG_WARN(lcenet::kLog, __VA_ARGS__)
#elif LCE_PLATFORM_WIIU
namespace lcenet
{
void ConsoleLog(const char* format, ...);
}

#define NET_LOG_DEBUG(...)
#define NET_LOG_INFO(format, ...) lcenet::ConsoleLog("[Network] " format "\n", ##__VA_ARGS__)
#define NET_LOG_WARN(format, ...) lcenet::ConsoleLog("[Network] warning: " format "\n", ##__VA_ARGS__)
#endif

#endif
