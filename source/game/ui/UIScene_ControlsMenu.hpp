#ifndef GAME_UI_UISCENE_CONTROLSMENU_HPP
#define GAME_UI_UISCENE_CONTROLSMENU_HPP

#include <stdint.h>
#include <string>

#include "game/core/GameSettings.hpp"
#include "game/ui/UIControl_Label.hpp"
#include "game/ui/UIControl_MultiList.hpp"
#include "game/ui/UIScene.hpp"

class UIScene_ControlsMenu : public UIScene
{
public:
    enum EControl
    {
        EControl_ResetToDefaults,
        EControl_InvertLook,
        EControl_SouthPaw,
        EControl_Look,
        EControl_Move,
        EControl_Use,
        EControl_Action,
        EControl_Jump,
        EControl_Drop,
        EControl_Inventory,
        EControl_Crafting,
        EControl_CycleHeldItemLeft,
        EControl_CycleHeldItemRight,
        EControl_Sneak,
        EControl_Sprint,
        EControl_PickBlock,
        EControl_ChangeCameraMode,
        EControl_FlyLeft,
        EControl_FlyRight,
        EControl_FlyUp,
        EControl_FlyDown,
        EControl_SpectatePrevious,
        EControl_SpectateNext,
        EControl_SpectateNoise,
        EControl_SpectateCancel,
        EControl_ConfirmReady,
        EControl_Vote,
        EControl_RestartSection,
        EControl_RestartRace,
        EControl_LookBehind,
        EControl_QuickMove,
        EControl_TakeHalf,
        EControl_PlayerList,
        EControl_ToggleHud,
        EControl_Chat,
        EControl_ChatCommand,
        EControl_DebugOverlay,
        EControl_Label,
        EControl_HoldToSneak,
        EControl_TouchControls,
        EControl_TouchPickBlock,
        EControl_TouchLeftHanded,
        EControl_TouchEasySprint,
        EControl_TouchPerspective,
        EControl_TouchJoystick,
        EControl_TouchSwapJumpSneak
    };

    UIScene_ControlsMenu(int player, void* data, UILayer* layer);
    virtual ~UIScene_ControlsMenu();
    std::wstring GetControlsString(int player, EControllerActions action, bool& mapped);
#if LCE_PLATFORM_PC
    std::wstring TouchControlsString();
    std::wstring TouchJoystickString();
    void AddTouchRows();
    void UpdateTouchRows();
#endif
    void SetControlsHelp(int player, int control, std::wstring help);
    void Init(int player);
    virtual unsigned int mapElementsAndNames();
    virtual void updateTooltips();
    virtual void updateComponents();
    int GetControlsAction(EControl control);
    std::wstring GetControlsString(EControl control, bool& mapped);
    void UpdateControls(int player);
    static int ResetControlsWarningDialogReturned(void* context, int pad, C4JStorage::EMessageResult result);
    void CheckMapping(int player);
    virtual void tick();
    virtual std::wstring getMoviePath();
    virtual void handleCheckboxElementToggled(int first, int second, bool checked);
    static int ResetDefaultsDialogReturned(void* context, int pad, C4JStorage::EMessageResult result);
    static int ControlsWarningDialogReturned(void* context, int pad, C4JStorage::EMessageResult result);
    virtual void handleReload();
    unsigned int CheckControls(int player, EControl control);
    bool HasMapping(int player, EControl control);
    bool SameMapping(int player, EControl control, EControl otherControl);
    void ClearMapping(int player, EControl control);
    bool UsesKeyboard(int player);
    std::wstring AssignHelp(int player);
    void FinishButtonEdit(int player);
    void HandleKeyCapture(int player);
    void HandlePadCapture(int player);
    void CommitPadButton(int player, unsigned char button);
    int ControlGroup(EControl control);
    int InventoryControl(EControl control);
    unsigned char PadMapping(int player, EControl control);
    void SetPadMapping(int player, EControl control, unsigned char mapping);
    void AddControlRow(int player, EControl control, const std::wstring& label);
    void HandleButtonEdit(int player, int action, bool repeat, bool pressed, bool released);
    unsigned int CheckControlsOnExit(int player);
    virtual void handleInput(int player, int action, bool repeat, bool pressed, bool released, bool& handled);
    virtual void handlePress(int controlId, int childId);
    virtual void handleFocusChange(int focus, int childFocus);
    virtual int getSceneType();

private:
    bool m_canFly;
    bool m_updateControls;
    bool m_mappingsChanged;
    bool m_showAssignHelp;
    bool m_clearHelp;
    bool m_editingButton;
    bool m_unusedEditFlag;
    bool m_checkMapping;
    unsigned char m_firstButton;
    unsigned char m_secondButton;
    unsigned int m_firstButtonMask;
    int m_buttonCount;
    int m_lastEditAction;
    unsigned int m_ignoredPadButtons;
    UIControl_Label m_version;
    UIControl_MultiList m_controlsList;
#if LCE_PLATFORM_PC
    bool m_touchRows;
#endif
};

#endif
