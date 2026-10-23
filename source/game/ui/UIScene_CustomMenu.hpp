#ifndef GAME_UI_UISCENE_CUSTOMMENU_HPP
#define GAME_UI_UISCENE_CUSTOMMENU_HPP

#include <map>
#include <string>

#include "game/ui/UIControl_HTMLLabel.hpp"
#include "game/ui/UIControl_MultiList.hpp"
#include "game/ui/UIScene.hpp"

class FJ_FuiNode;
class UICustomMenuPage;

class UIScene_CustomMenu : public UIScene
{
public:
    UIScene_CustomMenu(int playerId, void* data, UILayer* layer);
    virtual ~UIScene_CustomMenu();

    virtual std::wstring getMoviePath();
    virtual unsigned int mapElementsAndNames();
    virtual int getSceneType();
    virtual void tick();
    virtual void updateTooltips();
    virtual void updateComponents();
    virtual bool hidesLowerScenes();
    virtual bool blocksInput();
    virtual void handleInput(int playerId, int action, bool repeat, bool pressed, bool released, bool& handled);
    virtual void handleReload();
    virtual void handlePress(int controlId, int childId);
    virtual void handleCheckboxElementToggled(int controlId, int childId, bool checked);
    virtual void handleSliderElementMove(int controlId, int childId, int value);
    virtual void handleFocusChange(int focus, int childFocus);
    virtual void handleInitFocus(int focus, int childFocus);
    virtual void handleGainFocus(bool hadFocus);
    virtual void HandleDLCMountingComplete();

    void AddButton(int id, const std::wstring& label, const std::wstring& description);
    void AddCheckbox(int id, const std::wstring& label, bool checked, const std::wstring& description);
    void AddSlider(int id, const std::wstring& label, int minimum, int maximum, int value,
                   const std::wstring& description);
    void AddValue(int id, const std::wstring& label, const std::wstring& value, const std::wstring& description);
    void AddLabel(int id, const std::wstring& label);
    void SetItemLabel(int id, const std::wstring& label);
    void SetItemValue(int id, const std::wstring& value);
    bool GetCheckboxValue(int id);
    int GetSliderValue(int id);
    void SetCheckboxValue(int id, bool checked);
    void SetSliderValue(int id, int value);
    void SetDescription(int id, const std::wstring& description);
    int GetFocusId() const;
    void Rebuild();
    void OpenPage(UICustomMenuPage* page);
    int GetPlayer() const;

private:
    enum EPanel
    {
        ePanel_List,
        ePanel_ListBackground,
        ePanel_Description,
        ePanel_DescriptionBackground,
        ePanel_Count
    };

    void Build();
    void ShowDescription(int id);
    void ToggleDescriptionPanel();
    void ApplyDescriptionPanel();
    void FitDescriptionPanel();
    void PlaceDescriptionBehindList(FJ_FuiNode* menu);
    bool HasDescription(int id) const;

    UIControl_MultiList m_list;
    UIControl_HTMLLabel m_description;
    UICustomMenuPage* m_page;
    std::map<int, std::wstring> m_descriptions;
    FJ_FuiNode* m_panels[ePanel_Count];
    float m_openX[ePanel_Count];
    float m_closedX[ePanel_Count];
    float m_maximumDescriptionHeight;
    int m_focusId;
    bool m_descriptionOpen;
    float m_descriptionAmount;
    int m_lastAnimationTime;
};

#endif
