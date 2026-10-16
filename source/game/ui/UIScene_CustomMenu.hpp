#ifndef GAME_UI_UISCENE_CUSTOMMENU_HPP
#define GAME_UI_UISCENE_CUSTOMMENU_HPP

#include <map>
#include <string>

#include "game/ui/UIControl_HTMLLabel.hpp"
#include "game/ui/UIControl_MultiList.hpp"
#include "game/ui/UIScene.hpp"

class UICustomMenuPage;

class UIScene_CustomMenu : public UIScene
{
public:
    UIScene_CustomMenu(int playerId, void* data, UILayer* layer);
    virtual ~UIScene_CustomMenu();

    virtual std::wstring getMoviePath();
    virtual unsigned int mapElementsAndNames();
    virtual int getSceneType();
    virtual void updateTooltips();
    virtual void updateComponents();
    virtual void handleInput(int playerId, int action, bool repeat, bool pressed, bool released, bool& handled);
    virtual void handleReload();
    virtual void handlePress(int controlId, int childId);
    virtual void handleCheckboxElementToggled(int controlId, int childId, bool checked);
    virtual void handleSliderElementMove(int controlId, int childId, int value);
    virtual void handleFocusChange(int focus, int childFocus);

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
    void OpenPage(UICustomMenuPage* page);
    int GetPlayer() const;

private:
    void Build();
    void ShowDescription(int id);

    UIControl_MultiList m_list;
    UIControl_HTMLLabel m_description;
    UICustomMenuPage* m_page;
    std::map<int, std::wstring> m_descriptions;
};

#endif
