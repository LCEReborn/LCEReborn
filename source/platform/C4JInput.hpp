#ifndef PLATFORM_C4JINPUT_HPP
#define PLATFORM_C4JINPUT_HPP

struct _STRING_VERIFY_RESPONSE;

class C_4JInput
{
public:
#if LCE_PLATFORM_PC
    enum EPcMenuClick
    {
        EPcMenuClick_Primary = 0x1,
        EPcMenuClick_Secondary = 0x2,
        EPcMenuClick_Quick = 0x4
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
        EKeyboardMode_0 = 0
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
#endif
};

#endif
