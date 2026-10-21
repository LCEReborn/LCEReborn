#ifndef PLATFORM_PC_KEYBOARDMOUSE_HPP
#define PLATFORM_PC_KEYBOARDMOUSE_HPP

#include <string>
#include <vector>

union SDL_Event;

enum EPcInputContext
{
    EPcInputContext_Menu = 0,
    EPcInputContext_Game = 1,
    EPcInputContext_Count = 2
};

class PcKeyboardMouse
{
public:
    enum
    {
        ACTION_COUNT = 72,
        SCANCODE_COUNT = 512,
        MOUSE_BUTTON_COUNT = 8,
        INPUT_WHEEL_UP = SCANCODE_COUNT + MOUSE_BUTTON_COUNT,
        INPUT_WHEEL_DOWN = INPUT_WHEEL_UP + 1,
        INPUT_WHEEL_LEFT = INPUT_WHEEL_DOWN + 1,
        INPUT_WHEEL_RIGHT = INPUT_WHEEL_LEFT + 1,
        INPUT_COUNT = INPUT_WHEEL_RIGHT + 1,
        INPUT_MASK = 0xffff,
        MOD_CTRL = 0x10000,
        MOD_SHIFT = 0x20000,
        MOD_ALT = 0x40000,
        MOD_MASK = MOD_CTRL | MOD_SHIFT | MOD_ALT,
        HOTBAR_SIZE = 9
    };

    PcKeyboardMouse();

    void LoadSettings(const std::string& path);
    bool SaveSettings(const std::string& path) const;
    int GetSetting(int setting) const;
    void SetSetting(int setting, int value);
    void LoadBindings(const std::string& path);
    bool SaveBindings(const std::string& path) const;
    void SetDefaultBindings();
    const std::vector<int>& GetBindings(EPcInputContext context, int action) const;
    void SetBindings(EPcInputContext context, int action, const std::vector<int>& inputs);
    static std::string InputName(int input);
    static std::string DisplayName(int input);
    static EPcInputContext ActionContext(int action);
    static bool PointerPromptInput(int action, int* input, int* modifier);
    static bool IsLookAction(int action);
    static int InputFromName(const std::string& name);
    static const char* ActionName(int action);

    void HandleEvent(const SDL_Event& event, EPcInputContext context, bool mouseCaptured);
    void BeginCapture();
    void CancelCapture();
    int ConsumeCapture();
    void SetContext(EPcInputContext context);
    void SetToggleSprint(bool enabled);
    void Update(EPcInputContext context, float initialRepeatDelay, float repeatDelay);
    void ReleaseAll();
    void SetFakeAction(int action, int ticks);

    bool ActionDown(int action) const;
    bool ActionPressed(int action) const;
    bool ActionReleased(int action) const;
    bool ActionRepeat(int action);
    bool AnyInputPressed() const;
    bool AnyInputDown() const;
    float MoveX() const;
    float MoveY() const;

    bool ConsumeLook(float* x, float* y);
    bool ConsumeHotbar(int* slot, int* scroll);
    int ConsumeMenuWheel();
    bool PointerMoved() const;
    bool MouseMovedThisTick() const;
    void ClearPointerMoved();
    float PointerX() const;
    float PointerY() const;
    void SetPointer(float x, float y);
    unsigned int ConsumeClicks();
    void DiscardRelativeInput();

private:
    struct ActionState
    {
        int held;
        bool pendingPressed;
        bool pendingReleased;
        bool down;
        bool pressed;
        bool released;
        unsigned long long pressTime;
        unsigned long long nextRepeat;
        bool repeatStarted;
    };

    bool CaptureEvent(const SDL_Event& event);
    bool QuickMoveHeld() const;
    struct BindingTarget
    {
        int action;
        int modifiers;
    };

    void PressInput(int input, unsigned long long timestamp);
    void ReleaseInput(int input, unsigned long long timestamp);
    void RebuildBindingIndex();
    void RefreshInput(int input, unsigned long long timestamp, bool activate = true);
    void RefreshModifiers(unsigned long long timestamp);
    void ChangeAction(int action, int change, unsigned long long timestamp);
    int Modifiers() const;
    static int ModifierForInput(int input);
    void Wheel(float x, float y, unsigned long long timestamp);
    bool PulseInput(int input, unsigned long long timestamp);
    void AddBinding(EPcInputContext context, int action, int input);

    std::vector<int> mBindings[EPcInputContext_Count][ACTION_COUNT];
    bool mInputDown[INPUT_COUNT];
    std::vector<BindingTarget> mBindingIndex[EPcInputContext_Count][INPUT_COUNT];
    std::vector<int> mActiveActions[INPUT_COUNT];
    EPcInputContext mContext;
    bool mPendingAnyPressed;
    int mMouseSensitivity;
    bool mToggleSneak;
    bool mToggleSprint;
    bool mSprintLatched;
    float mWheelX;
    float mWheelY;
    int mCaptureModifier;
    EPcInputContext mInputContext[INPUT_COUNT];
    ActionState mActions[ACTION_COUNT];
    int mFakeTicks[ACTION_COUNT];
    bool mAnyPressed;
    float mMoveX;
    float mMoveY;
    float mLookX;
    float mLookY;
    int mHotbarSlot;
    int mHotbarScroll;
    int mMenuWheel;
    float mPointerX;
    float mPointerY;
    bool mPointerMoved;
    float mMotionSinceTick;
    bool mMovedThisTick;
    unsigned int mClicks;
    unsigned long long mRepeatDelay;
    bool mCapturing;
    int mCaptured;
};

#endif
