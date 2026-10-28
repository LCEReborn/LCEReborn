#include "game/ui/UIScene_AbstractContainerMenu.hpp"
#include "game/core/StringIds.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "game/client/render/Renderer.hpp"
#include "game/core/CConsoleMinecraftApp.hpp"
#include "game/core/CMinecraftApp.hpp"
#include "game/core/ConsoleUIController.hpp"
#include "game/core/HtmlString.hpp"
#include "game/core/L10N.hpp"
#include "game/core/Minecraft.hpp"
#include "game/core/Slot.hpp"
#include "game/item/Inventory.hpp"
#include "game/item/Item.hpp"
#include "game/item/ItemInstance.hpp"
#include "game/minigame/Tutorial.hpp"
#include "game/ui/AbstractContainerMenu.hpp"
#include "game/ui/FJ_AbstractContainerDocument.hpp"
#include "game/ui/fui.hpp"
#include "game/ui/fuiFile.hpp"
#if LCE_PLATFORM_PC
#include "platform/CInput.hpp"
#endif
#include "game/ui/fuiRect.hpp"
#include "game/ui/fuiRenderNode.hpp"
#include "game/ui/UILayer.hpp"
#include "game/world/generation/MultiPlayerGameMode.hpp"
#include "game/world/generation/MultiplayerLocalPlayer.hpp"

extern ConsoleUIController gConsoleUIController;

UIScene_AbstractContainerMenu::UIScene_AbstractContainerMenu(int type, UILayer* layer) : UIScene(type, layer)
{
    m_focusSection = IUIScene_AbstractContainerMenu::ESceneSection_None;
    gConsoleUIController.OverrideSFX(m_playerId, 0, true);
    gConsoleUIController.OverrideSFX(m_playerId, 20, true);
    gConsoleUIController.OverrideSFX(m_playerId, 2, true);
    gConsoleUIController.OverrideSFX(m_playerId, 3, true);
    gConsoleUIController.OverrideSFX(m_playerId, 11, true);
    gConsoleUIController.OverrideSFX(m_playerId, 10, true);
    gConsoleUIController.OverrideSFX(m_playerId, 7, true);
    gConsoleUIController.OverrideSFX(m_playerId, 6, true);
    gConsoleUIController.OverrideSFX(m_playerId, 4, true);
    gConsoleUIController.OverrideSFX(m_playerId, 5, true);
    m_ignoreInput = false;
#if LCE_PLATFORM_PC
    m_touchDragSection = IUIScene_AbstractContainerMenu::ESceneSection_None;
    m_touchDragSlotX = 0;
    m_touchDragSlotY = 0;
#endif
}

UIScene_AbstractContainerMenu::~UIScene_AbstractContainerMenu()
{
}

void UIScene_AbstractContainerMenu::PlatformInitialize(bool navigateBack, int pad, int startIndex)
{
    m_inventoryLabel.init(UIString(L10N::GetString(IDS_INVENTORY)));
    if (m_smallInventory)
    {
        static_cast<FJ_AbstractContainerDocument*>(m_movie->getRootNode()->getFuiNode())->HideInventory();
        m_hotbarList.UpdateControl();
        m_backgroundPanel.UpdateControl();
    }
    if (startIndex >= 0)
    {
        if (!m_smallInventory)
            m_inventoryList.addSlots(static_cast<unsigned int>(startIndex), 27);
        m_hotbarList.addSlots(static_cast<unsigned int>(startIndex + 27), 9);
    }

    float panelWidth = m_backgroundPanel.getWidth();
    float panelHeight = m_backgroundPanel.getHeight();
    float panelX = m_backgroundPanel.getXPos();
    float panelY = m_backgroundPanel.getYPos();

    m_pointerImageOffset.x = 0.0f;
    m_pointerImageOffset.y = 0.0f;
    m_panelMinX = panelX;
    m_panelMaxX = panelX + panelWidth;
    m_panelMinY = panelY;
    m_panelMaxY = panelY + panelHeight;

    float pointerMinY;
    if (gApp.GetLocalPlayerCount() > 1)
        pointerMinY = floorf(25.0f);
    else
        pointerMinY = panelY - 50.0f;
    m_pointerMinY = pointerMinY;
    m_pointerMinX = panelX - 50.0f;
    m_pointerMaxX = m_panelMaxX + 50.0f;
    m_pointerMaxY = m_panelMaxY + 25.0f;

    _UIVec2D itemPos;
    _UIVec2D itemSize;
    _UIVec2D sectionPos;
    GetItemScreenData(m_currentSection, 0, &itemPos, &itemSize);
    GetPositionOfSection(m_currentSection, &sectionPos);
    m_movieWidth = m_stageWidth;
    m_pointerPos.x = (itemSize.x * 0.5f + (sectionPos.x + itemPos.x)) - m_pointerImageOffset.x;
    m_pointerPos.y = (itemSize.y * 0.5f + (sectionPos.y + itemPos.y)) - m_pointerImageOffset.y;
    m_movieHeight = m_stageHeight;
#if LCE_PLATFORM_PC
    if (g_pInput->UsesKeyboard(m_playerId))
        PlacePointerAtMouse();
#endif

    int width;
    int height;
    m_layer->getRenderDimensions(width, height);
    if (width == 1280 && height == 480)
        width = 640;
    gFui->dispatchMouseMoveEvent(
        m_movie,
        static_cast<float>(
            static_cast<int>(m_pointerPos.x * (static_cast<float>(width) / static_cast<float>(m_stageWidth)))),
        static_cast<float>(
            static_cast<int>(m_pointerPos.y * (static_cast<float>(height) / static_cast<float>(m_stageHeight)))));
}

void UIScene_AbstractContainerMenu::InitDataAssociations(int, AbstractContainerMenu*, int)
{
}

void UIScene_AbstractContainerMenu::setSectionFocus(IUIScene_AbstractContainerMenu::ESceneSection section, int)
{
    UIControl* selectedSection = getSection(section);
    if (selectedSection != NULL)
        selectedSection->setFocus(true);

    if (m_focusSection != IUIScene_AbstractContainerMenu::ESceneSection_None)
    {
        UIControl* previousSection = getSection(m_focusSection);
        if (previousSection != selectedSection && previousSection != NULL)
            previousSection->setFocus(false);
    }
    m_focusSection = section;
}

void UIScene_AbstractContainerMenu::setFocusToPointer(int)
{
    if (m_focusSection != IUIScene_AbstractContainerMenu::ESceneSection_None)
    {
        UIControl* previousSection = getSection(m_focusSection);
        if (previousSection != NULL)
            previousSection->setFocus(false);
    }
    m_focusSection = IUIScene_AbstractContainerMenu::ESceneSection_None;
}

void UIScene_AbstractContainerMenu::SetPointerText(std::vector<HtmlString>* description, bool newSlot)
{
    m_cursor.setLabel(UIString(HtmlString::Compose(description)), false, newSlot);
}

not_null_ptr<ItemInstance> UIScene_AbstractContainerMenu::getSlotItem(
    IUIScene_AbstractContainerMenu::ESceneSection section, int slotIndex)
{
    Slot* slot = m_menu->getSlot(getSectionStartOffset(section) + slotIndex);
    if (slot != NULL)
        return slot->getItem();
    return ItemInstance::EMPTY;
}

Slot* UIScene_AbstractContainerMenu::getSlot(IUIScene_AbstractContainerMenu::ESceneSection section, int slotIndex)
{
    return m_menu->getSlot(getSectionStartOffset(section) + slotIndex);
}

bool UIScene_AbstractContainerMenu::isSlotEmpty(IUIScene_AbstractContainerMenu::ESceneSection section, int slotIndex)
{
    Slot* slot = m_menu->getSlot(getSectionStartOffset(section) + slotIndex);
    if (slot != NULL)
        return !slot->hasItem();
    return false;
}

void UIScene_AbstractContainerMenu::adjustPointerForSafeZone()
{
}

unsigned int UIScene_AbstractContainerMenu::mapElementsAndNames()
{
    fuiRenderNode* currentNode = m_movie->getRootNode();
    {
        m_mainPanel.setupControl(this, currentNode, std::string("MainPanel"));
        m_controls.push_back(&m_mainPanel);
    }
    {
        fuiRenderNode* parentNode = currentNode;
        currentNode = m_mainPanel.getFuiRenderNode();
        {
            m_backgroundPanel.setupControl(this, currentNode, std::string("BackgroundPanel"));
            m_controls.push_back(&m_backgroundPanel);
        }
        {
            m_hotbarList.setupControl(this, currentNode, std::string("hotbarList"));
            m_controls.push_back(&m_hotbarList);
        }
        {
            m_inventoryList.setupControl(this, currentNode, std::string("inventoryList"));
            m_controls.push_back(&m_inventoryList);
        }
        {
            m_cursor.setupControl(this, currentNode, std::string("cursor"));
            m_controls.push_back(&m_cursor);
        }
        {
            m_inventoryLabel.setupControl(this, currentNode, std::string("inventoryLabel"));
            m_controls.push_back(&m_inventoryLabel);
        }
        currentNode = parentNode;
    }
    return 1;
}

void UIScene_AbstractContainerMenu::handleDestroy()
{
    Minecraft* minecraft = Minecraft::GetInstance();
    if (minecraft->mGameModes[m_playerId] != NULL)
    {
        MultiPlayerGameMode* gameMode = Minecraft::GetInstance()->mGameModes[m_playerId];
        if (gameMode != NULL)
        {
            Tutorial* tutorial = gameMode->getTutorial();
            if (tutorial != NULL)
                tutorial->changeTutorialState(m_previousTutorialState, NULL);
        }
    }

    if (minecraft->mLocalPlayers[m_playerId] != NULL
        && minecraft->mLocalPlayers[m_playerId]->mContainerMenu->m_containerId == m_menu->m_containerId)
        minecraft->mLocalPlayers[m_playerId]->closeContainer();

    gConsoleUIController.OverrideSFX(m_playerId, 0, false);
    gConsoleUIController.OverrideSFX(m_playerId, 20, false);
    gConsoleUIController.OverrideSFX(m_playerId, 2, false);
    gConsoleUIController.OverrideSFX(m_playerId, 3, false);
    gConsoleUIController.OverrideSFX(m_playerId, 11, false);
    gConsoleUIController.OverrideSFX(m_playerId, 10, false);
    gConsoleUIController.OverrideSFX(m_playerId, 7, false);
    gConsoleUIController.OverrideSFX(m_playerId, 6, false);
    gConsoleUIController.OverrideSFX(m_playerId, 4, false);
    gConsoleUIController.OverrideSFX(m_playerId, 5, false);
}

void UIScene_AbstractContainerMenu::tick()
{
    UIScene::tick();
#if LCE_PLATFORM_PC
    float mouseX;
    float mouseY;
    if (!m_ignoreInput && g_pInput->ConsumeMenuPointer(m_playerId, &mouseX, &mouseY)
        && TouchToPanel(mouseX, mouseY, &m_mousePointerX, &m_mousePointerY))
    {
        m_mousePointerPending = true;
    }
#endif
    onMouseTick();
#if LCE_PLATFORM_PC
    unsigned int clicks = g_pInput->ConsumeMenuClicks(m_playerId);
    float split = g_pInput->ConsumeMenuSplit(m_playerId);
    int wheel = g_pInput->ConsumeMenuWheel(m_playerId);
    if (!m_ignoreInput)
    {
        if ((clicks & C_4JInput::EPcMenuClick_Quick) != 0)
            handleKeyDown(m_playerId, EControllerActions_MenuY, false);
        if ((clicks & C_4JInput::EPcMenuClick_Primary) != 0)
            handleKeyDown(m_playerId, EControllerActions_MenuA, false);
        if ((clicks & C_4JInput::EPcMenuClick_Secondary) != 0)
            handleKeyDown(m_playerId, EControllerActions_MenuX, false);
        if ((clicks & C_4JInput::EPcMenuClick_DragStart) != 0)
            TouchDragStart();
        if ((clicks & C_4JInput::EPcMenuClick_DragEnd) != 0)
            TouchDragEnd();
        if ((clicks & C_4JInput::EPcMenuClick_Auto) != 0)
            TouchAutoPlace();
        if ((clicks & C_4JInput::EPcMenuClick_Split) != 0)
            TouchSplit(split);
        if (wheel != 0)
            onMouseWheel(wheel);
        TouchHoldAmount();
    }
#endif

    int width;
    int height;
    m_layer->getRenderDimensions(width, height);
    if (width == 1280 && height == 480)
        width = 640;
    gFui->dispatchMouseMoveEvent(
        m_movie,
        static_cast<float>(
            static_cast<int>(m_pointerPos.x * (static_cast<float>(width) / static_cast<float>(m_stageWidth)))),
        static_cast<float>(
            static_cast<int>(m_pointerPos.y * (static_cast<float>(height) / static_cast<float>(m_stageHeight)))));
}

#if LCE_PLATFORM_PC
bool UIScene_AbstractContainerMenu::TouchToPanel(float touchX, float touchY, float* x, float* y)
{
    int originX = 0;
    int originY = 0;
    gConsoleUIController.GetViewportTouchOffset(m_layer->getViewport(), &originX, &originY);
    int renderWidth;
    int renderHeight;
    m_layer->getRenderDimensions(renderWidth, renderHeight);
    if (renderWidth <= 0 || renderHeight <= 0)
        return false;
    *x = (touchX - static_cast<float>(originX)) * static_cast<float>(m_stageWidth) / static_cast<float>(renderWidth);
    *y = (touchY - static_cast<float>(originY)) * static_cast<float>(m_stageHeight) / static_cast<float>(renderHeight);
    if (m_mainPanel.getFuiRenderNode() != NULL)
    {
        *x -= m_mainPanel.getXPos();
        *y -= m_mainPanel.getYPos();
    }
    return true;
}

void UIScene_AbstractContainerMenu::PlacePointerAtMouse()
{
    float touchX;
    float touchY;
    const bool mouseFree = g_pInput->GetMenuPointer(m_playerId, &touchX, &touchY);
    if (!mouseFree)
    {
        int originX = 0;
        int originY = 0;
        gConsoleUIController.GetViewportTouchOffset(m_layer->getViewport(), &originX, &originY);
        int renderWidth;
        int renderHeight;
        m_layer->getRenderDimensions(renderWidth, renderHeight);
        touchX = static_cast<float>(originX) + static_cast<float>(renderWidth) * 0.5f;
        touchY = static_cast<float>(originY) + static_cast<float>(renderHeight) * 0.5f;
    }

    float x;
    float y;
    if (!TouchToPanel(touchX, touchY, &x, &y))
        return;
    m_pointerPos.x = x < m_pointerMinX ? m_pointerMinX : (x > m_pointerMaxX ? m_pointerMaxX : x);
    m_pointerPos.y = y < m_pointerMinY ? m_pointerMinY : (y > m_pointerMaxY ? m_pointerMaxY : y);
    m_mousePointerActive = true;
    if (!mouseFree)
        g_pInput->PlaceMenuPointer(m_playerId, touchX, touchY);
}

void UIScene_AbstractContainerMenu::onMouseWheel(int)
{
}

static int TouchSplitAmount(float fraction, int total)
{
    int amount = static_cast<int>(ceilf(fraction * static_cast<float>(total)));
    if (amount < 1)
        amount = 1;
    return amount > total ? total : amount;
}

static bool CarryingItem(int pad)
{
    boost::shared_ptr<MultiplayerLocalPlayer> player = Minecraft::GetInstance()->mLocalPlayers[pad];
    return player.get() != NULL && !player->mInventory->getCarried()->isEmpty();
}

bool UIScene_AbstractContainerMenu::TouchSplitTarget(int* total, bool* placing)
{
    *total = 0;
    *placing = false;
    boost::shared_ptr<MultiplayerLocalPlayer> player = Minecraft::GetInstance()->mLocalPlayers[getPad()];
    if (player.get() == NULL || m_pointerOutsideMenu || !IsSectionSlotList(m_currentSection))
        return false;
    not_null_ptr<ItemInstance> carried = player->mInventory->getCarried();
    const int slotIndex = getCurrentIndex(m_currentSection) - getSectionStartOffset(m_currentSection);
    not_null_ptr<ItemInstance> item = getSlotItem(m_currentSection, slotIndex);
    if (carried->isEmpty())
    {
        if (item->isEmpty())
            return false;
        *total = item->getCount();
        return true;
    }
    if (!item->isEmpty() && !IsSameItemAs(item, carried))
        return false;
    *placing = true;
    *total = carried->getCount();
    return true;
}

void UIScene_AbstractContainerMenu::TouchSplit(float fraction)
{
    int total;
    bool placing;
    if (!TouchSplitTarget(&total, &placing))
    {
        handleKeyDown(m_playerId, EControllerActions_MenuA, false);
        return;
    }
    const int amount = TouchSplitAmount(fraction, total);
    if (placing)
    {
        for (int placed = 0; placed < amount; ++placed)
            handleKeyDown(m_playerId, EControllerActions_MenuX, false);
        return;
    }
    handleKeyDown(m_playerId, EControllerActions_MenuA, false);
    for (int left = amount; left < total; ++left)
        handleKeyDown(m_playerId, EControllerActions_MenuX, false);
}

void UIScene_AbstractContainerMenu::TouchDragStart()
{
    m_touchDragSection = IUIScene_AbstractContainerMenu::ESceneSection_None;
    if (CarryingItem(getPad()) || m_pointerOutsideMenu)
        return;
    if (IsSectionSlotList(m_currentSection))
    {
        if (isSlotEmpty(m_currentSection, getCurrentIndex(m_currentSection) - getSectionStartOffset(m_currentSection)))
            return;
        m_touchDragSection = m_currentSection;
        m_touchDragSlotX = m_currentSlotX;
        m_touchDragSlotY = m_currentSlotY;
    }
    handleKeyDown(m_playerId, EControllerActions_MenuA, false);
}

void UIScene_AbstractContainerMenu::TouchDragEnd()
{
    if (CarryingItem(getPad()))
        handleKeyDown(m_playerId, EControllerActions_MenuA, false);
    TouchReturnCarried();
}

void UIScene_AbstractContainerMenu::TouchReturnCarried()
{
    const IUIScene_AbstractContainerMenu::ESceneSection section = m_touchDragSection;
    m_touchDragSection = IUIScene_AbstractContainerMenu::ESceneSection_None;
    boost::shared_ptr<MultiplayerLocalPlayer> player = Minecraft::GetInstance()->mLocalPlayers[getPad()];
    if (section == IUIScene_AbstractContainerMenu::ESceneSection_None || player.get() == NULL)
        return;
    not_null_ptr<ItemInstance> carried = player->mInventory->getCarried();
    if (carried->isEmpty())
        return;
    const IUIScene_AbstractContainerMenu::ESceneSection currentSection = m_currentSection;
    const int currentSlotX = m_currentSlotX;
    const int currentSlotY = m_currentSlotY;
    m_currentSection = section;
    m_currentSlotX = m_touchDragSlotX;
    m_currentSlotY = m_touchDragSlotY;
    Slot* slot = getSlot(section, getCurrentIndex(section) - getSectionStartOffset(section));
    if (slot != NULL && slot->mayPlace(carried) && (!slot->hasItem() || IsSameItemAs(slot->getItem(), carried)))
        handleSlotListClicked(section, 0, 0);
    m_currentSection = currentSection;
    m_currentSlotX = currentSlotX;
    m_currentSlotY = currentSlotY;
}

void UIScene_AbstractContainerMenu::TouchAutoPlace()
{
    if (CarryingItem(getPad()))
        handleKeyDown(m_playerId, EControllerActions_MenuA, false);
    handleKeyDown(m_playerId, EControllerActions_MenuY, false);
}

void UIScene_AbstractContainerMenu::TouchHoldAmount()
{
    float progress;
    if (!g_pInput->GetMenuHold(m_playerId, &progress))
        return;
    int total;
    bool placing;
    if (!TouchSplitTarget(&total, &placing))
        total = 0;
    g_pInput->SetMenuHoldAmount(m_playerId, total > 0 ? TouchSplitAmount(progress, total) : 0, total);
}
#endif

void UIScene_AbstractContainerMenu::render(int width, int height, C4JRender::eViewportType viewport)
{
    m_slotDrawPending = true;
    m_updatePending = m_updatePending || m_menu->needsRendered();
    if (m_updatePending)
    {
        m_slotDrawCount = static_cast<int>(GetBaseSlotCount());
        unsigned int slotCount = static_cast<unsigned int>(m_menu->m_slots.size());
        for (unsigned int slot = 0; slot < slotCount; ++slot)
        {
            if (m_menu->getSlot(static_cast<int>(slot))->hasItem())
                ++m_slotDrawCount;
        }
    }
    UIScene::render(width, height, viewport);
    m_updatePending = false;
}

void UIScene_AbstractContainerMenu::customDraw(const char* name, fuiRect* rect)
{
    Minecraft* minecraft = Minecraft::GetInstance();
    if (minecraft->mLocalPlayers[m_playerId] == NULL || minecraft->mGameModes[m_playerId] == NULL)
        return;

    not_null_ptr<ItemInstance> item = ItemInstance::EMPTY;
    int slotId = -1;
    if (std::strcmp(name, "pointerIcon") == 0)
    {
        m_slotDrawPending = false;
        gRenderer->Clear(1);
        item = m_menu->GetQuickCraftItem(m_playerId, -1);
        if (item->isEmpty())
            item = minecraft->mLocalPlayers[m_playerId]->mInventory->getCarried();
    }
    else
    {
        std::sscanf(name, "slot_%d", &slotId);
        if (slotId != -1)
        {
            int slotDrawCount = static_cast<int>(GetBaseSlotCount());
            unsigned int slotCount = static_cast<unsigned int>(m_menu->m_slots.size());
            for (unsigned int slot = 0; slot < slotCount; ++slot)
            {
                if (m_menu->getSlot(static_cast<int>(slot))->hasItem())
                    ++slotDrawCount;
            }

            Slot* slot = m_menu->getSlot(slotId);
            item = m_menu->GetQuickCraftItem(m_playerId, slot->mIndex);
            if (!item->isEmpty())
            {
                not_null_ptr<ItemInstance> slotItem = slot->getItem();
                not_null_ptr<ItemInstance> carried = minecraft->mLocalPlayers[m_playerId]->mInventory->getCarried();
                m_slotDrawPending = IsSameItemAs(slotItem, carried);
            }
            else
            {
                m_slotDrawPending = true;
                item = slot->getItem();
            }
        }
    }

    if (!item->isEmpty())
    {
        float alpha = m_menu->isValidIngredient(item, slotId) ? 1.0f : 0.5f;
        bool foil = item->getItem()->isFoil(item);
        customDrawSlotControlFui(rect, m_playerId, item, alpha, foil, true, false);
    }
}

void UIScene_AbstractContainerMenu::handleInput(int, int action, bool repeat, bool pressed, bool released,
                                                bool& handled)
{
    if (m_ignoreInput)
        return;
    gConsoleUIController.AnimateKeyPress(m_playerId, action, repeat, pressed, released);
    if (pressed)
        handled = handleKeyDown(m_playerId, action, repeat);
}

bool UIScene_AbstractContainerMenu::doesSectionTreeHaveFocus(IUIScene_AbstractContainerMenu::ESceneSection)
{
    return false;
}

int UIScene_AbstractContainerMenu::getPad()
{
    return m_playerId;
}

UIControl* UIScene_AbstractContainerMenu::getSection(IUIScene_AbstractContainerMenu::ESceneSection)
{
    return NULL;
}

unsigned int UIScene_AbstractContainerMenu::GetBaseSlotCount()
{
    return 0;
}

void UIScene_AbstractContainerMenu::updateTooltips()
{
    UpdateTooltips();
}
