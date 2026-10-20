#include "game/core/GuiMessage.hpp"

GuiMessage::GuiMessage(const std::wstring& message, unsigned int colour) : message(message), age(0), colour(colour)
{
}
