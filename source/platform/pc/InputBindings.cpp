#include "platform/pc/InputBindings.hpp"

#include <SDL3/SDL.h>

namespace
{
const PcInputBinding s_keyBindings[] = {
    {SDL_SCANCODE_W, 0, 0, EPcStickDirection_LeftUp, EPcStickDirection_LeftUp, 0},
    {SDL_SCANCODE_S, 0, 0, EPcStickDirection_LeftDown, EPcStickDirection_LeftDown, 0},
    {SDL_SCANCODE_A, 0, 0, EPcStickDirection_LeftLeft, EPcStickDirection_LeftLeft, 0},
    {SDL_SCANCODE_D, 0, 0, EPcStickDirection_LeftRight, EPcStickDirection_LeftRight, 0},
    {SDL_SCANCODE_SPACE, EPcButton_Face2, EPcButton_Face2, 0, 0, 0},
    {SDL_SCANCODE_RETURN, EPcButton_Face2, EPcButton_Face2, 0, 0, 0},
    {SDL_SCANCODE_KP_ENTER, EPcButton_Face2, EPcButton_Face2, 0, 0, 0},
    {SDL_SCANCODE_LSHIFT, EPcButton_RightStickPress, 0, 0, 0, 0},
    {SDL_SCANCODE_RSHIFT, EPcButton_RightStickPress, 0, 0, 0, 0},
    {SDL_SCANCODE_E, EPcButton_Face4, EPcButton_Face1, 0, 0, 0},
    {SDL_SCANCODE_Q, EPcButton_Face1, EPcButton_Face3, 0, 0, 0},
    {SDL_SCANCODE_R, 0, EPcButton_Face4, 0, 0, 0},
    {SDL_SCANCODE_C, EPcButton_Face3, 0, 0, 0, 0},
    {SDL_SCANCODE_F5, EPcButton_LeftStickPress, EPcButton_LeftStickPress, 0, 0, 0},
    {SDL_SCANCODE_ESCAPE, EPcButton_Start, EPcButton_Face1, 0, 0, 0},
    {SDL_SCANCODE_BACKSPACE, 0, EPcButton_Face1, 0, 0, 0},
    {SDL_SCANCODE_TAB, EPcButton_Back, EPcButton_Back, 0, 0, 0},
    {SDL_SCANCODE_UP, EPcButton_DPadUp, EPcButton_DPadUp, 0, 0, 0},
    {SDL_SCANCODE_DOWN, EPcButton_DPadDown, EPcButton_DPadDown, 0, 0, 0},
    {SDL_SCANCODE_LEFT, EPcButton_DPadLeft, EPcButton_DPadLeft, 0, 0, 0},
    {SDL_SCANCODE_RIGHT, EPcButton_DPadRight, EPcButton_DPadRight, 0, 0, 0},
    {SDL_SCANCODE_PAGEUP, 0, EPcButton_LeftTrigger, 0, 0, 0},
    {SDL_SCANCODE_PAGEDOWN, 0, EPcButton_RightTrigger, 0, 0, 0},
    {SDL_SCANCODE_1, 0, 0, 0, 0, 1},
    {SDL_SCANCODE_2, 0, 0, 0, 0, 2},
    {SDL_SCANCODE_3, 0, 0, 0, 0, 3},
    {SDL_SCANCODE_4, 0, 0, 0, 0, 4},
    {SDL_SCANCODE_5, 0, 0, 0, 0, 5},
    {SDL_SCANCODE_6, 0, 0, 0, 0, 6},
    {SDL_SCANCODE_7, 0, 0, 0, 0, 7},
    {SDL_SCANCODE_8, 0, 0, 0, 0, 8},
    {SDL_SCANCODE_9, 0, 0, 0, 0, 9}};

const PcInputBinding s_mouseBindings[] = {{SDL_BUTTON_LEFT, EPcButton_RightTrigger, 0, 0, 0, 0},
                                          {SDL_BUTTON_RIGHT, EPcButton_LeftTrigger, 0, 0, 0, 0}};

const PcGamepadBinding s_gamepadBindings[] = {{SDL_GAMEPAD_BUTTON_START, EPcButton_Start},
                                              {SDL_GAMEPAD_BUTTON_BACK, EPcButton_Back},
                                              {SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, EPcButton_RightBumper},
                                              {SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, EPcButton_LeftBumper},
                                              {SDL_GAMEPAD_BUTTON_RIGHT_STICK, EPcButton_RightStickPress},
                                              {SDL_GAMEPAD_BUTTON_LEFT_STICK, EPcButton_LeftStickPress},
                                              {SDL_GAMEPAD_BUTTON_DPAD_UP, EPcButton_DPadUp},
                                              {SDL_GAMEPAD_BUTTON_DPAD_DOWN, EPcButton_DPadDown},
                                              {SDL_GAMEPAD_BUTTON_DPAD_LEFT, EPcButton_DPadLeft},
                                              {SDL_GAMEPAD_BUTTON_DPAD_RIGHT, EPcButton_DPadRight}};

const int s_gamepadFaceButtons[] = {SDL_GAMEPAD_BUTTON_SOUTH, SDL_GAMEPAD_BUTTON_EAST, SDL_GAMEPAD_BUTTON_WEST,
                                    SDL_GAMEPAD_BUTTON_NORTH};

const PcGamepadLabelBinding s_gamepadLabelBindings[] = {{SDL_GAMEPAD_BUTTON_LABEL_A, EPcButton_Face2},
                                                        {SDL_GAMEPAD_BUTTON_LABEL_B, EPcButton_Face1},
                                                        {SDL_GAMEPAD_BUTTON_LABEL_X, EPcButton_Face4},
                                                        {SDL_GAMEPAD_BUTTON_LABEL_Y, EPcButton_Face3},
                                                        {SDL_GAMEPAD_BUTTON_LABEL_CROSS, EPcButton_Face2},
                                                        {SDL_GAMEPAD_BUTTON_LABEL_CIRCLE, EPcButton_Face1},
                                                        {SDL_GAMEPAD_BUTTON_LABEL_SQUARE, EPcButton_Face4},
                                                        {SDL_GAMEPAD_BUTTON_LABEL_TRIANGLE, EPcButton_Face3}};
}

const PcInputBinding* PcInputKeyBindings(int& count)
{
    count = static_cast<int>(sizeof(s_keyBindings) / sizeof(s_keyBindings[0]));
    return s_keyBindings;
}

const PcInputBinding* PcInputMouseBindings(int& count)
{
    count = static_cast<int>(sizeof(s_mouseBindings) / sizeof(s_mouseBindings[0]));
    return s_mouseBindings;
}

const PcGamepadBinding* PcInputGamepadBindings(int& count)
{
    count = static_cast<int>(sizeof(s_gamepadBindings) / sizeof(s_gamepadBindings[0]));
    return s_gamepadBindings;
}

const int* PcInputGamepadFaceButtons(int& count)
{
    count = static_cast<int>(sizeof(s_gamepadFaceButtons) / sizeof(s_gamepadFaceButtons[0]));
    return s_gamepadFaceButtons;
}

const PcGamepadLabelBinding* PcInputGamepadLabelBindings(int& count)
{
    count = static_cast<int>(sizeof(s_gamepadLabelBindings) / sizeof(s_gamepadLabelBindings[0]));
    return s_gamepadLabelBindings;
}
