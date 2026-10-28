#ifndef PLATFORM_C4JINPUT_HPP
#define PLATFORM_C4JINPUT_HPP

#if LCE_PLATFORM_PC
#include <string>
#endif

struct _STRING_VERIFY_RESPONSE;

class C_4JInput
{
public:
#if LCE_PLATFORM_PC
    enum EPcMenuClick
    {
        EPcMenuClick_Primary = 0x1,
        EPcMenuClick_Secondary = 0x2,
        EPcMenuClick_Quick = 0x4,
        EPcMenuClick_DragStart = 0x8,
        EPcMenuClick_DragEnd = 0x10,
        EPcMenuClick_Auto = 0x20,
        EPcMenuClick_Split = 0x40
    };
    enum ETextKey
    {
        ETextKey_None = -1,
        ETextKey_Backspace,
        ETextKey_Delete,
        ETextKey_Enter,
        ETextKey_Escape,
        ETextKey_Tab,
        ETextKey_Up,
        ETextKey_Down,
        ETextKey_Left,
        ETextKey_Right,
        ETextKey_Home,
        ETextKey_End,
        ETextKey_PageUp,
        ETextKey_PageDown,
        ETextKey_WordLeft,
        ETextKey_WordRight,
        ETextKey_WordBackspace
    };
    enum EPromptStyle
    {
        EPromptStyle_Keyboard,
        EPromptStyle_Pointer,
        EPromptStyle_Touch,
        EPromptStyle_Gamepad
    };
    enum ETouchScheme
    {
        ETouchScheme_JoystickTap,
        ETouchScheme_JoystickCrosshair,
        ETouchScheme_DPadTap,
        ETouchScheme_Count
    };
    enum ETouchHud
    {
        ETouchHud_Flying = 0x1,
        ETouchHud_Swimming = 0x2,
        ETouchHud_Riding = 0x4,
        ETouchHud_Sneaking = 0x8,
        ETouchHud_HoldToUse = 0x10,
        ETouchHud_Spectator = 0x20,
        ETouchHud_Hotbar = 0x40,
        ETouchHud_HoldToSneak = 0x80,
        ETouchHud_Survival = 0x100,
        ETouchHud_CanSprint = 0x200
    };
    enum ETouchOption
    {
        ETouchOption_PickBlock = 0x1,
        ETouchOption_LeftHanded = 0x2,
        ETouchOption_EasySprint = 0x4,
        ETouchOption_Perspective = 0x8,
        ETouchOption_SwapJumpSneak = 0x10
    };
    enum ETouchJoystick
    {
        ETouchJoystick_Visible,
        ETouchJoystick_HiddenWhenUnused,
        ETouchJoystick_Hidden,
        ETouchJoystick_Count
    };
    struct TouchHud
    {
        unsigned int flags;
        int scheme;
        unsigned int options;
        int joystick;
        float opacity;
        float hotbarX;
        float hotbarY;
        float hotbarWidth;
        float hotbarHeight;
    };
    enum ETouchAim
    {
        ETouchAim_None,
        ETouchAim_Block,
        ETouchAim_Entity
    };
    enum EKeyCapture
    {
        EKeyCapture_None = -1,
        EKeyCapture_Cancelled = -2
    };
#endif
    enum ETouchType
    {
        ETouchType_0 = 0,
        ETouchType_1 = 1,
        ETouchType_2 = 2
    };

    enum EKeyboardMode
    {
        EKeyboardMode_0 = 0,
        EKeyboardMode_Chat = 8
    };

    typedef int (*ErrorViewerCallback)(void*);
    typedef int (*InputCallback)(void*);
    typedef int (*KeyboardCallback)(void*, bool);

    virtual void Initialise(int controllerCount, unsigned char gameMapCount, unsigned char menuMapCount) = 0;
    virtual void Tick() = 0;
    virtual void SetDeadzoneAndMovementRange(unsigned int deadzone, unsigned int maximum,
                                             unsigned int movementRange) = 0;
    virtual void SetGameControllerTypeJoypadMaps(int controllerType, unsigned char map, unsigned int value) = 0;
    virtual void SetGameJoypadMaps(int pad, unsigned char map, unsigned int value) = 0;
    virtual unsigned int GetGameJoypadMaps(int pad, unsigned char map) = 0;
    virtual void SetJoypadMapArraySetupComplete() = 0;
    virtual void SetJoypadSensitivity(int pad, float sensitivity) = 0;
    virtual unsigned int GetValue(int value, unsigned char type, bool flags) = 0;
    virtual void SetFakePadValue(int pad, unsigned char map) = 0;
    virtual bool ButtonPressed(int pad, unsigned char map, unsigned int mask) = 0;
    virtual bool ButtonReleased(int pad, unsigned char map) = 0;
    virtual bool ButtonDown(int pad, unsigned char map) = 0;
    virtual void SetJoypadStickAxisMap(int axis, unsigned int negativeMask, unsigned int positiveMask) = 0;
    virtual void SetJoypadStickTriggerMap(int trigger, unsigned int mask, unsigned int threshold) = 0;
    virtual void SetKeyRepeatRate(float initialDelay, float repeatDelay) = 0;
    virtual void SetDebugSequence(const char* sequence, InputCallback callback, void* context) = 0;
    virtual double GetIdleSeconds(int pad) = 0;
    virtual unsigned char IsPadConnected(int pad) = 0;
    virtual void SetCircleCrossSwapped(bool swapped) = 0;
    virtual unsigned char IsCircleCrossSwapped() = 0;
    virtual void MapTouchInput(int pad, unsigned int mask) = 0;
    virtual float GetJoypadStick_Menu_LX(int value) = 0;
    virtual float GetJoypadStick_Menu_LY(int value) = 0;
    virtual float GetJoypadStick_Menu_RX(int value) = 0;
    virtual float GetJoypadStick_Menu_RY(int value) = 0;
    virtual unsigned char GetJoypadLTrigger_Menu(int value) = 0;
    virtual unsigned char GetJoypadRTrigger_Menu(int value) = 0;
    virtual float GetJoypadStick_LX(int pad, bool checkMenu) = 0;
    virtual float GetJoypadStick_LY(int pad, bool checkMenu) = 0;
    virtual float GetJoypadStick_RX(int pad, bool checkMenu) = 0;
    virtual float GetJoypadStick_RY(int pad, bool checkMenu) = 0;
    virtual unsigned char GetJoypadLTrigger(int pad, bool raw) = 0;
    virtual unsigned char GetJoypadRTrigger(int pad, bool raw) = 0;
    virtual unsigned short* GetTouchPadData(int pad, ETouchType touchType, bool previous) = 0;
    virtual void SetMenuDisplayed(int pad, bool displayed) = 0;
    virtual unsigned int RequestKeyboard(const wchar_t* title, const wchar_t* initialText, unsigned int pad,
                                         unsigned int maximumLength, KeyboardCallback callback, void* context,
                                         EKeyboardMode mode) = 0;
    virtual void RequestErrorViewer(unsigned int value, unsigned int type, ErrorViewerCallback callback,
                                    void* context) = 0;
    virtual void RequestErrorViewer(unsigned int value, wchar_t* title, wchar_t* message, ErrorViewerCallback callback,
                                    void* context) = 0;
    virtual void DestroyErrorViewer() = 0;
    virtual void DestroyKeyboard() = 0;
    virtual void GetText(wchar_t* text, int length) = 0;
    virtual void SetLocalMultiplayer(bool enabled) = 0;
    virtual unsigned int IsLocalMultiplayerAvailable() = 0;
    virtual unsigned int UsingRemoteVita() = 0;
    virtual unsigned int VerifyStrings(wchar_t** strings, int count, int (*callback)(void*, _STRING_VERIFY_RESPONSE*),
                                       void* context) = 0;
    virtual void CancelQueuedVerifyStrings(int (*callback)(void*, _STRING_VERIFY_RESPONSE*), void* context) = 0;
    virtual void CancelAllVerifyInProgress() = 0;
    virtual unsigned int LockPad(int pad, int controllerId) = 0;
    virtual int UnlockPad(int pad) = 0;
    virtual unsigned int IsPadLocked(int pad) = 0;
    virtual int GetControllerID(int pad) = 0;
    virtual unsigned char IsPadDRCController(int pad) = 0;
    virtual unsigned int IsPadClassicController(int pad) = 0;
    virtual unsigned char IsDRCConnected() = 0;
    virtual void AcquiredForeground() = 0;
#if LCE_PLATFORM_PC
    virtual bool ConsumeMouseLook(int pad, float* x, float* y) = 0;
    virtual bool ConsumeMenuPointer(int pad, float* x, float* y) = 0;
    virtual unsigned int ConsumeMenuClicks(int pad) = 0;
    virtual bool MenuQuickMoveHeld(int pad) = 0;
    virtual bool MenuPointerHeld(int pad) = 0;
    virtual bool ConsumeHotbarInput(int pad, int* slot, int* scroll) = 0;
    virtual bool GetMenuPointer(int pad, float* x, float* y) = 0;
    virtual int ConsumeMenuWheel(int pad) = 0;
    virtual bool UseKeyboardPrompts(int pad) = 0;
    virtual void SetMenuDrawsPointer(bool drawsPointer) = 0;
    virtual int GetPromptStyle(int pad) = 0;
    virtual int GetPromptImages(int pad, const int* actions, int actionCount, unsigned int buttons, int* images,
                                int maxImages) = 0;
    virtual int GetTouchPromptImages(int pad, const int* actions, int actionCount, int* images, int maxImages) = 0;
    virtual bool UsesKeyboard(int pad) = 0;
    virtual void BeginKeyCapture() = 0;
    virtual void CancelKeyCapture() = 0;
    virtual int ConsumeCapturedKey() = 0;
    virtual bool HasKeyBinding(int action) = 0;
    virtual bool SameKeyBinding(int action, int otherAction) = 0;
    virtual void SetKeyBinding(int action, int input) = 0;
    virtual void ResetKeyBindings() = 0;
    virtual unsigned int GetPadButtons(int pad, bool pressedOnly) = 0;
    virtual bool ShowsJoinPromptOnLoad() = 0;
    virtual void PlaceMenuPointer(int pad, float x, float y) = 0;
    virtual void BeginTextCapture(int pad) = 0;
    virtual void EndTextCapture() = 0;
    virtual bool ConsumeCapturedText(std::wstring* text) = 0;
    virtual int ConsumeTextKey() = 0;
    virtual bool UsesTouch(int pad) = 0;
    virtual bool HasTouchscreen() = 0;
    virtual void SetTouchHud(int pad, const TouchHud& hud) = 0;
    virtual bool GetTouchAim(int pad, float* x, float* y) = 0;
    virtual void SetTouchAimTarget(int pad, int target) = 0;
    virtual void SetTouchInteract(int pad, const wchar_t* label) = 0;
    virtual bool ConsumeTouchLook(int pad, float* x, float* y) = 0;
    virtual bool GetMenuHold(int pad, float* progress) = 0;
    virtual float ConsumeMenuSplit(int pad) = 0;
    virtual void SetMenuHoldAmount(int pad, int amount, int total) = 0;
#endif
};

#endif
