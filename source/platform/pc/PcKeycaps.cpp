#include "platform/pc/PcKeycaps.hpp"

#include <SDL3/SDL.h>
#include <stb_image.h>

#include <stdio.h>

#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <vector>

#include "platform/pc/KeyboardMouse.hpp"
#include "platform/pc/PcPaths.hpp"
#include "platform/pc/Renderer.hpp"
#include "vendor/stb_truetype.hpp"

namespace
{
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
const int kArrowSize = 7;
const int kMouseInputBase = PcKeyboardMouse::SCANCODE_COUNT;
const unsigned char kLabelColour[4] = {56, 56, 56, 255};

struct Image
{
    int width;
    int height;
    std::vector<unsigned char> rgba;

    Image() : width(0), height(0) {}
};

struct Keycap
{
    Image image;
    int texture;
};

struct Art
{
    bool loaded;
    bool available;
    Image frame;
    Image mouse;
    Image arrows;
    std::vector<unsigned char> fontData;
    stbtt_fontinfo font;
    std::map<int, Keycap> keycaps;

    Art() : loaded(false), available(false) {}
};

Art sArt;

bool LoadImage(const std::string& name, Image* image)
{
    std::string path = PcResourceRoot() + "/ui/" + name;
    int channels = 0;
    unsigned char* pixels = stbi_load(path.c_str(), &image->width, &image->height, &channels, 4);
    if (pixels == NULL)
    {
        fprintf(stderr, "PC interface art %s could not be loaded\n", path.c_str());
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
        fprintf(stderr, "keycap font %s could not be opened\n", path.c_str());
        return false;
    }
    sArt.fontData.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    if (sArt.fontData.empty()
        || !stbtt_InitFont(&sArt.font, sArt.fontData.data(), stbtt_GetFontOffsetForIndex(sArt.fontData.data(), 0)))
    {
        fprintf(stderr, "keycap font %s could not be read\n", path.c_str());
        return false;
    }
    return true;
}

void LoadArt()
{
    if (sArt.loaded)
        return;
    sArt.loaded = true;
    sArt.available = LoadImage("keycap.png", &sArt.frame) && LoadImage("mouse.png", &sArt.mouse)
                     && LoadImage("arrows.png", &sArt.arrows) && LoadFont() && sArt.frame.height == kKeyHeight
                     && sArt.frame.width == 2 * kKeyEdge + 1;
}

void Copy(const Image& source, int sourceX, int sourceY, Image* target, int targetX, int targetY)
{
    const unsigned char* from = &source.rgba[(sourceY * source.width + sourceX) * 4];
    unsigned char* to = &target->rgba[(targetY * target->width + targetX) * 4];
    to[0] = from[0];
    to[1] = from[1];
    to[2] = from[2];
    to[3] = from[3];
}

void Plot(Image* target, int x, int y)
{
    if (x < 0 || y < 0 || x >= target->width || y >= target->height)
        return;
    unsigned char* to = &target->rgba[(y * target->width + x) * 4];
    to[0] = kLabelColour[0];
    to[1] = kLabelColour[1];
    to[2] = kLabelColour[2];
    to[3] = kLabelColour[3];
}

std::vector<std::vector<bool> > RasterLabel(const std::string& text, int* width)
{
    std::vector<std::vector<bool> > columns;
    float scale = stbtt_ScaleForMappingEmToPixels(&sArt.font, (float)(kFontCell * kFontPixelsPerEm));
    for (size_t index = 0; index < text.size(); ++index)
    {
        int codepoint = (unsigned char)text[index];
        int advance = 0;
        int bearing = 0;
        stbtt_GetCodepointHMetrics(&sArt.font, codepoint, &advance, &bearing);
        int cells = (int)(advance * scale / kFontCell + 0.5f);
        int glyphWidth = 0;
        int glyphHeight = 0;
        int offsetX = 0;
        int offsetY = 0;
        unsigned char* bitmap =
            stbtt_GetCodepointBitmap(&sArt.font, scale, scale, codepoint, &glyphWidth, &glyphHeight, &offsetX, &offsetY);
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

Image BlankKey(int width)
{
    Image key;
    key.width = width < kKeyMinimumWidth ? kKeyMinimumWidth : width;
    key.height = kKeyHeight;
    key.rgba.assign(key.width * key.height * 4, 0);
    for (int y = 0; y < key.height; ++y)
    {
        for (int x = 0; x < key.width; ++x)
        {
            int sourceX = kKeyEdge;
            if (x < kKeyEdge)
                sourceX = x;
            else if (x >= key.width - kKeyEdge)
                sourceX = x - (key.width - sArt.frame.width);
            Copy(sArt.frame, sourceX, y, &key, x, y);
        }
    }
    return key;
}

Image TextKey(const std::string& label)
{
    int labelWidth = 0;
    std::vector<std::vector<bool> > columns = RasterLabel(label, &labelWidth);
    Image key = BlankKey(labelWidth + 2 * kLabelLeft);
    int left = (key.width - labelWidth) / 2;
    for (int x = 0; x < labelWidth; ++x)
    {
        for (int row = 0; row < kGlyphRows; ++row)
        {
            if (columns[x][row])
                Plot(&key, left + x, kLabelTop + row);
        }
    }
    return key;
}

Image ArrowKey(int arrow)
{
    Image key = BlankKey(kKeyMinimumWidth);
    int left = (key.width - kArrowSize) / 2;
    for (int y = 0; y < kArrowSize; ++y)
    {
        for (int x = 0; x < kArrowSize; ++x)
        {
            if (sArt.arrows.rgba[(y * sArt.arrows.width + arrow * kArrowSize + x) * 4 + 3] != 0)
                Plot(&key, left + x, kLabelTop + y);
        }
    }
    return key;
}

Image MouseIcon(int icon)
{
    Image image;
    image.width = kMouseIconWidth;
    image.height = sArt.mouse.height;
    image.rgba.assign(image.width * image.height * 4, 0);
    for (int y = 0; y < image.height; ++y)
    {
        for (int x = 0; x < image.width; ++x)
            Copy(sArt.mouse, icon * kMouseIconWidth + x, y, &image, x, y);
    }
    return image;
}

Image Enlarge(const Image& source)
{
    Image image;
    image.width = source.width * kArtScale;
    image.height = source.height * kArtScale;
    image.rgba.resize(image.width * image.height * 4);
    for (int y = 0; y < image.height; ++y)
    {
        for (int x = 0; x < image.width; ++x)
            Copy(source, x / kArtScale, y / kArtScale, &image, x, y);
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
        {"Right GUI", "Win"},     {"Space", "Space"}};
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

Image ComposeKeycap(int input)
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

Keycap* FindKeycap(int input)
{
    LoadArt();
    if (!sArt.available || input < 0 || input >= PcKeyboardMouse::INPUT_COUNT)
        return NULL;
    std::map<int, Keycap>::iterator found = sArt.keycaps.find(input);
    if (found != sArt.keycaps.end())
        return &found->second;
    Keycap& keycap = sArt.keycaps[input];
    keycap.image = Enlarge(ComposeKeycap(input));
    keycap.texture = -1;
    return &keycap;
}
}

bool PcKeycapSize(int input, int* width, int* height)
{
    Keycap* keycap = FindKeycap(input);
    if (keycap == NULL)
        return false;
    *width = keycap->image.width;
    *height = keycap->image.height;
    return true;
}

int PcKeycapTexture(int input)
{
    Keycap* keycap = FindKeycap(input);
    if (keycap == NULL)
        return -1;
    if (keycap->texture == -1)
    {
        keycap->texture = gRenderer->TextureCreate();
        if (keycap->texture == -1)
            return -1;
        gRenderer->TextureBind(keycap->texture);
        gRenderer->TextureSetTextureLevels(1);
        gRenderer->TextureData(keycap->image.width, keycap->image.height, keycap->image.rgba.data(), 0,
                               C4JRender::eTextureFormat_RGBA8);
    }
    return keycap->texture;
}
