#include "platform/pc/PcPromptArt.hpp"

#include <SDL3/SDL.h>
#include <stb_image.h>

#include <stdio.h>

#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <vector>

#include "platform/pc/KeyboardMouse.hpp"
#include "platform/pc/Log.hpp"
#include "platform/pc/PcPaths.hpp"
#include "platform/pc/Renderer.hpp"
#include "vendor/stb_truetype.hpp"

namespace
{
const Logger sLog("PromptArt");
const int kArtScale = 4;
const int kKeyHeight = 14;
const int kKeyMinimumWidth = 13;
const int kKeyEdge = 4;
const int kLabelLeft = 3;
const int kLabelTop = 3;
const int kFontCell = 8;
const int kFontPixelsPerEm = 10;
const int kGlyphRows = 9;
const int kCapRows = 7;
const int kMouseIconWidth = 11;
const int kMouseRestingIcon = 5;
const int kSymbolSize = 9;
const int kArrowSize = 7;
const int kMouseInputBase = PcKeyboardMouse::SCANCODE_COUNT;
const int kTouchButton = 20;
const int kTouchIcon = 12;
const int kTouchPad = 48;

struct Colour
{
    unsigned char r;
    unsigned char g;
    unsigned char b;
};

const Colour kKeyLabel = {56, 56, 56};
const Colour kPadLabel = {236, 236, 236};
const Colour kPadOutline = {20, 20, 20};
const Colour kDPadLit = {255, 216, 64};

enum EShape
{
    EShape_Round,
    EShape_Stick,
    EShape_Bumper,
    EShape_Trigger,
    EShape_Pill,
    EShape_DPad,
    EShape_Count
};

const char* const kShapeFiles[EShape_Count] = {"pad_round.png",   "pad_stick.png", "pad_bumper.png",
                                               "pad_trigger.png", "pad_pill.png",  "pad_dpad.png"};

enum ESymbol
{
    ESymbol_None = -1,
    ESymbol_Cross,
    ESymbol_Circle,
    ESymbol_Square,
    ESymbol_Triangle,
    ESymbol_Menu,
    ESymbol_View,
    ESymbol_Plus,
    ESymbol_Minus
};

struct Image
{
    int width;
    int height;
    std::vector<unsigned char> rgba;

    Image() : width(0), height(0) {}
};

struct Art
{
    Image image;
    int texture;
};

struct PadButton
{
    EShape shape;
    std::string label;
    ESymbol symbol;
    Colour face;
    Colour mark;
};

struct Resources
{
    bool loaded;
    bool available;
    Image keycap;
    Image mouse;
    Image arrows;
    Image symbols;
    Image shapes[EShape_Count];
    Image touch;
    std::vector<unsigned char> fontData;
    stbtt_fontinfo font;
    std::map<int, Art> art;

    Resources() : loaded(false), available(false) {}
};

Resources sResources;

bool LoadImage(const std::string& name, Image* image)
{
    std::string path = PcResourceRoot() + "/ui/" + name;
    int channels = 0;
    unsigned char* pixels = stbi_load(path.c_str(), &image->width, &image->height, &channels, 4);
    if (pixels == NULL)
    {
        LOG_ERROR(sLog, "PC interface art %s could not be loaded", path.c_str());
        return false;
    }
    image->rgba.assign(pixels, pixels + image->width * image->height * 4);
    stbi_image_free(pixels);
    return true;
}

bool LoadFont()
{
    std::string path = PcContentRoot() + "/Common/Media/font/Mojangles.ttf";
    std::ifstream file(path.c_str(), std::ios::binary);
    if (!file)
    {
        LOG_ERROR(sLog, "Prompt font %s could not be opened", path.c_str());
        return false;
    }
    sResources.fontData.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    if (sResources.fontData.empty()
        || !stbtt_InitFont(&sResources.font, sResources.fontData.data(),
                           stbtt_GetFontOffsetForIndex(sResources.fontData.data(), 0)))
    {
        LOG_ERROR(sLog, "Prompt font %s could not be read", path.c_str());
        return false;
    }
    return true;
}

void LoadResources()
{
    if (sResources.loaded)
        return;
    sResources.loaded = true;
    bool available = LoadImage("keycap.png", &sResources.keycap) && LoadImage("mouse.png", &sResources.mouse)
                     && LoadImage("arrows.png", &sResources.arrows) && LoadImage("symbols.png", &sResources.symbols)
                     && LoadFont();
    for (int shape = 0; available && shape < EShape_Count; ++shape)
        available = LoadImage(kShapeFiles[shape], &sResources.shapes[shape]);
    sResources.available = available && sResources.keycap.height == kKeyHeight
                           && sResources.keycap.width == 2 * kKeyEdge + 1;
}

Image BlankImage(int width, int height)
{
    Image image;
    image.width = width;
    image.height = height;
    image.rgba.assign(width * height * 4, 0);
    return image;
}

const unsigned char* Pixel(const Image& image, int x, int y)
{
    return &image.rgba[(y * image.width + x) * 4];
}

void Copy(const Image& source, int sourceX, int sourceY, Image* target, int targetX, int targetY)
{
    const unsigned char* from = Pixel(source, sourceX, sourceY);
    unsigned char* to = &target->rgba[(targetY * target->width + targetX) * 4];
    to[0] = from[0];
    to[1] = from[1];
    to[2] = from[2];
    to[3] = from[3];
}

void Blend(const Image& source, int sourceX, int sourceY, int width, int height, Image* target, int targetX,
           int targetY)
{
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            const unsigned char* from = Pixel(source, sourceX + x, sourceY + y);
            unsigned char* to = &target->rgba[((targetY + y) * target->width + targetX + x) * 4];
            const int alpha = from[3];
            const int below = to[3] * (255 - alpha) / 255;
            const int total = alpha + below;
            if (total == 0)
                continue;
            for (int channel = 0; channel < 3; ++channel)
                to[channel] = (unsigned char)((from[channel] * alpha + to[channel] * below) / total);
            to[3] = (unsigned char)total;
        }
    }
}

void Plot(Image* target, int x, int y, const Colour& colour)
{
    if (x < 0 || y < 0 || x >= target->width || y >= target->height)
        return;
    unsigned char* to = &target->rgba[(y * target->width + x) * 4];
    to[0] = colour.r;
    to[1] = colour.g;
    to[2] = colour.b;
    to[3] = 255;
}

Colour Shift(const Colour& colour, int amount)
{
    int r = colour.r + amount;
    int g = colour.g + amount;
    int b = colour.b + amount;
    Colour shifted = {(unsigned char)(r < 0 ? 0 : (r > 255 ? 255 : r)),
                      (unsigned char)(g < 0 ? 0 : (g > 255 ? 255 : g)),
                      (unsigned char)(b < 0 ? 0 : (b > 255 ? 255 : b))};
    return shifted;
}

std::vector<std::vector<bool> > RasterLabel(const std::string& text, int* width)
{
    std::vector<std::vector<bool> > columns;
    float scale = stbtt_ScaleForMappingEmToPixels(&sResources.font, (float)(kFontCell * kFontPixelsPerEm));
    for (size_t index = 0; index < text.size(); ++index)
    {
        int codepoint = (unsigned char)text[index];
        int advance = 0;
        int bearing = 0;
        stbtt_GetCodepointHMetrics(&sResources.font, codepoint, &advance, &bearing);
        int cells = (int)(advance * scale / kFontCell + 0.5f);
        int glyphWidth = 0;
        int glyphHeight = 0;
        int offsetX = 0;
        int offsetY = 0;
        unsigned char* bitmap = stbtt_GetCodepointBitmap(&sResources.font, scale, scale, codepoint, &glyphWidth,
                                                         &glyphHeight, &offsetX, &offsetY);
        for (int cell = 0; cell < cells; ++cell)
        {
            std::vector<bool> column(kGlyphRows, false);
            for (int row = 0; row < kGlyphRows; ++row)
            {
                int sampleX = cell * kFontCell + kFontCell / 2 - offsetX;
                int sampleY = (row - kCapRows) * kFontCell + kFontCell / 2 - offsetY;
                if (bitmap != NULL && sampleX >= 0 && sampleY >= 0 && sampleX < glyphWidth && sampleY < glyphHeight)
                    column[row] = bitmap[sampleY * glyphWidth + sampleX] > 127;
            }
            columns.push_back(column);
        }
        if (bitmap != NULL)
            stbtt_FreeBitmap(bitmap, NULL);
    }
    while (!columns.empty())
    {
        bool empty = true;
        for (int row = 0; row < kGlyphRows; ++row)
            empty = empty && !columns.back()[row];
        if (!empty)
            break;
        columns.pop_back();
    }
    *width = (int)columns.size();
    return columns;
}

void DrawLabel(Image* image, const std::string& label, int left, int top, const Colour& colour)
{
    int width = 0;
    std::vector<std::vector<bool> > columns = RasterLabel(label, &width);
    for (int x = 0; x < width; ++x)
    {
        for (int row = 0; row < kGlyphRows; ++row)
        {
            if (columns[x][row])
                Plot(image, left + x, top + row, colour);
        }
    }
}

int LabelWidth(const std::string& label)
{
    int width = 0;
    RasterLabel(label, &width);
    return width;
}

void DrawSymbol(Image* image, int symbol, int left, int top, const Colour& colour)
{
    for (int y = 0; y < kSymbolSize; ++y)
    {
        for (int x = 0; x < kSymbolSize; ++x)
        {
            if (Pixel(sResources.symbols, symbol * kSymbolSize + x, y)[3] != 0)
                Plot(image, left + x, top + y, colour);
        }
    }
}

Image StretchedFrame(const Image& frame, int width)
{
    if (width <= frame.width)
        return frame;
    Image image = BlankImage(width, frame.height);
    int middle = frame.width / 2;
    for (int y = 0; y < frame.height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            int sourceX = middle;
            if (x < middle)
                sourceX = x;
            else if (x >= width - (frame.width - middle - 1))
                sourceX = x - (width - frame.width);
            Copy(frame, sourceX, y, &image, x, y);
        }
    }
    return image;
}

Image TextKey(const std::string& label)
{
    int labelWidth = LabelWidth(label);
    int width = labelWidth + 2 * kLabelLeft;
    if (width < kKeyMinimumWidth)
        width = kKeyMinimumWidth;
    if ((width - labelWidth) % 2 != 0)
        ++width;
    Image key = StretchedFrame(sResources.keycap, width);
    DrawLabel(&key, label, (key.width - labelWidth) / 2, kLabelTop, kKeyLabel);
    return key;
}

Image ArrowKey(int arrow)
{
    Image key = StretchedFrame(sResources.keycap, kKeyMinimumWidth);
    int left = (key.width - kArrowSize) / 2;
    for (int y = 0; y < kArrowSize; ++y)
    {
        for (int x = 0; x < kArrowSize; ++x)
        {
            if (Pixel(sResources.arrows, arrow * kArrowSize + x, y)[3] != 0)
                Plot(&key, left + x, kLabelTop + y, kKeyLabel);
        }
    }
    return key;
}

Image MouseIcon(int icon)
{
    Image image = BlankImage(kMouseIconWidth, sResources.mouse.height);
    for (int y = 0; y < image.height; ++y)
    {
        for (int x = 0; x < image.width; ++x)
            Copy(sResources.mouse, icon * kMouseIconWidth + x, y, &image, x, y);
    }
    return image;
}

std::string KeyLabel(int input)
{
    std::string name = PcKeyboardMouse::DisplayName(input);
    static const char* const kShortNames[][2] = {
        {"Escape", "Esc"},        {"Return", "Enter"},     {"Keypad Enter", "Enter"}, {"Left Shift", "Shift"},
        {"Right Shift", "Shift"}, {"Left Ctrl", "Ctrl"},   {"Right Ctrl", "Ctrl"},    {"Left Alt", "Alt"},
        {"Right Alt", "Alt"},     {"PageUp", "PgUp"},      {"PageDown", "PgDn"},      {"CapsLock", "Caps"},
        {"Delete", "Del"},        {"Insert", "Ins"},       {"Backspace", "Back"},     {"Left GUI", "Win"},
        {"Right GUI", "Win"}};
    for (size_t index = 0; index < sizeof(kShortNames) / sizeof(kShortNames[0]); ++index)
    {
        if (name == kShortNames[index][0])
            return kShortNames[index][1];
    }
    for (size_t index = 0; index < name.size(); ++index)
    {
        unsigned char character = (unsigned char)name[index];
        if (character < 32 || character > 126)
            return PcKeyboardMouse::InputName(input);
    }
    return name;
}

Image ComposeInput(int input)
{
    switch (input)
    {
    case SDL_SCANCODE_UP:
        return ArrowKey(0);
    case SDL_SCANCODE_DOWN:
        return ArrowKey(1);
    case SDL_SCANCODE_LEFT:
        return ArrowKey(2);
    case SDL_SCANCODE_RIGHT:
        return ArrowKey(3);
    case kMouseInputBase + SDL_BUTTON_LEFT:
        return MouseIcon(0);
    case kMouseInputBase + SDL_BUTTON_RIGHT:
        return MouseIcon(1);
    case kMouseInputBase + SDL_BUTTON_MIDDLE:
        return MouseIcon(2);
    case PcKeyboardMouse::INPUT_WHEEL_UP:
        return MouseIcon(3);
    case PcKeyboardMouse::INPUT_WHEEL_DOWN:
        return MouseIcon(4);
    default:
        return TextKey(KeyLabel(input));
    }
}

Colour MakeColour(unsigned char r, unsigned char g, unsigned char b)
{
    Colour colour = {r, g, b};
    return colour;
}

PadButton Button(EShape shape, const char* label, ESymbol symbol, const Colour& face, const Colour& mark)
{
    PadButton button;
    button.shape = shape;
    button.label = label;
    button.symbol = symbol;
    button.face = face;
    button.mark = mark;
    return button;
}

const char* FamilyLabel(EPcPadFamily family, const char* xbox, const char* playStation, const char* nintendo)
{
    switch (family)
    {
    case EPcPadFamily_PlayStation3:
    case EPcPadFamily_PlayStation4:
    case EPcPadFamily_PlayStation5:
        return playStation;
    case EPcPadFamily_Nintendo:
        return nintendo;
    default:
        return xbox;
    }
}

PadButton DescribePadButton(EPcPadFamily family, EPcPadElement element)
{
    bool playStation = family == EPcPadFamily_PlayStation3 || family == EPcPadFamily_PlayStation4
                       || family == EPcPadFamily_PlayStation5;
    Colour face = playStation ? MakeColour(46, 46, 54) : MakeColour(60, 60, 60);
    bool coloured = family == EPcPadFamily_Xbox;
    switch (element)
    {
    case EPcPadElement_FaceA:
        return Button(EShape_Round, "A", ESymbol_None, face, coloured ? MakeColour(110, 200, 80) : kPadLabel);
    case EPcPadElement_FaceB:
        return Button(EShape_Round, "B", ESymbol_None, face, coloured ? MakeColour(235, 80, 70) : kPadLabel);
    case EPcPadElement_FaceX:
        return Button(EShape_Round, "X", ESymbol_None, face, coloured ? MakeColour(80, 150, 240) : kPadLabel);
    case EPcPadElement_FaceY:
        return Button(EShape_Round, "Y", ESymbol_None, face, coloured ? MakeColour(245, 200, 50) : kPadLabel);
    case EPcPadElement_FaceCross:
        return Button(EShape_Round, "", ESymbol_Cross, face, MakeColour(130, 170, 230));
    case EPcPadElement_FaceCircle:
        return Button(EShape_Round, "", ESymbol_Circle, face, MakeColour(235, 105, 100));
    case EPcPadElement_FaceSquare:
        return Button(EShape_Round, "", ESymbol_Square, face, MakeColour(220, 145, 205));
    case EPcPadElement_FaceTriangle:
        return Button(EShape_Round, "", ESymbol_Triangle, face, MakeColour(85, 200, 165));
    case EPcPadElement_LeftShoulder:
        return Button(EShape_Bumper, FamilyLabel(family, "LB", "L1", "L"), ESymbol_None, face, kPadLabel);
    case EPcPadElement_RightShoulder:
        return Button(EShape_Bumper, FamilyLabel(family, "RB", "R1", "R"), ESymbol_None, face, kPadLabel);
    case EPcPadElement_LeftTrigger:
        return Button(EShape_Trigger, FamilyLabel(family, "LT", "L2", "ZL"), ESymbol_None, face, kPadLabel);
    case EPcPadElement_RightTrigger:
        return Button(EShape_Trigger, FamilyLabel(family, "RT", "R2", "ZR"), ESymbol_None, face, kPadLabel);
    case EPcPadElement_LeftStick:
        return Button(EShape_Stick, "L", ESymbol_None, face, kPadLabel);
    case EPcPadElement_RightStick:
        return Button(EShape_Stick, "R", ESymbol_None, face, kPadLabel);
    case EPcPadElement_LeftStickPress:
        return Button(EShape_Stick, FamilyLabel(family, "LS", "L3", "LS"), ESymbol_None, face, kPadLabel);
    case EPcPadElement_RightStickPress:
        return Button(EShape_Stick, FamilyLabel(family, "RS", "R3", "RS"), ESymbol_None, face, kPadLabel);
    case EPcPadElement_Start:
        if (family == EPcPadFamily_Nintendo)
            return Button(EShape_Round, "", ESymbol_Plus, face, kPadLabel);
        if (family == EPcPadFamily_PlayStation3)
            return Button(EShape_Pill, "Start", ESymbol_None, face, kPadLabel);
        return Button(EShape_Pill, "", ESymbol_Menu, face, kPadLabel);
    case EPcPadElement_Back:
        switch (family)
        {
        case EPcPadFamily_Nintendo:
            return Button(EShape_Round, "", ESymbol_Minus, face, kPadLabel);
        case EPcPadFamily_PlayStation3:
            return Button(EShape_Pill, "Select", ESymbol_None, face, kPadLabel);
        case EPcPadFamily_PlayStation4:
            return Button(EShape_Pill, "Share", ESymbol_None, face, kPadLabel);
        case EPcPadFamily_PlayStation5:
            return Button(EShape_Pill, "Create", ESymbol_None, face, kPadLabel);
        default:
            return Button(EShape_Pill, "", ESymbol_View, face, kPadLabel);
        }
    default:
        return Button(EShape_DPad, "", ESymbol_None, face, kDPadLit);
    }
}

Image TintedShape(EShape shape, const Colour& face, int width)
{
    Image image = StretchedFrame(sResources.shapes[shape], width);
    Colour light = Shift(face, 50);
    Colour dark = Shift(face, -28);
    for (int y = 0; y < image.height; ++y)
    {
        for (int x = 0; x < image.width; ++x)
        {
            const unsigned char* marker = Pixel(image, x, y);
            if (marker[3] == 0)
                continue;
            if (marker[0] == 0)
                Plot(&image, x, y, kPadOutline);
            else if (marker[0] == 255)
                Plot(&image, x, y, light);
            else if (marker[0] == 64)
                Plot(&image, x, y, dark);
            else
                Plot(&image, x, y, face);
        }
    }
    return image;
}

void LightDPadArm(Image* image, EPcPadElement element, const Colour& colour)
{
    int middle = image->width / 2;
    for (int y = 1; y < image->height - 1; ++y)
    {
        for (int x = 1; x < image->width - 1; ++x)
        {
            bool arm = false;
            switch (element)
            {
            case EPcPadElement_DPadUp:
                arm = y < middle - 2 && x > middle - 2 && x < middle + 2;
                break;
            case EPcPadElement_DPadDown:
                arm = y > middle + 2 && x > middle - 2 && x < middle + 2;
                break;
            case EPcPadElement_DPadLeft:
                arm = x < middle - 2 && y > middle - 2 && y < middle + 2;
                break;
            default:
                arm = x > middle + 2 && y > middle - 2 && y < middle + 2;
                break;
            }
            if (arm && Pixel(*image, x, y)[3] != 0)
                Plot(image, x, y, colour);
        }
    }
}

Image ComposePadButton(EPcPadFamily family, EPcPadElement element)
{
    PadButton button = DescribePadButton(family, element);
    int labelWidth = button.label.empty() ? 0 : LabelWidth(button.label);
    bool stretches = button.shape == EShape_Bumper || button.shape == EShape_Trigger || button.shape == EShape_Pill;
    int width = 0;
    if (stretches)
    {
        int content = button.symbol != ESymbol_None ? kSymbolSize : labelWidth;
        width = content + 2 * kLabelLeft;
        if (width < sResources.shapes[button.shape].width)
            width = sResources.shapes[button.shape].width;
        if ((width - content) % 2 != 0)
            ++width;
    }
    Image image = TintedShape(button.shape, button.face, width);
    if (button.shape == EShape_DPad)
    {
        LightDPadArm(&image, element, button.mark);
    }
    else if (button.symbol != ESymbol_None)
    {
        DrawSymbol(&image, button.symbol, (image.width - kSymbolSize) / 2, (image.height - kSymbolSize) / 2,
                   button.mark);
    }
    else if (labelWidth > 0)
    {
        DrawLabel(&image, button.label, (image.width - labelWidth) / 2, (image.height - kCapRows) / 2, button.mark);
    }

    int canvasHeight = sResources.shapes[EShape_Round].height;
    if (image.height >= canvasHeight)
        return image;
    Image canvas = BlankImage(image.width, canvasHeight);
    int top = (canvasHeight - image.height) / 2;
    for (int y = 0; y < image.height; ++y)
    {
        for (int x = 0; x < image.width; ++x)
            Copy(image, x, y, &canvas, x, top + y);
    }
    return canvas;
}

bool ComposeTouch(int art, Image* image)
{
    if (sResources.touch.rgba.empty() && !LoadImage("touch.png", &sResources.touch))
        return false;
    const Image& atlas = sResources.touch;
    if (atlas.height < kTouchButton + kTouchIcon + kTouchPad)
        return false;
    if (art == PC_PROMPT_ART_TOUCH_JOYSTICK)
    {
        *image = BlankImage(kTouchPad, kTouchPad);
        const int knob = (kTouchPad - kTouchButton) / 2;
        Blend(atlas, 0, kTouchButton + kTouchIcon, kTouchPad, kTouchPad, image, 0, 0);
        Blend(atlas, 0, 0, kTouchButton, kTouchButton, image, knob, knob);
        return true;
    }
    const int icon = art - PC_PROMPT_ART_TOUCH_BASE;
    if ((icon + 1) * kTouchIcon > atlas.width)
        return false;
    *image = BlankImage(kTouchButton, kTouchButton);
    const int inset = (kTouchButton - kTouchIcon) / 2;
    Blend(atlas, 0, 0, kTouchButton, kTouchButton, image, 0, 0);
    Blend(atlas, icon * kTouchIcon, kTouchButton, kTouchIcon, kTouchIcon, image, inset, inset);
    return true;
}

Image Enlarge(const Image& source)
{
    Image image = BlankImage(source.width * kArtScale, source.height * kArtScale);
    for (int y = 0; y < image.height; ++y)
    {
        for (int x = 0; x < image.width; ++x)
            Copy(source, x / kArtScale, y / kArtScale, &image, x, y);
    }
    return image;
}

bool Compose(int art, Image* image)
{
    if (art >= 0 && art < PcKeyboardMouse::INPUT_COUNT)
    {
        *image = ComposeInput(art);
        return true;
    }
    if (art == PC_PROMPT_ART_MOUSE)
    {
        *image = MouseIcon(kMouseRestingIcon);
        return true;
    }
    if (art == PC_PROMPT_ART_TOUCH_JOYSTICK || (art >= PC_PROMPT_ART_TOUCH_BASE && art < PC_PROMPT_ART_TOUCH_END))
        return ComposeTouch(art, image);
    int pad = art - PC_PROMPT_ART_PAD_BASE;
    if (pad >= 0 && pad < EPcPadFamily_Count * EPcPadElement_Count)
    {
        *image = ComposePadButton(static_cast<EPcPadFamily>(pad / EPcPadElement_Count),
                                  static_cast<EPcPadElement>(pad % EPcPadElement_Count));
        return true;
    }
    return false;
}

Art* FindArt(int art)
{
    LoadResources();
    if (!sResources.available)
        return NULL;
    std::map<int, Art>::iterator found = sResources.art.find(art);
    if (found != sResources.art.end())
        return &found->second;
    Image image;
    if (!Compose(art, &image))
        return NULL;
    Art& entry = sResources.art[art];
    entry.image = Enlarge(image);
    entry.texture = -1;
    return &entry;
}
}

int PcPadPromptArt(EPcPadFamily family, EPcPadElement element)
{
    return PC_PROMPT_ART_PAD_BASE + family * EPcPadElement_Count + element;
}

bool PcPromptArtPixels(int art, int* width, int* height, const unsigned char** rgba)
{
    Art* entry = FindArt(art);
    if (entry == NULL)
        return false;
    *width = entry->image.width;
    *height = entry->image.height;
    *rgba = entry->image.rgba.data();
    return true;
}

bool PcPromptArtSize(int art, int* width, int* height)
{
    const unsigned char* rgba = NULL;
    return PcPromptArtPixels(art, width, height, &rgba);
}

int PcPromptArtTexture(int art)
{
    Art* entry = FindArt(art);
    if (entry == NULL)
        return -1;
    if (entry->texture == -1)
    {
        entry->texture = gRenderer->TextureCreate();
        if (entry->texture == -1)
            return -1;
        gRenderer->TextureBind(entry->texture);
        gRenderer->TextureSetTextureLevels(1);
        gRenderer->TextureData(entry->image.width, entry->image.height, entry->image.rgba.data(), 0,
                               C4JRender::eTextureFormat_RGBA8);
    }
    return entry->texture;
}
