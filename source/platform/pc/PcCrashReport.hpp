#ifndef PLATFORM_PC_PCCRASHREPORT_HPP
#define PLATFORM_PC_PCCRASHREPORT_HPP

#include <string>

namespace PcCrashReport
{
typedef void (*SectionWriter)(std::string& out);

void Install(const std::string& logDirectory);
void AddSection(const char* title, SectionWriter writer);
void NameCurrentThread(const char* name);
std::string Write(const char* reason);
}

#endif
