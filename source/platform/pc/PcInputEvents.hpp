#ifndef PLATFORM_PC_PCINPUTEVENTS_HPP
#define PLATFORM_PC_PCINPUTEVENTS_HPP

#include <string>

union SDL_Event;

void PcInputHandleEvent(const SDL_Event& event);
bool PcInputGetKeyboardText(std::wstring* title, std::wstring* text);

#endif
