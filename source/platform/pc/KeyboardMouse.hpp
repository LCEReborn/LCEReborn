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
        INPUT_COUNT = INPUT_WHEEL_DOWN + 1,
        HOTBAR_SIZE = 9
    };

    PcKeyboardMouse();

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
    void MovePointer(float x, float y);
    unsigned int ConsumeClicks();
    void AddClicks(unsigned int clicks);
    void SetTouchInput(const bool* down, const bool* pressed, float moveX, float moveY, bool moving);
    bool QuickMoveHeld() const;
    bool PrimaryHeld() const;
    void DiscardRelativeInput();

private:
    struct ActionState
    {
        bool down;
        bool pressed;
        bool released;
        unsigned long long pressTime;
        unsigned long long nextRepeat;
        bool repeatStarted;
    };

    bool CaptureEvent(const SDL_Event& event);
    void PressInput(int input, EPcInputContext context);
    void ReleaseInput(int input);
    void AddBinding(EPcInputContext context, int action, int input);

    std::vector<int> mBindings[EPcInputContext_Count][ACTION_COUNT];
    bool mInputDown[INPUT_COUNT];
    bool mInputPressed[INPUT_COUNT];
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
    bool mTouchDown[ACTION_COUNT];
    bool mTouchPressed[ACTION_COUNT];
    float mTouchMoveX;
    float mTouchMoveY;
    bool mTouchMoving;
    unsigned long long mRepeatDelay;
    bool mCapturing;
    int mCaptured;
};

#endif
