#ifndef GAME_CORE_CHATSCREEN_HPP
#define GAME_CORE_CHATSCREEN_HPP

#include <string>

class Font;
class Gui;

namespace ChatScreen
{
enum EOpenMode
{
    EOpenMode_Keyboard,
    EOpenMode_Command,
    EOpenMode_Controller
};

bool Open(int pad, EOpenMode mode);
void OpenAfterMenus(int pad, EOpenMode mode);
void Close(int pad);
bool IsOpen(int pad);
void Tick(int pad);
void Render(int pad, Gui& gui, Font& font, int width, int height, float pixelsPerUnit);
bool DrawnOverHud(int pad);
void RenderOverHud(int pad, int viewport, int width, int height);
void RememberSender(int pad, const std::wstring& name);
}

#endif
