#ifndef PLATFORM_PC_PCINPUTEVENTS_HPP
#define PLATFORM_PC_PCINPUTEVENTS_HPP

union SDL_Event;

void PcInputHandleEvent(const SDL_Event& event);

#endif
