#ifndef PLATFORM_PC_PCINPUTEVENTS_HPP
#define PLATFORM_PC_PCINPUTEVENTS_HPP

#include <string>
#include "platform/pc/PcTextInput.hpp"

union SDL_Event;

struct PcKeyboardText
{
    std::wstring title;
    std::wstring text;
    PcTextComposition composition;
};

void PcInputHandleEvent(const SDL_Event& event);
bool PcInputGetKeyboardText(PcKeyboardText* text);

#endif
