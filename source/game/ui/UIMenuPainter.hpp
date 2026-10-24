#ifndef GAME_UI_UIMENUPAINTER_HPP
#define GAME_UI_UIMENUPAINTER_HPP

#include <string>
#include <vector>

class Font;
class GuiComponent;

namespace MenuColour
{
const unsigned int kBlack = 0xFF000000;
const unsigned int kPanelFill = 0xFFD0D0D0;
const unsigned int kPanelLight = 0xFFFFFFFF;
const unsigned int kPanelShade = 0xFF717171;
const unsigned int kButtonFill = 0xFF848484;
const unsigned int kButtonLight = 0xFFB8B8B8;
const unsigned int kButtonShade = 0xFF585858;
const unsigned int kFocusFill = 0xFF919ACA;
const unsigned int kFocusLight = 0xFFC9D0FF;
const unsigned int kFocusShade = 0xFF757FB1;
const unsigned int kDisabledFill = 0xFF6E6E6E;
const unsigned int kDisabledLight = 0xFF808080;
const unsigned int kDisabledShade = 0xFF5A5A5A;
const unsigned int kBoxFill = 0xFF373737;
const unsigned int kBoxFocusFill = 0xFF323A6E;
const unsigned int kBoxEdge = 0xFF1C1C1C;
const unsigned int kBoxInnerShadow = 0x60000000;
const unsigned int kSliderTrack = 0xFF1E1E1E;
const unsigned int kHandleFill = 0xFFB4B4B4;
const unsigned int kDescriptionFill = 0xD8394A52;
const unsigned int kDescriptionEdge = 0xFFE4E4E4;
const unsigned int kScrollTrack = 0xFF8B8B8B;
const unsigned int kScrollThumb = 0xFF555555;
const unsigned int kRing = 0x00FFFF55;
const unsigned int kSwitchEdge = 0xFF1D1D1D;
const unsigned int kSwitchFill = 0xFF3B3B3B;
const unsigned int kSwitchText = 0xFF9A9A9A;
const unsigned int kChipGreen = 0xFF3C8527;
const unsigned int kChipGreenEdge = 0xFF1D4D13;
const unsigned int kChipAmber = 0xFFC79A1E;
const unsigned int kChipAmberEdge = 0xFF6E5310;
const unsigned int kChipRed = 0xFF8A2B2B;
const unsigned int kChipRedEdge = 0xFF4A1414;
const unsigned int kChipGrey = 0xFF4E4E4E;
const unsigned int kChipGreyEdge = 0xFF262626;
const unsigned int kTextWhite = 0xFFFFFFFF;
const unsigned int kTextLight = 0xFFE0E0E0;
const unsigned int kTextYellow = 0xFFFFFF55;
const unsigned int kTextDark = 0xFF404040;
const unsigned int kTextSubtitle = 0xFFAAAAAA;
const unsigned int kTextDisabled = 0xFFB0B0B0;
}

struct MenuRect
{
    int x0;
    int y0;
    int x1;
    int y1;
};

class MenuFrame
{
public:
    static const int kWidth = 640;
    static const int kHeight = 360;

    MenuFrame();

    void Fit(int viewport);
    void Begin();
    void End();
    void Clip(int x0, int y0, int x1, int y1);
    void Unclip();
    bool Pointer(int pad, float* x, float* y) const;
    MenuRect Area() const;

    float originX;
    float originY;
    float unit;
    int height;
    int bottom;

private:
    int m_areaX;
    int m_areaY;
    int m_areaWidth;
    int m_areaHeight;
    unsigned int m_savedScissor[4];
};

class MenuPainter
{
public:
    MenuPainter(GuiComponent& gui, Font& font, const MenuFrame& frame);
    ~MenuPainter();

    void Fill(int x0, int y0, int x1, int y1, unsigned int colour);
    void Gradient(int x0, int y0, int x1, int y1, unsigned int top, unsigned int bottom);
    void Outline(int x0, int y0, int x1, int y1, unsigned int colour);
    void Bevel(int x0, int y0, int x1, int y1, unsigned int fill, unsigned int light, unsigned int shade);
    void Panel(int x0, int y0, int x1, int y1);
    void Button(int x0, int y0, int x1, int y1, bool focused, bool enabled);
    void Ring(int x0, int y0, int x1, int y1, float pulse);
    void Highlight(int x0, int y0, int x1, int y1, float amount);
    void Box(int x0, int y0, int x1, int y1, bool focused);
    void Slider(int boxX0, int boxX1, int y1, int position, int count, bool focused, unsigned int colour);
    void Description(int x0, int y0, int x1, int y1);
    void Scrollbar(int x, int y0, int y1, int content, float scroll, unsigned int track, unsigned int thumb);
    void SwitchSegment(int x0, int y0, int x1, int y1, const std::wstring& label, bool chosen, unsigned int fill,
                       unsigned int edge);
    int Chip(int right, int y, const std::wstring& text, unsigned int fill, unsigned int edge);
    void Title(const std::wstring& title, const std::wstring& subtitle, unsigned int subtitleColour);

    int Width(const std::wstring& text);
    void Text(const std::wstring& text, int x, int y, unsigned int colour, bool shadow);
    void Centred(const std::wstring& text, int centreX, int y, unsigned int colour, bool shadow);
    std::wstring Trim(const std::wstring& text, int width);
    std::wstring TrimStart(const std::wstring& text, int width);
    std::vector<std::wstring> Wrap(const std::wstring& text, int width);

    static unsigned int WithAlpha(unsigned int colour, float alpha);
    static std::wstring FormatReset();

private:
    void WrapLine(const std::wstring& text, int width, std::vector<std::wstring>& lines);

    GuiComponent& m_gui;
    Font& m_font;
    bool m_smoothText;
};

float MenuApproach(float value, float target, float milliseconds, float timeConstant);

#endif
