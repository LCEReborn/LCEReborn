#include "game/ui/UIScene_CustomMenu.hpp"
#include "game/core/StringIds.hpp"

#include "game/client/audio/SoundEvents.hpp"
#include "game/core/CConsoleMinecraftApp.hpp"
#include "game/core/ConsoleUIController.hpp"
#include "game/core/GameSettings.hpp"
#include "game/core/Minecraft.hpp"
#include "game/ui/FJ_MultilistMenu.hpp"
#include "game/ui/UICustomMenuPage.hpp"
#include "game/ui/UILayer.hpp"
#include "game/ui/UIString.hpp"
#include "game/ui/fuiFile.hpp"
#include "game/ui/fuiRenderNode.hpp"

extern ConsoleUIController gConsoleUIController;

UICustomMenuPage::~UICustomMenuPage()
{
}

void UICustomMenuPage::OnPress(UIScene_CustomMenu&, int)
{
}

void UICustomMenuPage::OnToggle(UIScene_CustomMenu&, int, bool)
{
}

void UICustomMenuPage::OnSlide(UIScene_CustomMenu&, int, int)
{
}

void UICustomMenuPage::OnClose(UIScene_CustomMenu&)
{
}

UIScene_CustomMenu::UIScene_CustomMenu(int playerId, void* data, UILayer* layer)
    : UIScene(playerId, layer),
      m_page(static_cast<UICustomMenuPage*>(data))
{
    initialiseMovie();
    m_description.init(UIString(L""));
    Build();
}

UIScene_CustomMenu::~UIScene_CustomMenu()
{
    delete m_page;
}

std::wstring UIScene_CustomMenu::getMoviePath()
{
    return gApp.GetLocalPlayerCount() > 1 ? std::wstring(L"MultilistMenuSplit") : std::wstring(L"MultilistMenu");
}

unsigned int UIScene_CustomMenu::mapElementsAndNames()
{
    UIScene::mapElementsAndNames();
    fuiRenderNode* root = m_movie->getRootNode();
    m_list.setupControl(this, root, std::string("MultiList"));
    m_controls.push_back(&m_list);
    m_description.setupControl(this, root, std::string("Description"));
    m_controls.push_back(&m_description);
    return 1;
}

int UIScene_CustomMenu::getSceneType()
{
    return EUIScene_CustomMenu;
}

void UIScene_CustomMenu::Build()
{
    m_descriptions.clear();
    if (m_page != NULL)
        m_page->Build(*this);

    FJ_MultilistMenu* menu = static_cast<FJ_MultilistMenu*>(m_movie->getRootNode()->getFuiNode());
    menu->DoVerticalResizeCheck();
    doHorizontalResizeCheck();
    if (m_descriptions.empty())
        menu->HideDescription();
    gConsoleUIController.TouchBoxRebuild(this);
}

void UIScene_CustomMenu::updateTooltips()
{
    gConsoleUIController.SetTooltips(m_playerId, IDS_TOOLTIPS_SELECT, IDS_TOOLTIPS_BACK, -1, -1, -1, -1, -1, -1, -1, -1,
                                     -1, -1, -1, -1, -1, -1, false, false);
}

void UIScene_CustomMenu::updateComponents()
{
    if (Minecraft::GetInstance()->mLevel == NULL)
    {
        m_layer->showComponent(m_playerId, EUIScene_Panorama, true);
        m_layer->showComponent(m_playerId, EUIScene_Logo, true);
    }
    else
    {
        m_layer->showComponent(m_playerId, EUIScene_Panorama, false);
        m_layer->showComponent(m_playerId, EUIScene_Logo, gApp.GetLocalPlayerCount() == 1);
    }
}

void UIScene_CustomMenu::handleInput(int playerId, int action, bool repeat, bool pressed, bool released,
                                     bool& handled)
{
    gConsoleUIController.AnimateKeyPress(playerId, action, repeat, pressed, released);
    switch (action)
    {
    case EControllerActions_MenuCancel:
        if (pressed)
        {
            if (m_page != NULL)
                m_page->OnClose(*this);
            navigateBack();
            handled = true;
        }
        break;
    case EControllerActions_MenuUp:
    case EControllerActions_MenuDown:
    case EControllerActions_MenuRight:
    case EControllerActions_MenuLeft:
    case EControllerActions_MenuPageUp:
    case EControllerActions_MenuPageDown:
    case EControllerActions_MenuOtherStickUp:
    case EControllerActions_MenuOtherStickDown:
    case EControllerActions_MenuOk:
        sendInputToMovie(action, repeat, pressed, released);
        break;
    }
}

void UIScene_CustomMenu::handleReload()
{
    Build();
}

void UIScene_CustomMenu::handlePress(int, int childId)
{
    gConsoleUIController.PlayUISFX(SoundEvents::UI_PRESS);
    if (m_page != NULL)
        m_page->OnPress(*this, childId);
}

void UIScene_CustomMenu::handleCheckboxElementToggled(int, int childId, bool checked)
{
    gConsoleUIController.PlayUISFX(SoundEvents::UI_PRESS);
    if (m_page != NULL)
        m_page->OnToggle(*this, childId, checked);
}

void UIScene_CustomMenu::handleSliderElementMove(int, int childId, int value)
{
    gConsoleUIController.PlayUISFX(SoundEvents::UI_SCROLL);
    if (m_page != NULL)
        m_page->OnSlide(*this, childId, value);
}

void UIScene_CustomMenu::handleFocusChange(int, int childFocus)
{
    ShowDescription(childFocus);
}

void UIScene_CustomMenu::ShowDescription(int id)
{
    if (m_descriptions.empty())
        return;
    std::map<int, std::wstring>::const_iterator found = m_descriptions.find(id);
    m_description.setLabel(UIString(found != m_descriptions.end() ? found->second : std::wstring()), true, false);
}

void UIScene_CustomMenu::AddButton(int id, const std::wstring& label, const std::wstring& description)
{
    m_list.AddNewButton(label, id, false);
    if (!description.empty())
        m_descriptions[id] = description;
}

void UIScene_CustomMenu::AddCheckbox(int id, const std::wstring& label, bool checked, const std::wstring& description)
{
    m_list.AddNewCheckbox(label, id, checked);
    if (!description.empty())
        m_descriptions[id] = description;
}

void UIScene_CustomMenu::AddSlider(int id, const std::wstring& label, int minimum, int maximum, int value,
                                   const std::wstring& description)
{
    m_list.AddNewSlider(label, id, minimum, maximum, value, true, 7, 3);
    if (!description.empty())
        m_descriptions[id] = description;
}

void UIScene_CustomMenu::AddValue(int id, const std::wstring& label, const std::wstring& value,
                                  const std::wstring& description)
{
    m_list.AddNewControllerButton(label, value, true, id);
    if (!description.empty())
        m_descriptions[id] = description;
}

void UIScene_CustomMenu::AddLabel(int id, const std::wstring& label)
{
    m_list.AddNewLabel(label, id);
}

void UIScene_CustomMenu::SetItemLabel(int id, const std::wstring& label)
{
    m_list.SetItemLabel(id, UIString(label), false);
}

void UIScene_CustomMenu::SetItemValue(int id, const std::wstring& value)
{
    m_list.SetSecondaryLabel(id, UIString(value), true, true, false);
}

bool UIScene_CustomMenu::GetCheckboxValue(int id)
{
    return m_list.GetCheckboxValue(id);
}

int UIScene_CustomMenu::GetSliderValue(int id)
{
    return static_cast<int>(m_list.GetSliderValue(id));
}

void UIScene_CustomMenu::OpenPage(UICustomMenuPage* page)
{
    gConsoleUIController.NavigateToScene(m_playerId, EUIScene_CustomMenu, page, EUILayer_Scene,
                                         EUIGroup_PlayerDefault);
}

int UIScene_CustomMenu::GetPlayer() const
{
    return m_playerId;
}
