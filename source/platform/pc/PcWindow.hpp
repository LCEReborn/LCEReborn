#ifndef PLATFORM_PC_PCWINDOW_HPP
#define PLATFORM_PC_PCWINDOW_HPP

bool PcWindowCreate(int width, int height);
void PcWindowDestroy();
void PcWindowShow();
void PcWindowSwap();
void PcWindowGetDrawableSize(int* width, int* height);
void* PcWindowGetProcAddress(const char* name);
void PcPumpEvents();
void PcWindowPaceFrame();
bool PcWindowIsFullscreen();
void PcWindowSetFullscreen(bool fullscreen);
bool PcWindowIsVSync();
void PcWindowSetVSync(bool enabled);

#endif
