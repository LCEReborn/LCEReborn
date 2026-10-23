#ifndef PLATFORM_PC_PCDISCORD_HPP
#define PLATFORM_PC_PCDISCORD_HPP

#include <string>

void PcDiscordStart(const std::string& applicationId);
void PcDiscordSetPresence(const std::wstring& details);

#endif
