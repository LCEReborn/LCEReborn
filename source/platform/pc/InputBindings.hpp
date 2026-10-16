#ifndef PLATFORM_PC_INPUTBINDINGS_HPP
#define PLATFORM_PC_INPUTBINDINGS_HPP

enum EPcButton
{
    EPcButton_Face1 = 0x1,
    EPcButton_Face2 = 0x2,
    EPcButton_Face3 = 0x4,
    EPcButton_Face4 = 0x8,
    EPcButton_Start = 0x10,
    EPcButton_Back = 0x20,
    EPcButton_RightBumper = 0x40,
    EPcButton_LeftBumper = 0x80,
    EPcButton_RightStickPress = 0x100,
    EPcButton_LeftStickPress = 0x200,
    EPcButton_DPadUp = 0x400,
    EPcButton_DPadDown = 0x800,
    EPcButton_DPadLeft = 0x1000,
    EPcButton_DPadRight = 0x2000,
    EPcButton_LeftStickRight = 0x4000,
    EPcButton_LeftStickLeft = 0x8000,
    EPcButton_RightStickDown = 0x10000,
    EPcButton_RightStickUp = 0x20000,
    EPcButton_RightStickRight = 0x40000,
    EPcButton_RightStickLeft = 0x80000,
    EPcButton_LeftStickDown = 0x100000,
    EPcButton_LeftStickUp = 0x200000,
    EPcButton_RightTrigger = 0x400000,
    EPcButton_LeftTrigger = 0x800000
};

struct PcGamepadBinding
{
    int button;
    unsigned int buttons;
};

struct PcGamepadLabelBinding
{
    int label;
    unsigned int buttons;
};

const PcGamepadBinding* PcInputGamepadBindings(int& count);
const int* PcInputGamepadFaceButtons(int& count);
const PcGamepadLabelBinding* PcInputGamepadLabelBindings(int& count);

#endif
