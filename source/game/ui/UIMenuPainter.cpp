#include "game/ui/UIMenuPainter.hpp"

#include <math.h>
#include <wctype.h>

#include "game/client/render/Font.hpp"
#include "game/client/render/Renderer.hpp"
#include "game/core/DebugOverlay.hpp"
#include "game/core/GlStateManager.hpp"
#include "game/core/GuiComponent.hpp"
#include "platform/CInput.hpp"

namespace
{
const float kPromptBand = 88.0f;
const int kSmallestBottom = 200;
const int kMinFilter = 1;
const wchar_t kFormatSign = static_cast<wchar_t>(0xA7);

std::wstring ActiveFormat(const std::wstring& text)
{
    std::wstring format;
    for (size_t index = 0; index + 1 < text.size(); ++index)
    {
        if (text[index] != kFormatSign)
            continue;
        const wchar_t code = static_cast<wchar_t>(towlower(text[index + 1]));
        if ((code >= L'0' && code <= L'9') || (code >= L'a' && code <= L'f'))
            format = std::wstring(1, kFormatSign) + code;
        else if (code == L'r')
            format.clear();
        else if (code >= L'k' && code <= L'o')
            format += std::wstring(1, kFormatSign) + code;
        ++index;
    }
    return format;
}

unsigned int ScissorEdge(float value)
{
    return value > 0.0f ? static_cast<unsigned int>(floorf(value + 0.5f)) : 0u;
}
}

MenuFrame::MenuFrame()
    : originX(0.0f),
      originY(0.0f),
      unit(1.0f),
      height(kHeight),
      bottom(kHeight - 44),
      m_areaX(0),
      m_areaY(0),
      m_areaWidth(kWidth),
      m_areaHeight(kHeight)
{
    for (int index = 0; index < 4; ++index)
        m_savedScissor[index] = 0;
}

void MenuFrame::Fit(int viewport)
{
    DebugOverlay::ViewportArea(viewport, &m_areaX, &m_areaY, &m_areaWidth, &m_areaHeight);
    const float widthFit = static_cast<float>(m_areaWidth) / kWidth;
    const float heightFit = static_cast<float>(m_areaHeight) / kHeight;
    const float fit = widthFit < heightFit ? widthFit : heightFit;
    const float pixels = DebugOverlay::PixelScale();
    // Font glyphs are 16 texels across 8 units, so text is only sharp at an even number of pixels per unit.
    if (fit * pixels >= 2.0f)
        unit = 2.0f * floorf(fit * pixels * 0.5f) / pixels;
    else if (fit * pixels >= 1.0f)
        unit = 1.0f / pixels;
    else
        unit = fit > 0.25f ? fit : 0.25f;
    height = static_cast<int>(static_cast<float>(m_areaHeight) / unit);
    if (height < kHeight)
        height = kHeight;
    bottom = height - static_cast<int>(ceilf(kPromptBand / unit));
    if (bottom < kSmallestBottom)
        bottom = kSmallestBottom;
    originX = DebugOverlay::SnapToPixel(m_areaX + (m_areaWidth - kWidth * unit) * 0.5f);
    originY = DebugOverlay::SnapToPixel(m_areaY + (m_areaHeight - height * unit) * 0.5f);
}

MenuRect MenuFrame::Area() const
{
    MenuRect area;
    area.x0 = static_cast<int>(floorf((m_areaX - originX) / unit));
    area.y0 = static_cast<int>(floorf((m_areaY - originY) / unit));
    area.x1 = static_cast<int>(ceilf((m_areaX + m_areaWidth - originX) / unit));
    area.y1 = static_cast<int>(ceilf((m_areaY + m_areaHeight - originY) / unit));
    return area;
}

void MenuFrame::Begin()
{
    gRenderer->StateGetScissorRect(m_savedScissor[0], m_savedScissor[1], m_savedScissor[2], m_savedScissor[3]);
    DebugOverlay::BeginScreenDraw(originX, originY, unit);
}

void MenuFrame::End()
{
    Unclip();
    DebugOverlay::EndScreenDraw();
}

void MenuFrame::Clip(int x0, int y0, int x1, int y1)
{
    gRenderer->StateSetScissorRect(ScissorEdge(originX + x0 * unit), ScissorEdge(originY + y0 * unit),
                                   ScissorEdge(originX + x1 * unit), ScissorEdge(originY + y1 * unit));
}

void MenuFrame::Unclip()
{
    gRenderer->StateSetScissorRect(m_savedScissor[0], m_savedScissor[1], m_savedScissor[2], m_savedScissor[3]);
}

bool MenuFrame::Pointer(int pad, float* x, float* y) const
{
#if LCE_PLATFORM_PC
    float pointerX = 0.0f;
    float pointerY = 0.0f;
    if (unit <= 0.0f || !g_pInput->GetMenuPointer(pad, &pointerX, &pointerY))
        return false;
    *x = (pointerX - originX) / unit;
    *y = (pointerY - originY) / unit;
    return true;
#else
    (void)pad;
    (void)x;
    (void)y;
    return false;
#endif
}

MenuPainter::MenuPainter(GuiComponent& gui, Font& font, const MenuFrame& frame)
    : m_gui(gui), m_font(font), m_smoothText(frame.unit * DebugOverlay::PixelScale() < 1.99f)
{
    if (!m_smoothText)
        return;
    m_font.bindTexture();
    gRenderer->TextureSetParam(kMinFilter, 1);
}

MenuPainter::~MenuPainter()
{
    if (!m_smoothText)
        return;
    m_font.bindTexture();
    gRenderer->TextureSetParam(kMinFilter, 0);
}

void MenuPainter::Fill(int x0, int y0, int x1, int y1, unsigned int colour)
{
    if (x1 > x0 && y1 > y0)
        m_gui.fill(x0, y0, x1, y1, static_cast<int>(colour));
}

void MenuPainter::Gradient(int x0, int y0, int x1, int y1, unsigned int top, unsigned int bottom)
{
    m_gui.fillGradient(x0, y0, x1, y1, static_cast<int>(top), static_cast<int>(bottom));
}

void MenuPainter::Outline(int x0, int y0, int x1, int y1, unsigned int colour)
{
    Fill(x0, y0, x1, y0 + 1, colour);
    Fill(x0, y1 - 1, x1, y1, colour);
    Fill(x0, y0 + 1, x0 + 1, y1 - 1, colour);
    Fill(x1 - 1, y0 + 1, x1, y1 - 1, colour);
}

void MenuPainter::Bevel(int x0, int y0, int x1, int y1, unsigned int fill, unsigned int light, unsigned int shade)
{
    Outline(x0, y0, x1, y1, MenuColour::kBlack);
    Fill(x0 + 1, y0 + 1, x1 - 1, y1 - 1, fill);
    Fill(x0 + 1, y0 + 1, x1 - 1, y0 + 2, light);
    Fill(x0 + 1, y0 + 2, x0 + 2, y1 - 2, light);
    Fill(x0 + 1, y1 - 2, x1 - 1, y1 - 1, shade);
    Fill(x1 - 2, y0 + 2, x1 - 1, y1 - 2, shade);
}

void MenuPainter::Panel(int x0, int y0, int x1, int y1)
{
    using namespace MenuColour;
    Fill(x0 + 2, y0, x1 - 2, y0 + 1, kBlack);
    Fill(x0 + 2, y1 - 1, x1 - 2, y1, kBlack);
    Fill(x0, y0 + 2, x0 + 1, y1 - 2, kBlack);
    Fill(x1 - 1, y0 + 2, x1, y1 - 2, kBlack);
    Fill(x0 + 2, y0 + 1, x1 - 2, y1 - 1, kPanelFill);
    Fill(x0 + 1, y0 + 2, x0 + 2, y1 - 2, kPanelFill);
    Fill(x1 - 2, y0 + 2, x1 - 1, y1 - 2, kPanelFill);
    Fill(x0 + 2, y0 + 1, x1 - 2, y0 + 2, kPanelLight);
    Fill(x0 + 1, y0 + 2, x0 + 2, y1 - 2, kPanelLight);
    Fill(x0 + 2, y1 - 2, x1 - 2, y1 - 1, kPanelShade);
    Fill(x1 - 2, y0 + 2, x1 - 1, y1 - 2, kPanelShade);
    Fill(x0 + 1, y0 + 1, x0 + 2, y0 + 2, kBlack);
    Fill(x1 - 2, y0 + 1, x1 - 1, y0 + 2, kBlack);
    Fill(x0 + 1, y1 - 2, x0 + 2, y1 - 1, kBlack);
    Fill(x1 - 2, y1 - 2, x1 - 1, y1 - 1, kBlack);
}

void MenuPainter::Button(int x0, int y0, int x1, int y1, bool focused, bool enabled)
{
    using namespace MenuColour;
    if (!enabled)
        Bevel(x0, y0, x1, y1, kDisabledFill, kDisabledLight, kDisabledShade);
    else if (focused)
        Bevel(x0, y0, x1, y1, kFocusFill, kFocusLight, kFocusShade);
    else
        Bevel(x0, y0, x1, y1, kButtonFill, kButtonLight, kButtonShade);
}

void MenuPainter::Ring(int x0, int y0, int x1, int y1, float pulse)
{
    const int grow = 1 + static_cast<int>(pulse * 3.0f);
    const unsigned int alpha = static_cast<unsigned int>((1.0f - pulse) * 200.0f);
    Outline(x0 - grow, y0 - grow, x1 + grow, y1 + grow, (alpha << 24) | MenuColour::kRing);
}

void MenuPainter::Highlight(int x0, int y0, int x1, int y1, float amount)
{
    if (amount > 0.0f)
        Fill(x0, y0, x1, y1, WithAlpha(MenuColour::kTextWhite, amount * 0.35f));
}

void MenuPainter::Box(int x0, int y0, int x1, int y1, bool focused)
{
    using namespace MenuColour;
    Outline(x0, y0, x1, y1, kBoxEdge);
    Fill(x0 + 1, y0 + 1, x1 - 1, y1 - 1, focused ? kBoxFocusFill : kBoxFill);
    Fill(x0 + 1, y0 + 1, x1 - 1, y0 + 2, kBoxInnerShadow);
}

void MenuPainter::Slider(int boxX0, int boxX1, int y1, int position, int count, bool focused, unsigned int colour)
{
    using namespace MenuColour;
    const int left = boxX0 + 4;
    const int right = boxX1 - 4;
    const int handle = count > 1 ? left + (right - left) * position / (count - 1) : left;
    Fill(left, y1 - 6, right, y1 - 5, kSliderTrack);
    Fill(left, y1 - 6, handle, y1 - 5, WithAlpha(colour, 0.85f));
    Fill(handle - 2, y1 - 8, handle + 2, y1 - 3, kBlack);
    Fill(handle - 1, y1 - 7, handle + 1, y1 - 4, focused ? kFocusLight : kHandleFill);
}

void MenuPainter::Description(int x0, int y0, int x1, int y1)
{
    Fill(x0 + 1, y0 + 1, x1 - 1, y1 - 1, MenuColour::kDescriptionFill);
    Outline(x0, y0, x1, y1, MenuColour::kDescriptionEdge);
}

void MenuPainter::Scrollbar(int x, int y0, int y1, int content, float scroll, unsigned int track, unsigned int thumb)
{
    const int visible = y1 - y0;
    if (content <= visible || visible <= 0)
        return;
    int barHeight = visible * visible / content;
    if (barHeight < 8)
        barHeight = 8;
    const int barTop = y0 + static_cast<int>((visible - barHeight) * scroll / (content - visible));
    Fill(x, y0, x + 3, y1, track);
    Fill(x, barTop, x + 3, barTop + barHeight, thumb);
}

void MenuPainter::SwitchSegment(int x0, int y0, int x1, int y1, const std::wstring& label, bool chosen,
                                unsigned int fill, unsigned int edge)
{
    using namespace MenuColour;
    Fill(x0, y0, x1, y1, chosen ? edge : kSwitchEdge);
    Fill(x0 + 1, y0 + 1, x1 - 1, y1 - 1, chosen ? fill : kSwitchFill);
    Centred(label, (x0 + x1) / 2, y0 + (y1 - y0 - 7) / 2, chosen ? kTextWhite : kSwitchText, chosen);
}

int MenuPainter::Chip(int right, int y, const std::wstring& text, unsigned int fill, unsigned int edge)
{
    const int left = right - Width(text) - 8;
    Outline(left, y, right, y + 11, edge);
    Fill(left + 1, y + 1, right - 1, y + 10, fill);
    Text(text, left + 4, y + 2, MenuColour::kTextWhite, true);
    return left;
}

void MenuPainter::Title(const std::wstring& title, const std::wstring& subtitle, unsigned int subtitleColour)
{
    GlStateManager::pushMatrix();
    GlStateManager::translatef(MenuFrame::kWidth * 0.5f, 6.0f, 0.0f);
    GlStateManager::scalef(2.0f, 2.0f, 1.0f);
    Text(title, -Width(title) / 2, 0, MenuColour::kTextWhite, true);
    GlStateManager::popMatrix();
    Centred(Trim(subtitle, MenuFrame::kWidth - 200), MenuFrame::kWidth / 2, 26, subtitleColour, true);
}

int MenuPainter::Width(const std::wstring& text)
{
    return m_font.width(text);
}

void MenuPainter::Text(const std::wstring& text, int x, int y, unsigned int colour, bool shadow)
{
    GlStateManager::enableBlend();
    GlStateManager::blendFunc(4, 5);
    GlStateManager::enableTexture();
    if (shadow)
        m_font.drawShadow(text, x, y, static_cast<int>(colour));
    else
        m_font.draw(text, x, y, static_cast<int>(colour));
}

void MenuPainter::Centred(const std::wstring& text, int centreX, int y, unsigned int colour, bool shadow)
{
    Text(text, centreX - Width(text) / 2, y, colour, shadow);
}

std::wstring MenuPainter::Trim(const std::wstring& text, int width)
{
    if (Width(text) <= width)
        return text;
    const std::wstring dots = L"...";
    const int limit = width - Width(dots);
    std::wstring out;
    for (size_t index = 0; index < text.size(); ++index)
    {
        if (text[index] == kFormatSign && index + 1 < text.size())
        {
            out += text[index];
            out += text[++index];
            continue;
        }
        const std::wstring next = out + text[index];
        if (Width(next) > limit)
            break;
        out = next;
    }
    return out + FormatReset() + dots;
}

std::wstring MenuPainter::TrimStart(const std::wstring& text, int width)
{
    if (Width(text) <= width)
        return text;
    const std::wstring dots = L"...";
    std::wstring out = text;
    while (!out.empty() && Width(dots + out) > width)
        out.erase(0, 1);
    return dots + out;
}

std::vector<std::wstring> MenuPainter::Wrap(const std::wstring& text, int width)
{
    std::vector<std::wstring> lines;
    size_t start = 0;
    while (start <= text.size())
    {
        size_t end = text.find(L'\n', start);
        if (end == std::wstring::npos)
            end = text.size();
        WrapLine(text.substr(start, end - start), width, lines);
        start = end + 1;
    }
    return lines;
}

void MenuPainter::WrapLine(const std::wstring& text, int width, std::vector<std::wstring>& lines)
{
    std::wstring line;
    std::wstring carried;
    size_t start = 0;
    while (start <= text.size())
    {
        size_t end = text.find(L' ', start);
        if (end == std::wstring::npos)
            end = text.size();
        const std::wstring word = text.substr(start, end - start);
        const std::wstring candidate = line.empty() ? carried + word : line + L" " + word;
        if (Width(candidate) <= width || line.empty())
        {
            line = candidate;
            while (Width(line) > width && line.size() > 1)
            {
                size_t cut = line.size();
                while (cut > 1 && Width(line.substr(0, cut)) > width)
                    --cut;
                lines.push_back(line.substr(0, cut));
                carried = ActiveFormat(line.substr(0, cut));
                line = carried + line.substr(cut);
            }
        }
        else
        {
            lines.push_back(line);
            carried = ActiveFormat(line);
            line = carried + word;
        }
        start = end + 1;
    }
    if (!line.empty() || lines.empty())
        lines.push_back(line);
}

unsigned int MenuPainter::WithAlpha(unsigned int colour, float alpha)
{
    if (alpha >= 1.0f)
        return colour;
    const unsigned int value = static_cast<unsigned int>(((colour >> 24) & 255) * (alpha < 0.0f ? 0.0f : alpha));
    return (colour & 0x00FFFFFF) | (value << 24);
}

std::wstring MenuPainter::FormatReset()
{
    return std::wstring(1, kFormatSign) + L"r";
}

float MenuApproach(float value, float target, float milliseconds, float timeConstant)
{
    return value + (target - value) * (1.0f - expf(-milliseconds / timeConstant));
}
