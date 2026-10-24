#include "game/ui/UIScene_DebugOverlayMenu.hpp"
#include "game/core/StringIds.hpp"

#include <math.h>

#include "game/client/audio/SoundEvents.hpp"
#include "game/client/render/Font.hpp"
#include "game/client/render/Renderer.hpp"
#include "game/core/CConsoleMinecraftApp.hpp"
#include "game/core/ConsoleUIController.hpp"
#include "game/core/ExtraStrings.hpp"
#include "game/core/GameSettings.hpp"
#include "game/core/GlStateManager.hpp"
#include "game/core/Minecraft.hpp"
#include "game/core/System.hpp"
#include "game/ui/UILayer.hpp"
#include "platform/CInput.hpp"

extern ConsoleUIController gConsoleUIController;

namespace
{
const int kLayoutWidth = MenuFrame::kWidth;
const int kLayoutHeight = MenuFrame::kHeight;
const int kListLeft = 20;
const int kListTop = 40;
const int kListRight = 290;
const int kListInset = 6;
const int kCategoryHeight = 12;
const int kRowHeight = 17;
const int kRowGap = 2;
const int kSwitchWidth = 38;
const int kSwitchHeight = 11;
const int kPresetHeight = 38;
const int kPresetGap = 6;
const int kPreviewLeft = 300;
const int kPreviewRight = 620;
const int kPreviewTop = 40;
const int kTabHeight = 15;
const int kViewportTop = kPreviewTop + kListInset + kTabHeight + 4;
const int kListInnerLeft = kListLeft + kListInset;
const int kListInnerTop = kListTop + kListInset;
const int kListInnerRight = kListRight - kListInset - 4;
const int kSwitchLeftOffset = 4 + kSwitchWidth * 3 + 2;
const float kPresetSpacing = 84.0f;
const float kPi = 3.14159265f;
const int kDisplayOrder[DebugOverlay::EState_Count] = {DebugOverlay::EState_Always, DebugOverlay::EState_WithF3,
                                                      DebugOverlay::EState_Off};
const unsigned int kPanelFill = 0xFFC6C6C6;
const unsigned int kPanelShade = 0xFF555555;
const unsigned int kRowFill = 0xFF6F6F6F;
const unsigned int kRowLight = 0xFFAAAAAA;
const unsigned int kRowFocusFill = 0xFF7B85C4;
const unsigned int kRowFocusLight = 0xFFC7CFFF;
const unsigned int kRowFocusShade = 0xFF59609A;
const unsigned int kTabFill = 0xFF8B8B8B;
const unsigned int kPreviewShade = 0x30000000;

int DisplayIndex(int state)
{
    for (int index = 0; index < DebugOverlay::EState_Count; ++index)
    {
        if (kDisplayOrder[index] == state)
            return index;
    }
    return 1;
}

unsigned int SwitchColour(int state)
{
    switch (state)
    {
    case DebugOverlay::EState_Always:
        return MenuColour::kChipGreen;
    case DebugOverlay::EState_WithF3:
        return MenuColour::kChipAmber;
    default:
        return MenuColour::kChipRed;
    }
}

unsigned int SwitchEdge(int state)
{
    switch (state)
    {
    case DebugOverlay::EState_Always:
        return MenuColour::kChipGreenEdge;
    case DebugOverlay::EState_WithF3:
        return MenuColour::kChipAmberEdge;
    default:
        return MenuColour::kChipRedEdge;
    }
}

void Row(MenuPainter& painter, int x0, int y0, int x1, int y1, unsigned int fill, unsigned int light,
         unsigned int shade)
{
    painter.Fill(x0, y0, x1, y1, MenuColour::kBlack);
    painter.Fill(x0 + 1, y0 + 1, x1 - 1, y1 - 1, fill);
    painter.Fill(x0 + 1, y0 + 1, x1 - 1, y0 + 2, light);
    painter.Fill(x0 + 1, y1 - 3, x1 - 1, y1 - 1, shade);
}
}

UIScene_DebugOverlayMenu::UIScene_DebugOverlayMenu(int playerId, void*, UILayer* layer)
    : UIScene(playerId, layer),
      m_focus(0),
      m_preset(ePreset_Default),
      m_previewOpen(true),
      m_focusY(0.0f),
      m_scroll(0.0f),
      m_carousel(0.0f),
      m_zoom(1.0f),
      m_cameraX(4.0f),
      m_cameraY(4.0f),
      m_pulse(0.0f),
      m_elapsed(0.0f),
      m_lastTime(0)
{
    initialiseMovie();
    const int matching = MatchingPreset();
    m_preset = matching >= 0 ? matching : ePreset_Default;
    m_carousel = static_cast<float>(m_preset);
    m_focusY = static_cast<float>(RowTop(0));
    for (int entry = 0; entry < DebugOverlay::EEntry_Count; ++entry)
    {
        m_changeTime[entry] = 0;
        m_pendingTime[entry] = 0;
        m_pendingState[entry] = 0;
        m_look.amount[entry] = DebugOverlay::IsShown(playerId, entry, m_previewOpen) ? 1.0f : 0.0f;
        m_look.flash[entry] = 0.0f;
    }
    m_look.highlight = 0;
    m_look.pulse = 0.0f;
    m_pointerX = -1.0f;
    m_pointerY = -1.0f;
    m_scrollTarget = 0.0f;
    m_mouseScroll = false;
}

bool UIScene_DebugOverlayMenu::handlesPointer()
{
    return true;
}

UIScene_DebugOverlayMenu::~UIScene_DebugOverlayMenu()
{
    GameSettings::CheckGameSettingsChanged(true, m_playerId);
}

std::wstring UIScene_DebugOverlayMenu::getMoviePath()
{
    return gApp.GetLocalPlayerCount() > 1 ? std::wstring(L"MultilistMenuSplit") : std::wstring(L"MultilistMenu");
}

int UIScene_DebugOverlayMenu::getSceneType()
{
    return EUIScene_DebugOverlayMenu;
}

void UIScene_DebugOverlayMenu::tick()
{
    UIScene::tick();
    HandlePointer();
}

int UIScene_DebugOverlayMenu::ListBottom() const
{
    return m_frame.bottom - kPresetHeight - kPresetGap;
}

int UIScene_DebugOverlayMenu::PresetTop() const
{
    return m_frame.bottom - kPresetHeight;
}

int UIScene_DebugOverlayMenu::RowAt(float x, float y) const
{
    if (x < kListInnerLeft || x >= kListInnerRight || y < kListInnerTop || y >= ListBottom() - kListInset)
        return -1;
    const float content = y - kListInnerTop + m_scroll;
    for (int entry = 0; entry < DebugOverlay::EEntry_Count; ++entry)
    {
        const int top = RowTop(entry);
        if (content >= top && content < top + kRowHeight)
            return entry;
    }
    return -1;
}

int UIScene_DebugOverlayMenu::SwitchAt(int entry, float x, float y) const
{
    const float rowTop = kListInnerTop - m_scroll + RowTop(entry);
    if (y < rowTop + 3 || y >= rowTop + 3 + kSwitchHeight)
        return -1;
    const int left = kListInnerRight - kSwitchLeftOffset;
    for (int index = 0; index < DebugOverlay::EState_Count; ++index)
    {
        const int x0 = left + index * (kSwitchWidth + 1);
        if (x >= x0 && x < x0 + kSwitchWidth)
            return kDisplayOrder[index];
    }
    return -1;
}

int UIScene_DebugOverlayMenu::PresetAt(float x, float y) const
{
    if (y < PresetTop() + 2 || y >= m_frame.bottom - 2 || x < kListLeft || x >= kListRight)
        return -1;
    const float centreX = (kListLeft + kListRight) * 0.5f;
    const float offset = (x - centreX) / kPresetSpacing;
    const int step = static_cast<int>(floorf(offset + 0.5f));
    if (step < -2 || step > 2)
        return -1;
    return ((static_cast<int>(floorf(m_carousel + 0.5f)) + step) % ePreset_Count + ePreset_Count) % ePreset_Count;
}

void UIScene_DebugOverlayMenu::Click(float x, float y)
{
    const int entry = RowAt(x, y);
    if (entry >= 0)
    {
        if (!m_mouseScroll)
            m_scrollTarget = m_scroll;
        m_mouseScroll = true;
        m_focus = entry;
        const int state = SwitchAt(entry, x, y);
        if (state >= 0)
            SetEntryState(entry, state);
        else
            ChangeState(1);
        return;
    }

    const int left = kPreviewLeft + kListInset;
    const int right = kPreviewRight - kListInset;
    const int tabTop = kPreviewTop + kListInset;
    if (x >= left && x < right && y >= tabTop && y < tabTop + kTabHeight)
    {
        const bool open = x >= (left + right) / 2;
        if (open != m_previewOpen)
        {
            m_previewOpen = open;
            gConsoleUIController.PlayUISFX(SoundEvents::UI_PRESS);
        }
        return;
    }

    if (y >= PresetTop() && y < m_frame.bottom && x >= kListLeft && x < kListRight)
    {
        if (x < kListLeft + 14)
        {
            ApplyPreset(m_preset - 1);
            return;
        }
        if (x >= kListRight - 14)
        {
            ApplyPreset(m_preset + 1);
            return;
        }
        const int preset = PresetAt(x, y);
        if (preset >= 0)
            ApplyPreset(preset);
    }
}

void UIScene_DebugOverlayMenu::HandlePointer()
{
#if LCE_PLATFORM_PC
    float pointerX = 0.0f;
    float pointerY = 0.0f;
    const bool hasPointer = g_pInput->GetMenuPointer(m_playerId, &pointerX, &pointerY);
    float x = 0.0f;
    float y = 0.0f;
    m_frame.Pointer(m_playerId, &x, &y);
    if (hasPointer && (pointerX != m_pointerX || pointerY != m_pointerY))
    {
        m_pointerX = pointerX;
        m_pointerY = pointerY;
        const int entry = RowAt(x, y);
        if (entry >= 0 && entry != m_focus)
        {
            if (!m_mouseScroll)
                m_scrollTarget = m_scroll;
            m_mouseScroll = true;
            MoveFocus(entry - m_focus);
        }
    }

    const unsigned int clicks = g_pInput->ConsumeMenuClicks(m_playerId);
    const int wheel = g_pInput->ConsumeMenuWheel(m_playerId);
    if ((clicks & C_4JInput::EPcMenuClick_Primary) != 0 && hasPointer)
        Click(x, y);
    if ((clicks & C_4JInput::EPcMenuClick_Secondary) != 0)
    {
        gConsoleUIController.PlayUISFX(SoundEvents::UI_BACK);
        GameSettings::CheckGameSettingsChanged(true, m_playerId);
        navigateBack();
        return;
    }
    if (wheel != 0)
    {
        if (!m_mouseScroll)
            m_scrollTarget = m_scroll;
        m_mouseScroll = true;
        m_scrollTarget -= static_cast<float>(wheel) * (kRowHeight + kRowGap) * 2.0f;
    }
#endif
}

void UIScene_DebugOverlayMenu::updateTooltips()
{
    gConsoleUIController.SetTooltips(m_playerId, IDS_LCE_DEBUG_CHANGE, IDS_TOOLTIPS_BACK, IDS_LCE_DEBUG_PREVIEW,
                                     IDS_LCE_DEBUG_RESET, -1, -1, -1, IDS_LCE_DEBUG_PRESET, -1, -1, -1, -1, -1, -1, -1,
                                     -1, false, false);
}

void UIScene_DebugOverlayMenu::updateComponents()
{
    const bool inGame = Minecraft::GetInstance()->mLevel != NULL;
    m_layer->showComponent(m_playerId, EUIScene_Panorama, !inGame);
    m_layer->showComponent(m_playerId, EUIScene_Logo, false);
}

int UIScene_DebugOverlayMenu::RowTop(int entry) const
{
    int y = 0;
    int group = -1;
    for (int index = 0; index < DebugOverlay::EEntry_Count; ++index)
    {
        if (DebugOverlay::Group(index) != group)
        {
            group = DebugOverlay::Group(index);
            y += kCategoryHeight;
        }
        if (index == entry)
            return y;
        y += kRowHeight + kRowGap;
    }
    return y;
}

int UIScene_DebugOverlayMenu::ContentHeight() const
{
    return RowTop(DebugOverlay::EEntry_Count);
}

int UIScene_DebugOverlayMenu::PresetState(int preset, int entry)
{
    switch (preset)
    {
    case ePreset_Minimal:
        return entry == DebugOverlay::EEntry_Position || entry == DebugOverlay::EEntry_FrameRate
                   ? DebugOverlay::EState_Always
                   : DebugOverlay::EState_Off;
    case ePreset_Builder:
        if (entry == DebugOverlay::EEntry_Position || entry == DebugOverlay::EEntry_Block
            || entry == DebugOverlay::EEntry_Facing)
            return DebugOverlay::EState_Always;
        if (entry == DebugOverlay::EEntry_Chunk || entry == DebugOverlay::EEntry_Biome
            || entry == DebugOverlay::EEntry_Light)
            return DebugOverlay::EState_WithF3;
        return DebugOverlay::EState_Off;
    case ePreset_Speedrun:
        return entry == DebugOverlay::EEntry_FrameRate || entry == DebugOverlay::EEntry_Time
                       || entry == DebugOverlay::EEntry_Position || entry == DebugOverlay::EEntry_Facing
                       || entry == DebugOverlay::EEntry_Dimension
                   ? DebugOverlay::EState_Always
                   : DebugOverlay::EState_Off;
    case ePreset_Everything:
        return DebugOverlay::EState_Always;
    default:
        return DebugOverlay::EState_WithF3;
    }
}

const wchar_t* UIScene_DebugOverlayMenu::PresetName(int preset)
{
    static const wchar_t* const kNames[ePreset_Count] = {L"Minimal", L"Builder", L"Speedrun", L"Everything",
                                                         L"Default"};
    return kNames[preset];
}

int UIScene_DebugOverlayMenu::MatchingPreset() const
{
    for (int preset = 0; preset < ePreset_Count; ++preset)
    {
        bool matches = true;
        for (int entry = 0; entry < DebugOverlay::EEntry_Count && matches; ++entry)
            matches = DebugOverlay::GetState(m_playerId, entry) == PresetState(preset, entry);
        if (matches)
            return preset;
    }
    return -1;
}

void UIScene_DebugOverlayMenu::MoveFocus(int step)
{
    m_focus = (m_focus + step + DebugOverlay::EEntry_Count) % DebugOverlay::EEntry_Count;
    gConsoleUIController.PlayUISFX(SoundEvents::UI_FOCUS);
}

void UIScene_DebugOverlayMenu::ChangeState(int step)
{
    const int current = DisplayIndex(DebugOverlay::GetState(m_playerId, m_focus));
    const int next = (current + step + DebugOverlay::EState_Count) % DebugOverlay::EState_Count;
    SetEntryState(m_focus, kDisplayOrder[next]);
}

void UIScene_DebugOverlayMenu::SetEntryState(int entry, int state)
{
    DebugOverlay::SetState(m_playerId, entry, state);
    m_pendingTime[entry] = 0;
    m_changeTime[entry] = System::processTimeInMilliSecs();
    const int matching = MatchingPreset();
    if (matching >= 0)
        m_preset = matching;
    gConsoleUIController.PlayUISFX(SoundEvents::UI_PRESS);
}

void UIScene_DebugOverlayMenu::ApplyPreset(int preset)
{
    m_preset = (preset + ePreset_Count) % ePreset_Count;
    const long long now = System::processTimeInMilliSecs();
    int order = 0;
    for (int entry = 0; entry < DebugOverlay::EEntry_Count; ++entry)
    {
        const int target = PresetState(m_preset, entry);
        if (target == DebugOverlay::GetState(m_playerId, entry))
        {
            m_pendingTime[entry] = 0;
            continue;
        }
        m_pendingState[entry] = target;
        m_pendingTime[entry] = now + 1 + order * 35;
        ++order;
    }
    gConsoleUIController.PlayUISFX(SoundEvents::UI_SCROLL);
}

void UIScene_DebugOverlayMenu::handleInput(int, int action, bool repeat, bool pressed, bool, bool& handled)
{
    if (!pressed)
        return;
    switch (action)
    {
    case EControllerActions_MenuUp:
        m_mouseScroll = false;
        MoveFocus(-1);
        break;
    case EControllerActions_MenuDown:
        m_mouseScroll = false;
        MoveFocus(1);
        break;
    case EControllerActions_MenuLeft:
        if (!repeat)
            ChangeState(-1);
        break;
    case EControllerActions_MenuRight:
    case EControllerActions_MenuOk:
        if (!repeat)
            ChangeState(1);
        break;
    case EControllerActions_MenuX:
        if (!repeat)
        {
            m_previewOpen = !m_previewOpen;
            gConsoleUIController.PlayUISFX(SoundEvents::UI_PRESS);
        }
        break;
    case EControllerActions_MenuLeftScroll:
        if (!repeat)
            ApplyPreset(m_preset - 1);
        break;
    case EControllerActions_MenuRightScroll:
        if (!repeat)
            ApplyPreset(m_preset + 1);
        break;
    case EControllerActions_MenuY:
        if (!repeat)
            ApplyPreset(ePreset_Default);
        break;
    case EControllerActions_MenuCancel:
        if (!repeat)
        {
            gConsoleUIController.PlayUISFX(SoundEvents::UI_BACK);
            GameSettings::CheckGameSettingsChanged(true, m_playerId);
            navigateBack();
        }
        break;
    default:
        return;
    }
    handled = true;
}

void UIScene_DebugOverlayMenu::Animate()
{
    const long long now = System::processTimeInMilliSecs();
    float elapsed = m_lastTime == 0 ? 1000.0f : static_cast<float>(now - m_lastTime);
    if (elapsed < 0.0f)
        elapsed = 0.0f;
    if (elapsed > 250.0f)
        elapsed = 250.0f;
    m_lastTime = now;
    m_elapsed = elapsed;

    for (int entry = 0; entry < DebugOverlay::EEntry_Count; ++entry)
    {
        if (m_pendingTime[entry] != 0 && now >= m_pendingTime[entry])
        {
            DebugOverlay::SetState(m_playerId, entry, m_pendingState[entry]);
            m_pendingTime[entry] = 0;
            m_changeTime[entry] = now;
        }
        const float target = DebugOverlay::IsShown(m_playerId, entry, m_previewOpen) ? 1.0f : 0.0f;
        const float step = elapsed / 180.0f;
        float& amount = m_look.amount[entry];
        amount = amount < target ? (amount + step > target ? target : amount + step)
                                 : (amount - step < target ? target : amount - step);
        const float flash = 1.0f - static_cast<float>(now - m_changeTime[entry]) / 600.0f;
        m_look.flash[entry] = m_changeTime[entry] != 0 && flash > 0.0f ? flash * 0.7f : 0.0f;
    }
    m_look.highlight = m_focus;
    m_pulse = static_cast<float>(now % 1200) / 1200.0f;
    m_look.pulse = 0.5f + 0.5f * sinf(static_cast<float>(now % 1000) / 1000.0f * 2.0f * kPi);

    const int focusTop = RowTop(m_focus);
    m_focusY = MenuApproach(m_focusY, static_cast<float>(focusTop), elapsed, 55.0f);
    const int visible = (ListBottom() - kListTop) - kListInset * 2;
    float scrollTarget =
        m_mouseScroll ? m_scrollTarget : static_cast<float>(focusTop) - visible * 0.5f + kRowHeight * 0.5f;
    const float maximum = static_cast<float>(ContentHeight() - visible);
    if (scrollTarget > maximum)
        scrollTarget = maximum;
    if (scrollTarget < 0.0f)
        scrollTarget = 0.0f;
    if (m_mouseScroll)
        m_scrollTarget = scrollTarget;
    m_scroll = MenuApproach(m_scroll, scrollTarget, elapsed, 90.0f);

    float carouselTarget = static_cast<float>(m_preset);
    while (carouselTarget - m_carousel > ePreset_Count * 0.5f)
        carouselTarget -= ePreset_Count;
    while (m_carousel - carouselTarget > ePreset_Count * 0.5f)
        carouselTarget += ePreset_Count;
    m_carousel = MenuApproach(m_carousel, carouselTarget, elapsed, 120.0f);
    if (fabsf(m_carousel - carouselTarget) < 0.001f)
        m_carousel = static_cast<float>(m_preset);

    const bool shown = DebugOverlay::IsShown(m_playerId, m_focus, m_previewOpen);
    m_zoom = MenuApproach(m_zoom, shown ? 1.0f : 0.62f, elapsed, 140.0f);
}

void UIScene_DebugOverlayMenu::DrawSwitch(MenuPainter& painter, int x, int y, int entry)
{
    const int state = DebugOverlay::GetState(m_playerId, entry);
    const long long since = System::processTimeInMilliSecs() - m_changeTime[entry];
    for (int index = 0; index < DebugOverlay::EState_Count; ++index)
    {
        const int option = kDisplayOrder[index];
        const bool selected = option == state;
        const int x0 = x + index * (kSwitchWidth + 1);
        float scale = 1.0f;
        if (selected && m_changeTime[entry] != 0 && since < 300)
            scale = 1.0f + 0.25f * sinf(static_cast<float>(since) / 300.0f * kPi);
        GlStateManager::pushMatrix();
        GlStateManager::translatef(x0 + kSwitchWidth * 0.5f, y + kSwitchHeight * 0.5f, 0.0f);
        GlStateManager::scalef(scale, scale, 1.0f);
        const int left = -kSwitchWidth / 2;
        const int top = -kSwitchHeight / 2;
        painter.SwitchSegment(left, top, left + kSwitchWidth, top + kSwitchHeight, DebugOverlay::StateName(option),
                              selected, SwitchColour(option), SwitchEdge(option));
        GlStateManager::popMatrix();
    }
}

void UIScene_DebugOverlayMenu::DrawList(MenuPainter& painter)
{
    const int listBottom = ListBottom();
    painter.Bevel(kListLeft, kListTop, kListRight, listBottom, kPanelFill, MenuColour::kPanelLight, kPanelShade);

    const int innerLeft = kListLeft + kListInset;
    const int innerTop = kListTop + kListInset;
    const int innerRight = kListRight - kListInset - 4;
    const int innerBottom = listBottom - kListInset;
    m_frame.Clip(innerLeft - 6, innerTop, innerRight + 6, innerBottom);
    const int offset = innerTop - static_cast<int>(m_scroll + 0.5f);

    int group = -1;
    for (int entry = 0; entry < DebugOverlay::EEntry_Count; ++entry)
    {
        const int rowTop = offset + RowTop(entry);
        if (DebugOverlay::Group(entry) != group)
        {
            group = DebugOverlay::Group(entry);
            painter.Text(DebugOverlay::GroupName(group), innerLeft + 2, rowTop - kCategoryHeight + 3,
                         MenuColour::kTextDark, false);
        }
        Row(painter, innerLeft, rowTop, innerRight, rowTop + kRowHeight, kRowFill, kRowLight, kPanelShade);
    }

    const int focusTop = offset + static_cast<int>(m_focusY + 0.5f);
    painter.Ring(innerLeft, focusTop, innerRight, focusTop + kRowHeight, m_pulse);
    Row(painter, innerLeft, focusTop, innerRight, focusTop + kRowHeight, kRowFocusFill, kRowFocusLight,
        kRowFocusShade);

    for (int entry = 0; entry < DebugOverlay::EEntry_Count; ++entry)
    {
        const int rowTop = offset + RowTop(entry);
        const bool focused = entry == m_focus;
        painter.Text(DebugOverlay::EntryName(entry), innerLeft + 5, rowTop + 5,
                     focused ? MenuColour::kTextYellow : MenuColour::kTextLight, true);
        DrawSwitch(painter, innerRight - 4 - kSwitchWidth * 3 - 2, rowTop + 3, entry);
    }
    m_frame.Unclip();

    painter.Scrollbar(innerRight + 3, innerTop, innerBottom, ContentHeight(), m_scroll, MenuColour::kScrollTrack,
                      MenuColour::kScrollThumb);
}

void UIScene_DebugOverlayMenu::DrawPresets(MenuPainter& painter)
{
    const int presetTop = PresetTop();
    const int presetBottom = m_frame.bottom;
    painter.Bevel(kListLeft, presetTop, kListRight, presetBottom, kPanelFill, MenuColour::kPanelLight, kPanelShade);

    const int arrowWidth = painter.Width(L">");
    const int stripLeft = kListLeft + 6 + arrowWidth + 4;
    const int stripRight = kListRight - 6 - arrowWidth - 4;
    const int middleY = (presetTop + presetBottom) / 2;
    painter.Text(L"<", kListLeft + 6, middleY - 4, MenuColour::kTextDark, false);
    painter.Text(L">", kListRight - 6 - arrowWidth, middleY - 4, MenuColour::kTextDark, false);
    m_frame.Clip(stripLeft, presetTop + 2, stripRight, presetBottom - 2);

    const float centreX = (stripLeft + stripRight) * 0.5f;
    float offsets[ePreset_Count];
    int order[ePreset_Count];
    for (int preset = 0; preset < ePreset_Count; ++preset)
    {
        float offset = preset - m_carousel;
        while (offset > ePreset_Count * 0.5f)
            offset -= ePreset_Count;
        while (offset < -ePreset_Count * 0.5f)
            offset += ePreset_Count;
        offsets[preset] = offset;
        order[preset] = preset;
    }
    for (int i = 0; i < ePreset_Count; ++i)
    {
        for (int j = i + 1; j < ePreset_Count; ++j)
        {
            if (fabsf(offsets[order[j]]) > fabsf(offsets[order[i]]))
            {
                const int swap = order[i];
                order[i] = order[j];
                order[j] = swap;
            }
        }
    }

    for (int index = 0; index < ePreset_Count; ++index)
    {
        const int preset = order[index];
        const float offset = offsets[preset];
        if (fabsf(offset) > 2.0f)
            continue;
        const float distance = fabsf(offset) < 1.0f ? fabsf(offset) : 1.0f;
        const float closeness = 1.0f - distance;
        const std::wstring name = PresetName(preset);
        const int width = painter.Width(name) + 12 + static_cast<int>(10.0f * closeness);
        const int height = 20 + static_cast<int>(6.0f * closeness);
        const int x = static_cast<int>(centreX + offset * kPresetSpacing);
        const int x0 = x - width / 2;
        const int y0 = middleY - height / 2;
        const bool current = preset == m_preset && closeness > 0.5f;
        if (current)
            Row(painter, x0, y0, x0 + width, y0 + height, kRowFocusFill, kRowFocusLight, kRowFocusShade);
        else
            Row(painter, x0, y0, x0 + width, y0 + height, kRowFill, kRowLight, kPanelShade);
        painter.Text(name, x - painter.Width(name) / 2, middleY - 4,
                     current ? MenuColour::kTextYellow : MenuColour::kTextLight, true);
        if (fabsf(offset) > 1.0f)
        {
            const float fade = fabsf(offset) - 1.0f;
            const float alpha = (fade > 1.0f ? 1.0f : fade) * 0.78f;
            painter.Fill(x0, y0, x0 + width, y0 + height, MenuPainter::WithAlpha(kPanelFill, alpha));
        }
    }
    m_frame.Unclip();
}

void UIScene_DebugOverlayMenu::DrawPreview(MenuPainter& painter, Font& font)
{
    const int previewBottom = m_frame.bottom;
    const int left = kPreviewLeft + kListInset;
    const int right = kPreviewRight - kListInset;
    const int viewTop = kViewportTop;
    const int viewBottom = previewBottom - kListInset;
    painter.Outline(kPreviewLeft, kPreviewTop, kPreviewRight, previewBottom, MenuColour::kBlack);
    painter.Fill(kPreviewLeft + 1, kPreviewTop + 1, kPreviewRight - 1, viewTop, kPanelFill);
    painter.Fill(kPreviewLeft + 1, viewBottom, kPreviewRight - 1, previewBottom - 1, kPanelFill);
    painter.Fill(kPreviewLeft + 1, viewTop, left, viewBottom, kPanelFill);
    painter.Fill(right, viewTop, kPreviewRight - 1, viewBottom, kPanelFill);
    painter.Fill(kPreviewLeft + 1, kPreviewTop + 1, kPreviewRight - 1, kPreviewTop + 2, MenuColour::kPanelLight);
    painter.Fill(kPreviewLeft + 1, kPreviewTop + 1, kPreviewLeft + 2, previewBottom - 1, MenuColour::kPanelLight);
    painter.Fill(kPreviewLeft + 1, previewBottom - 2, kPreviewRight - 1, previewBottom - 1, kPanelShade);
    painter.Fill(kPreviewRight - 2, kPreviewTop + 1, kPreviewRight - 1, previewBottom - 1, kPanelShade);

    const int tabTop = kPreviewTop + kListInset;
    const int middle = (left + right) / 2;
    const wchar_t* const tabs[2] = {L"Playing", L"F3 open"};
    for (int tab = 0; tab < 2; ++tab)
    {
        const bool selected = (tab == 1) == m_previewOpen;
        const int x0 = tab == 0 ? left : middle;
        const int x1 = tab == 0 ? middle : right;
        painter.Fill(x0, tabTop, x1, tabTop + kTabHeight, MenuColour::kBlack);
        painter.Fill(x0 + 1, tabTop + 1, x1 - 1, tabTop + kTabHeight - 1, selected ? kRowFocusFill : kTabFill);
        const unsigned int colour = selected ? MenuColour::kTextYellow : MenuColour::kTextDark;
        painter.Centred(tabs[tab], (x0 + x1) / 2, tabTop + 4, colour, selected);
    }

    painter.Outline(left, viewTop, right, viewBottom, MenuColour::kBlack);
    painter.Fill(left + 1, viewTop + 1, right - 1, viewBottom - 1, kPreviewShade);

    std::wstring values[DebugOverlay::EEntry_Count];
    DebugOverlay::Values(m_playerId, values);
    m_frame.Clip(left + 1, viewTop + 1, right - 1, viewBottom - 1);
    int highlightX = -1;
    int highlightY = -1;
    GlStateManager::pushMatrix();
    GlStateManager::translatef(left + 1 + m_cameraX, viewTop + 1 + m_cameraY, 0.0f);
    GlStateManager::scalef(m_zoom, m_zoom, 1.0f);
    DebugOverlay::Draw(m_gui, font, 2, 2, values, m_look, &highlightX, &highlightY);
    GlStateManager::popMatrix();
    m_frame.Unclip();

    const float viewWidth = static_cast<float>(right - left - 2);
    const float viewHeight = static_cast<float>(viewBottom - viewTop - 2);
    float targetX = 4.0f;
    float targetY = 4.0f;
    if (highlightY >= 0 && DebugOverlay::IsShown(m_playerId, m_focus, m_previewOpen))
    {
        targetX = viewWidth * 0.42f - highlightX * m_zoom;
        targetY = viewHeight * 0.45f - highlightY * m_zoom;
        if (targetX > 4.0f)
            targetX = 4.0f;
        if (targetY > 4.0f)
            targetY = 4.0f;
        const float lowestX = viewWidth - kLayoutWidth * m_zoom;
        const float lowestY = viewHeight - kLayoutHeight * m_zoom;
        if (targetX < lowestX)
            targetX = lowestX;
        if (targetY < lowestY)
            targetY = lowestY;
    }
    m_cameraX = MenuApproach(m_cameraX, targetX, m_elapsed, 140.0f);
    m_cameraY = MenuApproach(m_cameraY, targetY, m_elapsed, 140.0f);
}

void UIScene_DebugOverlayMenu::render(int, int, C4JRender::eViewportType viewport)
{
    Minecraft* minecraft = Minecraft::GetInstance();
    if (!m_hasTicked || minecraft == NULL || minecraft->mFont == NULL)
        return;
    Font& font = *minecraft->mFont;
    m_frame.Fit(viewport);
    Animate();

    MenuPainter painter(m_gui, font, m_frame);
    m_frame.Begin();
    painter.Title(L"Debug Overlay", L"More Options", MenuColour::kTextSubtitle);
    DrawList(painter);
    DrawPresets(painter);
    DrawPreview(painter, font);
    m_frame.End();
}
