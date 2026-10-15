#ifndef PLATFORM_PC_CINPUT_HPP
#define PLATFORM_PC_CINPUT_HPP

#include "platform/C4JInput.hpp"

class CInput : public C_4JInput
{
public:
    CInput();
    virtual ~CInput();

    virtual void Initialise(int controllerCount, unsigned char gameMapCount, unsigned char menuMapCount);
    virtual void Tick();
    virtual void SetDeadzoneAndMovementRange(unsigned int deadzone, unsigned int maximum,
                                             unsigned int movementRange);
    virtual void SetGameControllerTypeJoypadMaps(int controllerType, unsigned char map, unsigned int value);
    virtual void SetGameJoypadMaps(int pad, unsigned char map, unsigned int value);
    virtual unsigned int GetGameJoypadMaps(int pad, unsigned char map);
    virtual void SetJoypadMapArraySetupComplete();
    virtual void SetJoypadSensitivity(int pad, float sensitivity);
    virtual unsigned int GetValue(int value, unsigned char type, bool flags);
    virtual void SetFakePadValue(int pad, unsigned char map);
    virtual bool ButtonPressed(int pad, unsigned char map, unsigned int mask);
    virtual bool ButtonReleased(int pad, unsigned char map);
    virtual bool ButtonDown(int pad, unsigned char map);
    virtual void SetJoypadStickAxisMap(int axis, unsigned int negativeMask, unsigned int positiveMask);
    virtual void SetJoypadStickTriggerMap(int trigger, unsigned int mask, unsigned int threshold);
    virtual void SetKeyRepeatRate(float initialDelay, float repeatDelay);
    virtual void SetDebugSequence(const char* sequence, InputCallback callback, void* context);
    virtual double GetIdleSeconds(int pad);
    virtual unsigned char IsPadConnected(int pad);
    virtual void SetCircleCrossSwapped(bool swapped);
    virtual unsigned char IsCircleCrossSwapped();
    virtual void MapTouchInput(int pad, unsigned int mask);
    virtual float GetJoypadStick_Menu_LX(int value);
    virtual float GetJoypadStick_Menu_LY(int value);
    virtual float GetJoypadStick_Menu_RX(int value);
    virtual float GetJoypadStick_Menu_RY(int value);
    virtual unsigned char GetJoypadLTrigger_Menu(int value);
    virtual unsigned char GetJoypadRTrigger_Menu(int value);
    virtual float GetJoypadStick_LX(int pad, bool checkMenu);
    virtual float GetJoypadStick_LY(int pad, bool checkMenu);
    virtual float GetJoypadStick_RX(int pad, bool checkMenu);
    virtual float GetJoypadStick_RY(int pad, bool checkMenu);
    virtual unsigned char GetJoypadLTrigger(int pad, bool raw);
    virtual unsigned char GetJoypadRTrigger(int pad, bool raw);
    virtual unsigned short* GetTouchPadData(int pad, ETouchType touchType, bool previous);
    virtual void SetMenuDisplayed(int pad, bool displayed);
    virtual unsigned int RequestKeyboard(const wchar_t* title, const wchar_t* initialText, unsigned int pad,
                                         unsigned int maximumLength, KeyboardCallback callback, void* context,
                                         EKeyboardMode mode);
    virtual void RequestErrorViewer(unsigned int value, unsigned int type, ErrorViewerCallback callback,
                                    void* context);
    virtual void RequestErrorViewer(unsigned int value, wchar_t* title, wchar_t* message, ErrorViewerCallback callback,
                                    void* context);
    virtual void DestroyErrorViewer();
    virtual void DestroyKeyboard();
    virtual void GetText(wchar_t* text, int length);
    virtual void SetLocalMultiplayer(bool enabled);
    virtual unsigned int IsLocalMultiplayerAvailable();
    virtual unsigned int UsingRemoteVita();
    virtual unsigned int VerifyStrings(wchar_t** strings, int count, int (*callback)(void*, _STRING_VERIFY_RESPONSE*),
                                       void* context);
    virtual void CancelQueuedVerifyStrings(int (*callback)(void*, _STRING_VERIFY_RESPONSE*), void* context);
    virtual void CancelAllVerifyInProgress();
    virtual unsigned int LockPad(int pad, int controllerId);
    virtual int UnlockPad(int pad);
    virtual unsigned int IsPadLocked(int pad);
    virtual int GetControllerID(int pad);
    virtual unsigned char IsPadDRCController(int pad);
    virtual unsigned int IsPadClassicController(int pad);
    virtual unsigned char IsDRCConnected();
    virtual void AcquiredForeground();
};

extern CInput* g_pInput;
extern CInput* g_pInputInstance;

int XEnableGuestSignin(int enable);

#endif
