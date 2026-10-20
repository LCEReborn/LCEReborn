#ifndef PLATFORM_PC_PCTEXTINPUT_HPP
#define PLATFORM_PC_PCTEXTINPUT_HPP

#include <string>

struct PcTextComposition
{
    std::wstring text;
    size_t cursor = 0;
    size_t selection = 0;

    void Update(const char* utf8, int start, int length);
    void Clear();
};

#endif
