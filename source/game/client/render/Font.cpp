#include "game/client/render/Font.hpp"

#include <string.h>
#include <vector>

#include "game/client/render/Tesselator.hpp"
#include "game/client/render/Textures.hpp"
#include "game/core/BufferBuilder.hpp"
#include "game/core/BufferedImage.hpp"
#include "game/core/GlStateManager.hpp"
#include "game/core/Options.hpp"
#include "game/core/SharedConstants.hpp"
#include "game/core/StringUtils.hpp"
#include "game/util/Random.hpp"

namespace
{
const int kFormatObfuscated = 16;
const int kFormatBold = 17;
const int kFormatStrikethrough = 18;
const int kFormatUnderline = 19;
const int kFormatItalic = 20;
const int kFormatReset = 21;

int FormatCode(wchar_t code)
{
    if (code >= L'0' && code <= L'9')
        return code - L'0';
    if (code >= L'a' && code <= L'f')
        return code - L'a' + 10;
    if (code >= L'A' && code <= L'F')
        return code - L'A' + 10;
    if (code >= L'k' && code <= L'o')
        return code - L'k' + kFormatObfuscated;
    if (code >= L'K' && code <= L'O')
        return code - L'K' + kFormatObfuscated;
    if (code == L'r' || code == L'R')
        return kFormatReset;
    return -1;
}

struct TextLine
{
    float x0;
    float x1;
    float y0;
    float y1;
    int color;
};
}
#include "game/util/ResourceLocation.hpp"
#include "game/world/biome/arrayWithLength.hpp"

Font::Font(Options* options, const std::wstring& name, Textures* textures, bool, ResourceLocation* textureLocation,
           int columns, int rows, int characterWidth, int characterHeight, unsigned short* characterMap)
    : mTextures(textures),
      mCharacterMap()
{
    const int characterCount = columns * rows;
    mCharWidths = new float[characterCount];
    memset(mCharWidths, 0, characterCount * sizeof(float));
    mColumns = columns;
    mRows = rows;
    mCharacterWidth = characterWidth;
    mCharacterHeight = characterHeight;
    mEnforceUnicodeSheet = false;
    mTextureLocation = textureLocation;
    mBaseColor = -1;
    mX = 0.0f;
    mY = 0.0f;
    mBidirectional = false;

    if (characterMap != NULL)
    {
        for (int i = 0; i < characterCount; ++i)
            mCharacterMap.insert(std::pair<const int, int>(characterMap[i], i));
    }

    mRandom = new Random();

    BufferedImage* image = textures->readImage(static_cast<_TEXTURE_NAME>(textureLocation->getTexture()), name);
    const int imageWidth = image->getWidth();
    const int imageHeight = image->getHeight();
    arrayWithLength<int> pixels(imageWidth * imageHeight, true);
    image->getRGB(0, 0, imageWidth, imageHeight, pixels, 0, imageWidth, 0);

    for (int i = 0; i < characterCount; ++i)
    {
        const int row = i / mColumns;
        const int column = i % mColumns;
        int x;
        for (x = mCharacterWidth - 1; x >= 0; --x)
        {
            bool emptyColumn = true;
            for (int y = 0; y < mCharacterHeight && emptyColumn; ++y)
            {
                const int pixel =
                    pixels.data[(column * mCharacterWidth + x) + (row * mCharacterHeight + y) * imageWidth];
                if ((pixel >> 24) != 0)
                    emptyColumn = false;
            }
            if (!emptyColumn)
                break;
        }

        if (i == L' ')
            x = 2;
        mCharWidths[i] = static_cast<float>(x + 2);
    }

    delete image;

    const bool anaglyph = options->mAnaglyph3d;
    for (int i = 0; i < 32; ++i)
    {
        const int brightness = (i >> 3 & 1) * 85;
        int red = (i >> 2 & 1) * 170 + brightness;
        int green = (i >> 1 & 1) * 170 + brightness;
        int blue = (i & 1) * 170 + brightness;
        if (i == 6)
            red += 85;

        if (anaglyph)
        {
            const int anaglyphRed = (red * 30 + green * 59 + blue * 11) / 100;
            const int anaglyphGreen = (red * 30 + green * 70) / 100;
            const int anaglyphBlue = (red * 30 + blue * 70) / 100;
            red = anaglyphRed;
            green = anaglyphGreen;
            blue = anaglyphBlue;
        }

        if (i >= 16)
        {
            red /= 4;
            green /= 4;
            blue /= 4;
        }

        mColors[i] = (red & 255) << 16 | (green & 255) << 8 | (blue & 255);
    }

    mCharacters = new Character[characterCount];
    for (int i = 0; i < characterCount; ++i)
        cacheCharacter(i);
}

bool Font::CharacterExists(wchar_t character)
{
    if (mCharacterMap.size() != 0)
        return mCharacterMap.find(character) != mCharacterMap.end();
    return character <= mRows * mColumns;
}

int Font::MapCharacter(wchar_t character)
{
    if (mCharacterMap.size() != 0 && character != L' ')
        return mCharacterMap[character];
    return character;
}

std::wstring Font::sanitize(const std::wstring& text, bool trim)
{
    std::wstring result = text;
    for (unsigned int i = 0; i < result.length(); ++i)
    {
        if (CharacterExists(result[i]))
        {
            result[i] = static_cast<wchar_t>(MapCharacter(result[i]));
        }
        else if (!trim)
        {
            result[i] = 0;
        }
        else
        {
            result.erase(result.begin() + i);
            --i;
        }
    }

    if (trim)
    {
        const std::wstring::size_type first = result.find_first_not_of(L' ');
        const std::wstring::size_type last = result.find_last_not_of(L' ');
        result = first == std::wstring::npos ? std::wstring(L"") : std::wstring(result, first, last - first + 1);
    }

    return result;
}

int Font::width(const std::wstring& text)
{
    float result = 0.0f;
    bool bold = false;
    for (size_t i = 0; i < text.length(); ++i)
    {
        const wchar_t character = text[i];
        if (character == 0xA7 && i + 1 < text.length())
        {
            const int code = FormatCode(text[++i]);
            if (code == kFormatBold)
                bold = true;
            else if ((code >= 0 && code < 16) || code == kFormatReset)
                bold = false;
            continue;
        }
        const int mapped = CharacterExists(character) ? MapCharacter(character) : 0;
        result += mCharWidths[mapped];
        if (bold)
            result += 1.0f;
    }
    return static_cast<int>(result + 0.5f);
}

void Font::renderCharacter(wchar_t character, BufferBuilder* builder)
{
    renderCharacter(character, builder, 0.0f);
}

void Font::renderCharacter(wchar_t character, BufferBuilder* builder, float skew)
{
    const Character& glyph = mCharacters[character];
    builder->tex(glyph.u0, glyph.v1);
    builder->vertexNoBounds(mX - skew, mY + glyph.height, 0.0f);
    builder->tex(glyph.u1, glyph.v1);
    builder->vertexNoBounds(mX + glyph.width - skew, mY + glyph.height, 0.0f);
    builder->tex(glyph.u1, glyph.v0);
    builder->vertexNoBounds(mX + glyph.width + skew, mY, 0.0f);
    builder->tex(glyph.u0, glyph.v0);
    builder->vertexNoBounds(mX + skew, mY, 0.0f);
    mX += glyph.advance;
}

int Font::ObfuscatedCharacter(int character)
{
    const std::wstring& allowed = SharedConstants::ALLOWED_CHARACTERS;
    if (allowed.empty())
        return character;
    for (int attempt = 0; attempt < 64; ++attempt)
    {
        const wchar_t candidate = allowed[mRandom->nextInt(static_cast<int>(allowed.length()))];
        if (!CharacterExists(candidate))
            continue;
        const int mapped = MapCharacter(candidate);
        if (mCharWidths[mapped] == mCharWidths[character])
            return mapped;
    }
    return character;
}

unsigned int Font::draw(const std::wstring& text, bool darken, bool useBuilder)
{
    mTextures->bindTexture(mTextureLocation);
    BufferBuilder* builder = Tesselator::getInstance()->getBuilder();
    if (useBuilder)
        builder->begin();

    const bool formatted = text.find(static_cast<wchar_t>(0xA7)) != std::wstring::npos;
    int alpha = (mBaseColor >> 24) & 255;
    if (alpha == 0)
        alpha = 255;
    int color = mBaseColor & 0xFFFFFF;
    if (formatted)
    {
        GlStateManager::color4f(1.0f, 1.0f, 1.0f, 1.0f);
        builder->color(color, alpha);
    }

    bool obfuscated = false;
    bool bold = false;
    bool strikethrough = false;
    bool underline = false;
    bool italic = false;
    std::vector<TextLine> lines;
    for (size_t i = 0; i < text.length(); ++i)
    {
        const wchar_t character = text[i];
        if (character == 0xA7 && i + 1 < text.length())
        {
            const int code = FormatCode(text[++i]);
            if ((code >= 0 && code < 16) || code == kFormatReset)
            {
                obfuscated = false;
                bold = false;
                strikethrough = false;
                underline = false;
                italic = false;
                color = code == kFormatReset ? (mBaseColor & 0xFFFFFF) : mColors[darken ? code + 16 : code];
                builder->color(color, alpha);
            }
            else if (code == kFormatObfuscated)
                obfuscated = true;
            else if (code == kFormatBold)
                bold = true;
            else if (code == kFormatStrikethrough)
                strikethrough = true;
            else if (code == kFormatUnderline)
                underline = true;
            else if (code == kFormatItalic)
                italic = true;
            continue;
        }

        int mapped = CharacterExists(character) ? MapCharacter(character) : 0;
        if (obfuscated && character != L' ')
            mapped = ObfuscatedCharacter(mapped);
        const float skew = italic ? 1.0f : 0.0f;
        const float startX = mX;
        renderCharacter(static_cast<wchar_t>(mapped), builder, skew);
        if (bold)
        {
            const float endX = mX;
            mX = startX + 1.0f;
            renderCharacter(static_cast<wchar_t>(mapped), builder, skew);
            mX = endX + 1.0f;
        }
        if (strikethrough)
        {
            const TextLine line = {startX, mX, mY + 3.5f, mY + 4.5f, color};
            lines.push_back(line);
        }
        if (underline)
        {
            const TextLine line = {startX - 1.0f, mX, mY + 8.0f, mY + 9.0f, color};
            lines.push_back(line);
        }
    }

    if (useBuilder)
    {
        builder->end();
        if (!lines.empty())
        {
            GlStateManager::disableTexture();
            for (size_t index = 0; index < lines.size(); ++index)
            {
                const TextLine& line = lines[index];
                GlStateManager::color4f(static_cast<float>((line.color >> 16) & 255) / 255.0f,
                                        static_cast<float>((line.color >> 8) & 255) / 255.0f,
                                        static_cast<float>(line.color & 255) / 255.0f,
                                        static_cast<float>(alpha) / 255.0f);
                builder->begin();
                builder->vertex(line.x0, line.y1, 0.0f);
                builder->vertex(line.x1, line.y1, 0.0f);
                builder->vertex(line.x1, line.y0, 0.0f);
                builder->vertex(line.x0, line.y0, 0.0f);
                builder->end();
            }
            GlStateManager::enableTexture();
        }
    }
    return !obfuscated;
}

unsigned int Font::draw(const std::wstring& text, int x, int y, int color, bool darken, bool useBuilder)
{
    if (text.length() != 0)
    {
        if (darken)
            color = (color & 0xFCFCFC) >> 2 | (color & 0xFF000000);
        GlStateManager::color4f((color >> 16 & 255) / 255.0f, (color >> 8 & 255) / 255.0f, (color & 255) / 255.0f,
                                (color >> 24 & 255) / 255.0f);
        mBaseColor = color;
        mX = static_cast<float>(x);
        mY = static_cast<float>(y);
        return draw(text, darken, useBuilder);
    }
    return 1;
}

unsigned int Font::draw(const std::wstring& text, int x, int y, int color)
{
    return draw(text, x, y, color, false, true);
}

void Font::cacheCharacter(int character)
{
    const float textureWidth = mCharWidths[character] - 0.01f;
    const int column = character % mColumns;
    const int row = character / mColumns;
    const float x = static_cast<float>(column * mCharacterWidth);
    const float y = static_cast<float>(row * mCharacterHeight);
    const float textureHeight = static_cast<float>(mCharacterHeight) - 0.01f;
    const float sheetWidth = static_cast<float>(mColumns * mCharacterWidth);
    const float sheetHeight = static_cast<float>(mRows * mCharacterHeight);
    Character& glyph = mCharacters[character];

    float drawWidth = textureWidth;
    if (mCharacterWidth > 8)
    {
        mCharWidths[character] *= 0.5f;
        drawWidth = mCharWidths[character] - 0.01f;
    }

    glyph.height = textureHeight <= 7.99f ? textureHeight : 7.99f;
    glyph.u0 = x / sheetWidth;
    glyph.u1 = (x + textureWidth) / sheetWidth;
    glyph.v0 = y / sheetHeight;
    glyph.width = drawWidth;
    glyph.v1 = (y + textureHeight) / sheetHeight;
    glyph.advance = mCharWidths[character];
}

void Font::drawShadow(const std::wstring& text, int x, int y, int color)
{
    draw(text, x + 1, y + 1, color, true, true);
    draw(text, x, y, color, false, true);
}

int Font::wordWrapHeight(const std::wstring& text, int wrapWidth)
{
    std::vector<std::wstring> lines = stringSplit(text, L'\n');
    if (lines.size() > 1)
    {
        int height = 0;
        std::vector<std::wstring>::iterator end = lines.end();
        for (std::vector<std::wstring>::iterator it = lines.begin(); it != end; ++it)
            height += wordWrapHeight(*it, wrapWidth);
        return height;
    }

    std::vector<std::wstring> words = stringSplit(text, L' ');
    unsigned int position = 0;
    int height = 0;
    while (position < words.size())
    {
        std::wstring line = words[position++] + L" ";
        while (position < words.size() && width(line + words[position]) < wrapWidth)
            line += words[position++] + L" ";

        while (width(line) > wrapWidth)
        {
            int length = 0;
            while (width(line.substr(0, length + 1)) <= wrapWidth)
                ++length;
            if (trimString(line.substr(0, length)).length() > 0)
                height += 8;
            line = line.substr(length);
        }

        if (trimString(line).length() > 0)
            height += 8;
    }

    if (height < 8)
        height += 8;
    return height;
}

void Font::drawWordWrapInternal(const std::wstring& text, int x, int y, int wrapWidth, int color, int height)
{
    drawWordWrapInternal(text, x, y, wrapWidth, color, false, height);
}

void Font::drawWordWrapInternal(const std::wstring& text, int x, int y, int wrapWidth, int color, bool, int height)
{
    std::vector<std::wstring> lines = stringSplit(text, L'\n');
    if (lines.size() > 1)
    {
        std::vector<std::wstring>::iterator end = lines.end();
        for (std::vector<std::wstring>::iterator it = lines.begin(); it != end; ++it)
        {
            if (y + wordWrapHeight(*it, wrapWidth) > height)
                break;
            drawWordWrapInternal(*it, x, y, wrapWidth, color, height);
            y += wordWrapHeight(*it, wrapWidth);
        }
        return;
    }

    std::vector<std::wstring> words = stringSplit(text, L' ');
    unsigned int position = 0;
    while (position < words.size())
    {
        std::wstring line = words[position++] + L" ";
        while (position < words.size() && width(line + words[position]) < wrapWidth)
            line += words[position++] + L" ";

        while (width(line) > wrapWidth)
        {
            int length = 0;
            while (width(line.substr(0, length + 1)) <= wrapWidth)
                ++length;
            if (trimString(line.substr(0, length)).length() > 0)
            {
                draw(line.substr(0, length), x, y, color);
                y += 8;
            }
            line = line.substr(length);
            if (y + 8 > height)
                break;
        }

        if (trimString(line).length() > 0 && y + 8 <= height)
        {
            draw(line, x, y, color);
            y += 8;
        }
    }
}

unsigned int Font::drawNoBeginEndBuilder(const std::wstring& text, int x, int y, int color)
{
    return draw(text, x, y, color, false, false);
}

void Font::bindTexture()
{
    mTextures->bindTexture(mTextureLocation);
}

void Font::drawWordWrap(const std::wstring& text, int x, int y, int wrapWidth, int color, int height)
{
    drawWordWrapInternal(text, x, y, wrapWidth, color, height);
}
