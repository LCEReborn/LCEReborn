#ifndef GAME_CORE_GUIMESSAGE_HPP
#define GAME_CORE_GUIMESSAGE_HPP

#include <string>

class GuiMessage
{
public:
    GuiMessage(const std::wstring& message, unsigned int colour = 0xFFFFFF);

    std::wstring message;
    int age;
    unsigned int colour;
};

#endif
