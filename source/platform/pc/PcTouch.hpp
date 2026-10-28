#ifndef PLATFORM_PC_PCTOUCH_HPP
#define PLATFORM_PC_PCTOUCH_HPP

#include "platform/pc/KeyboardMouse.hpp"

#include <string>
#include <vector>

union SDL_Event;

class PcTouch
{
public:
    enum EContext
    {
        EContext_Menu,
        EContext_Game
    };

    struct Hud
    {
        unsigned int flags;
        int scheme;
        unsigned int options;
        int joystick;
        float opacity;
        float hotbarLeft;
        float hotbarTop;
        float hotbarWidth;
        float hotbarHeight;
        unsigned long long updated;
    };

    PcTouch();

    void SetScreen(float width, float height);
    void HandleEvent(const SDL_Event& event, EContext context);
    void Update(EContext context, bool containerMenu);
    void Cancel();

    bool InUse() const;
    void SetInUse(bool inUse);
    bool Touching() const;
    bool Seen() const;

    void SetHud(const Hud& hud);
    bool HasHud() const;
    void SetAimTarget(int target);
    void SetInteract(const wchar_t* label);

    void ApplyActions(PcKeyboardMouse& keyboardMouse);
    bool GetAim(float* x, float* y) const;
    bool ConsumeLook(float* x, float* y);
    bool ConsumeHotbar(int* slot);

    bool MenuPointer(float* x, float* y) const;
    bool MenuDown() const;
    bool ConsumeMenuMoved();
    unsigned int ConsumeMenuClicks();
    bool MenuHold(float* progress) const;
    float ConsumeMenuSplit();
    int ConsumeMenuWheel();
    void SetMenuHoldAmount(int amount, int total);

    int PromptArt(int action) const;

    void Draw(int width, int height);
    static void Shutdown();

private:
    enum ERole
    {
        ERole_None,
        ERole_Menu,
        ERole_World,
        ERole_Button,
        ERole_Joystick,
        ERole_DPad,
        ERole_Hotbar
    };

    enum EWorld
    {
        EWorld_Pending,
        EWorld_Looking,
        EWorld_Holding
    };

    enum EMenuGesture
    {
        EMenuGesture_Pending,
        EMenuGesture_Swipe,
        EMenuGesture_Drag,
        EMenuGesture_Hold
    };

    enum EButton
    {
        EButton_Jump,
        EButton_Sneak,
        EButton_Attack,
        EButton_Use,
        EButton_Pause,
        EButton_Chat,
        EButton_Inventory,
        EButton_Crafting,
        EButton_Sprint,
        EButton_PickBlock,
        EButton_Perspective,
        EButton_Interact,
        EButton_Count
    };

    struct Rect
    {
        float x;
        float y;
        float width;
        float height;

        bool Contains(float px, float py) const;
    };

    struct Layout
    {
        float scale;
        float topScale;
        Rect buttons[EButton_Count];
        bool shown[EButton_Count];
        int icons[EButton_Count];
        Rect joystickZone;
        float padX;
        float padY;
        float padSize;
        float travel;
        Rect dpad;
        float dpadCell;
        Rect hotbar;
        bool interactText;
    };

    struct Finger
    {
        unsigned long long id;
        ERole role;
        int button;
        float x;
        float y;
        float startX;
        float startY;
        float lastX;
        float lastY;
        float originX;
        float originY;
        unsigned long long downTime;
        unsigned long long nextDrop;
        EWorld world;
        EMenuGesture menu;
        bool moved;
        bool held;
    };

    struct Pulse
    {
        int action;
        unsigned long long until;
        float x;
        float y;
    };

    struct QueuedPress
    {
        int action;
        unsigned long long at;
    };

    Layout BuildLayout() const;
    bool GameHud() const;
    Finger* FindFinger(unsigned long long id);
    void FingerDown(unsigned long long id, float x, float y, EContext context);
    void FingerMoved(Finger& finger, float x, float y);
    void FingerUp(Finger& finger);
    void PressAction(int action);
    void PulseAt(int action, float x, float y);
    void ButtonDown(int button);
    bool FlyingDown() const;
    bool SneakButtonSneaks() const;
    int DPadCell(const Layout& layout, float x, float y) const;
    void DPadMove(int cell, float* x, float* y) const;
    void UpdateSneakButton(Finger& finger, unsigned long long now);
    void MenuFingerMoved(Finger& finger);
    void MenuFingerUp(Finger& finger);

    float mWidth;
    float mHeight;
    bool mInUse;
    bool mSeen;
    Hud mHud;
    int mAimTarget;
    std::wstring mInteract;
    bool mSprintLatched;
    bool mPickArmed;
    unsigned long long mLastFlyDown;
    std::vector<QueuedPress> mQueued;
    std::vector<Finger> mFingers;
    std::vector<Pulse> mPulses;
    bool mDown[PcKeyboardMouse::ACTION_COUNT];
    bool mPressed[PcKeyboardMouse::ACTION_COUNT];
    unsigned long long mHoldUntil[PcKeyboardMouse::ACTION_COUNT];
    float mMoveX;
    float mMoveY;
    bool mMoving;
    float mLookX;
    float mLookY;
    int mHotbarSlot;
    bool mAiming;
    float mAimX;
    float mAimY;
    bool mMenuDown;
    bool mMenuMoved;
    float mMenuX;
    float mMenuY;
    unsigned int mMenuClicks;
    bool mMenuPinned;
    float mMenuHold;
    float mMenuSplit;
    int mMenuWheel;
    int mHoldAmount;
    int mHoldTotal;
    unsigned long long mLastTap;
    float mLastTapX;
    float mLastTapY;
    EContext mContext;
    bool mContainerMenu;
};

#endif
