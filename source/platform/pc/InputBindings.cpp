#include "platform/pc/InputBindings.hpp"

#include <SDL3/SDL.h>

namespace
{
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
