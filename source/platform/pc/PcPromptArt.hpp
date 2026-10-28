#ifndef PLATFORM_PC_PCPROMPTART_HPP
#define PLATFORM_PC_PCPROMPTART_HPP

enum EPcPadFamily
{
    EPcPadFamily_Xbox,
    EPcPadFamily_PlayStation3,
    EPcPadFamily_PlayStation4,
    EPcPadFamily_PlayStation5,
    EPcPadFamily_Nintendo,
    EPcPadFamily_Generic,
    EPcPadFamily_Count
};

enum EPcPadElement
{
    EPcPadElement_FaceA,
    EPcPadElement_FaceB,
    EPcPadElement_FaceX,
    EPcPadElement_FaceY,
    EPcPadElement_FaceCross,
    EPcPadElement_FaceCircle,
    EPcPadElement_FaceSquare,
    EPcPadElement_FaceTriangle,
    EPcPadElement_LeftShoulder,
    EPcPadElement_RightShoulder,
    EPcPadElement_LeftTrigger,
    EPcPadElement_RightTrigger,
    EPcPadElement_LeftStick,
    EPcPadElement_RightStick,
    EPcPadElement_LeftStickPress,
    EPcPadElement_RightStickPress,
    EPcPadElement_Start,
    EPcPadElement_Back,
    EPcPadElement_DPadUp,
    EPcPadElement_DPadDown,
    EPcPadElement_DPadLeft,
    EPcPadElement_DPadRight,
    EPcPadElement_Count
};

enum
{
    PC_PROMPT_ART_MOUSE = 1000,
    PC_PROMPT_ART_PAD_BASE = 1024,
    PC_PROMPT_ART_TOUCH_JOYSTICK = 2000,
    PC_PROMPT_ART_TOUCH_BASE = 2048,
    PC_PROMPT_ART_TOUCH_END = 2112
};

int PcPadPromptArt(EPcPadFamily family, EPcPadElement element);
bool PcPromptArtSize(int art, int* width, int* height);
bool PcPromptArtPixels(int art, int* width, int* height, const unsigned char** rgba);
int PcPromptArtTexture(int art);

#endif
