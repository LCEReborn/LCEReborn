#include "platform/pc/PcTextInput.hpp"

#include <SDL3/SDL.h>

void PcTextComposition::Clear()
{
    text.clear();
    cursor = selection = 0;
}

void PcTextComposition::Update(const char* utf8, int start, int length)
{
    Clear();
    const char* source = utf8 ? utf8 : "";
    size_t remaining = SDL_strlen(source);
    // SDL offsets count Unicode characters, not bytes or native wchar_t units.
    // Clamp absent/invalid selections, including negative IME cursor offsets.
    size_t first = start < 0 ? SDL_MAX_SINT32 : static_cast<size_t>(start);
    size_t last = first + (length > 0 ? static_cast<size_t>(length) : 0);
    size_t character = 0;
    size_t end = 0;
    while (remaining > 0)
    {
        if (character == first)
            cursor = text.size();
        if (character == last)
            end = text.size();
        unsigned int codepoint = SDL_StepUTF8(&source, &remaining);
        if (sizeof(wchar_t) == 2 && codepoint > 0xffff)
        {
            codepoint -= 0x10000;
            text.push_back(static_cast<wchar_t>(0xd800 + (codepoint >> 10)));
            text.push_back(static_cast<wchar_t>(0xdc00 + (codepoint & 0x3ff)));
        }
        else
            text.push_back(static_cast<wchar_t>(codepoint));
        ++character;
    }
    if (first >= character)
        cursor = text.size();
    if (last >= character)
        end = text.size();
    selection = end - cursor;
}
