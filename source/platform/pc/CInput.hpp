#ifndef PLATFORM_PC_CINPUT_HPP
#define PLATFORM_PC_CINPUT_HPP

#include "platform/C4JInput.hpp"
#include "platform/pc/KeyboardMouse.hpp"
#include "platform/pc/PcInputEvents.hpp"
#include "platform/pc/PcTouch.hpp"

#include <string>
#include <vector>

struct SDL_Gamepad;
struct SDL_Window;

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
    virtual bool ConsumeMouseLook(int pad, float* x, float* y);
    virtual bool ConsumeMenuPointer(int pad, float* x, float* y);
    virtual unsigned int ConsumeMenuClicks(int pad);
    virtual bool MenuQuickMoveHeld(int pad);
    virtual bool MenuPointerHeld(int pad);
    virtual bool ConsumeHotbarInput(int pad, int* slot, int* scroll);
    virtual bool GetMenuPointer(int pad, float* x, float* y);
    virtual int ConsumeMenuWheel(int pad);
    virtual bool UseKeyboardPrompts(int pad);
    virtual void SetMenuDrawsPointer(bool drawsPointer);
    virtual int GetPromptStyle(int pad);
    virtual int GetPromptImages(int pad, const int* actions, int actionCount, unsigned int buttons, int* images,
                                int maxImages);
    virtual int GetTouchPromptImages(int pad, const int* actions, int actionCount, int* images, int maxImages);
    virtual bool UsesKeyboard(int pad);
    int LastInputPad();
    virtual void BeginKeyCapture();
    virtual void CancelKeyCapture();
    virtual int ConsumeCapturedKey();
    virtual bool HasKeyBinding(int action);
    virtual bool SameKeyBinding(int action, int otherAction);
    virtual void SetKeyBinding(int action, int input);
    virtual void ResetKeyBindings();
    virtual unsigned int GetPadButtons(int pad, bool pressedOnly);
    virtual bool ShowsJoinPromptOnLoad();
    virtual void PlaceMenuPointer(int pad, float x, float y);
    virtual void BeginTextCapture(int pad);
    virtual void EndTextCapture();
    virtual bool ConsumeCapturedText(std::wstring* text);
    virtual int ConsumeTextKey();
    virtual bool UsesTouch(int pad);
    virtual bool HasTouchscreen();
    virtual void SetTouchHud(int pad, const TouchHud& hud);
    virtual bool GetTouchAim(int pad, float* x, float* y);
    virtual void SetTouchAimTarget(int pad, int target);
    virtual void SetTouchInteract(int pad, const wchar_t* label);
    virtual bool ConsumeTouchLook(int pad, float* x, float* y);
    virtual bool GetMenuHold(int pad, float* progress);
    virtual float ConsumeMenuSplit(int pad);
    virtual void SetMenuHoldAmount(int pad, int amount, int total);

private:
    friend void PcInputHandleEvent(const SDL_Event& event);
    friend bool PcInputGetKeyboardText(std::wstring* title, std::wstring* text);
    friend void PcInputDrawTouch(int width, int height);

    enum
    {
        JOYPAD_COUNT = 5,
        BUTTON_COUNT = 24,
        KEYBOARD_TEXT_CAPACITY = 258
    };

    enum EKeyboardState
    {
        EKeyboardState_Idle = 0,
        EKeyboardState_Requested = 1,
        EKeyboardState_Active = 2
    };

    enum EErrorViewerState
    {
        EErrorViewerState_None = 0,
        EErrorViewerState_Requested = 1,
        EErrorViewerState_Active = 2,
        EErrorViewerState_Destroyed = 3
    };

    struct Joypad
    {
        bool connected;
        bool disconnected;
        bool newlyConnected;
        bool consumed;
        bool previousConsumed;
        unsigned int touchValue;
        unsigned int rawValue;
        unsigned int fakeValue;
        unsigned int fakeTimer;
        unsigned int previousValue;
        unsigned int pressedValue;
        unsigned int releasedValue;
        float leftX;
        float leftY;
        float rightX;
        float rightY;
        unsigned char leftTrigger;
        unsigned char rightTrigger;
        float* leftXSource;
        float* leftYSource;
        float* rightXSource;
        float* rightYSource;
        float sensitivity;
        unsigned char* leftTriggerSource;
        unsigned char* rightTriggerSource;
        int lockedController;
        SDL_Gamepad* gamepad;
        unsigned int gamepadId;
        unsigned int padButtons;
        unsigned int padFaceButtons[4];
        int padLeftX;
        int padLeftY;
        int padRightX;
        int padRightY;
        unsigned char padLeftTrigger;
        unsigned char padRightTrigger;

        void setConnected(bool value);
    };

    struct ButtonTime
    {
        unsigned long long lastUpdate;
        float heldTime;
        float repeatTime;
        bool repeated;
    };

    struct VerifyRequest
    {
        int (*callback)(void*, _STRING_VERIFY_RESPONSE*);
        void* context;
        int count;
    };

    void HandleEvent(const SDL_Event& event);
    bool FromTouch(const SDL_Event& event) const;
    void HandleFinger(const SDL_Event& event);
    PcTouch::EContext TouchContext() const;
    void AddGamepad(unsigned int gamepadId);
    void RemoveGamepad(unsigned int gamepadId);
    void ResolveFaceButtons(Joypad& joypad);
    void ReleaseAllKeys();
    bool IsKeyboardInMenu() const;
    int KeyboardPlayer() const;
    bool IsKeyboardPad(int pad);
    int PromptJoypad(int pad);
    int PadFamily(int joypad);
    int PadElement(int joypad, unsigned int button);
    bool MenuPointerPosition(float* x, float* y);
    SDL_Window* LocateWindow() const;
    bool RemapIpad(int& pad);
    unsigned int only_one_bit_set(unsigned int value);
    unsigned int allowInput(int pad, unsigned char map, unsigned int value);
    void ReadAllPads();
    void ReadGamepad(Joypad& joypad);
    void ClearJoypadValues(Joypad* joypad);
    void SetJoypadValues(int pad, Joypad* joypad);
    void ApplyStickValues(Joypad* joypad, int leftX, int leftY, int rightX, int rightY);
    void ConsumeAllPads();
    bool IsMenuShown(int pad) const;
    void UpdateMouseCapture();
    void WarpPointer(SDL_Window* window, float x, float y);
    void UpdateJoypads();
    void FollowDeviceInUse();
    void UpdateKeyboardMouse(Joypad& joypad);
    void UpdateTouch();
    EPcInputContext KeyboardContext() const;
    unsigned int KeyboardActionValue(int pad, unsigned char map);
    void GetStartTime(int pad, int button);
    void UpdateTime(int pad, int button);
    void TickErrorViewer();
    void TickKeyboard();
    void FinishKeyboard(bool accepted);
    void HandleKeyboardTextInput(const char* text);
    void HandleKeyboardKey(int scancode, unsigned short modifiers, bool repeat);
    void HandleTextCaptureKey(int scancode, unsigned short modifiers, bool repeat);
    void AppendCapturedText(const char* text);
    bool AppendKeyboardCodepoint(unsigned int codepoint);
    void TickVerifyStrings();

    Joypad joypads[JOYPAD_COUNT];
    int padRemap[4];
    unsigned int controllerCount;
    int lastInputJoypad;
    unsigned int deadzone;
    unsigned int maximumInput;
    unsigned int movementRange;
    unsigned int movementSpan;
    float movementSpanFloat;
    unsigned char controllerConfiguration[3];
    unsigned int* gameJoypadMaps[JOYPAD_COUNT];
    unsigned int* alternateJoypadMaps[JOYPAD_COUNT];
    bool joypadMapsReady;
    bool initialised;
    unsigned char menuDisplayed[4];
    unsigned int inputTick;
    float initialRepeatDelay;
    float repeatDelay;
    bool circleCrossSwapped;
    ButtonTime buttonTimes[JOYPAD_COUNT][BUTTON_COUNT];
    unsigned long long idleStartTimes[JOYPAD_COUNT];
    std::string debugSequence;
    InputCallback debugSequenceCallback;
    void* debugSequenceContext;
    bool localMultiplayer;

    PcKeyboardMouse keyboardMouse;
    PcTouch touch;
    std::string keyBindingsPath;
    unsigned short touchData[4];
    bool mouseLookEnabled;
    bool mouseCaptured;
    bool pointerPlacementPending;
    float pointerPlacementX;
    float pointerPlacementY;
    bool menuDrawsPointer;
    bool cursorShown;
    bool windowFocused;
    bool textCapture;
    unsigned int textCaptureWindowId;
    std::wstring capturedText;
    std::vector<int> capturedKeys;
    unsigned int lastWindowId;

    EKeyboardState keyboardState;
    KeyboardCallback keyboardCallback;
    void* keyboardContext;
    int keyboardMaximumLength;
    int keyboardPad;
    EKeyboardMode keyboardMode;
    wchar_t keyboardText[KEYBOARD_TEXT_CAPACITY];
    std::vector<wchar_t> keyboardEditText;
    std::wstring keyboardTitle;
    bool keyboardAccepted;
    bool keyboardCancelled;
    bool destroyKeyboardRequested;
    unsigned int keyboardWindowId;

    EErrorViewerState errorViewerState;
    ErrorViewerCallback errorViewerCallback;
    void* errorViewerContext;
    unsigned int errorViewerController;
    unsigned int errorViewerCode;
    std::wstring errorViewerTitle;
    std::wstring errorViewerMessage;
    bool errorViewerHasText;

    std::vector<VerifyRequest> verifyRequests;
};

extern CInput* g_pInput;
extern CInput* g_pInputInstance;

int XEnableGuestSignin(int enable);

#endif
