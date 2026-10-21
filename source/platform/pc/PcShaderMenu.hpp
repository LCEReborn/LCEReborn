#ifndef PLATFORM_PC_PCSHADERMENU_HPP
#define PLATFORM_PC_PCSHADERMENU_HPP

#include <string>
#include <vector>

#include "game/ui/UIControl.hpp"
#include "game/ui/UIControl_HTMLLabel.hpp"
#include "game/ui/UIControl_Label.hpp"
#include "game/ui/UIControl_MultiList.hpp"
#include "game/ui/UIControl_Touch.hpp"
#include "game/ui/UIScene.hpp"

class UILayer;

class UIScene_ShaderMenu : public UIScene
{
public:
    UIScene_ShaderMenu(int playerId, void* data, UILayer* layer);
    virtual ~UIScene_ShaderMenu();

    virtual std::wstring getMoviePath();
    virtual unsigned int mapElementsAndNames();
    virtual int getSceneType();
    virtual void updateTooltips();
    virtual void updateComponents();
    virtual void handleReload();
    virtual void handleGainFocus(bool hadFocus);
    virtual void handleInput(int playerId, int action, bool repeat, bool pressed, bool released, bool& handled);
    virtual void handleFocusChange(int focus, int childFocus);
    virtual void handlePress(int controlId, int childId);
    virtual void handleCheckboxElementToggled(int controlId, int childId, bool checked);
    virtual void handleSliderElementMove(int controlId, int childId, int value);
    virtual void handleTouchInput(unsigned int playerId, int touchX, int touchY, int touchBox, bool pressed,
                                  bool repeat, bool released);
    virtual UIControl* GetMainPanel();

private:
    void Build();
    void RegisterIcons();
    void FocusSelectedPack();
    void BuildPacks();
    void BuildSettings();
    void Layout();
    void SetTab(int tab);
    void UpdateDescription();
    void SelectPack(int id);
    void OpenPackSettings();
    void ResetSettings();
    void SaveSettings();

    UIControl_Label m_packsLabel;
    UIControl_Label m_settingsLabel;
    UIControl m_packsPanel;
    UIControl m_settingsPanel;
    UIControl_MultiList m_packsList;
    UIControl_MultiList m_settingsList;
    UIControl_Touch m_packsTab;
    UIControl_Touch m_settingsTab;
    UIControl_HTMLLabel m_packsDescription;
    UIControl_HTMLLabel m_settingsDescription;
    UIControl_Label m_unusedSizeLabel;
    UIControl_Label m_unusedResizeLabel;
    std::vector<std::string> m_packs;
    std::wstring m_iconColour;
    std::wstring m_iconGrey;
    int m_tab;
    int m_focus[2];
};

#endif
