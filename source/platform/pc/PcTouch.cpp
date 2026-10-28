#include "platform/pc/PcTouch.hpp"

#include "game/core/GameSettings.hpp"
#include "platform/C4JInput.hpp"
#include "platform/pc/GlApi.hpp"
#include "platform/pc/Log.hpp"
#include "platform/pc/PcPaths.hpp"
#include "platform/pc/PcPromptArt.hpp"

#include <SDL3/SDL.h>
#include <stb_image.h>

#include <stddef.h>

#include <cmath>
#include <cstring>
#include <string>

namespace
{
const Logger sLog("Touch");

const unsigned long long kHoldJoystick = 300000000ull;
const unsigned long long kHoldDPad = 400000000ull;
const unsigned long long kDragDwell = 180000000ull;
const unsigned long long kSplitHold = 525000000ull;
const unsigned long long kSplitFill = 1000000000ull;
const unsigned long long kDoubleTap = 300000000ull;
const float kSwipeStep = 0.1f;
const unsigned long long kSneakHold = 400000000ull;
const unsigned long long kTapPulse = 120000000ull;
const unsigned long long kMinimumPress = 70000000ull;
const unsigned long long kDropDelay = 500000000ull;
const unsigned long long kDropRepeat = 150000000ull;
const unsigned long long kHudTimeout = 500000000ull;
const unsigned long long kFlyStopGap = 150000000ull;
const float kSlop = 0.03f;
const float kDeadZone = 0.15f;
const float kSprintReach = 1.1f;

const int kButtonTexels = 20;
const int kIconTexels = 12;
const int kPadTexels = 48;
const int kAtlasIconTop = 20;
const int kAtlasPadTop = 32;
const int kAtlasWhiteLeft = 48;
const int kAtlasDigitsLeft = 52;
const int kDigitWidth = 3;
const int kDigitHeight = 5;
const int kPanelCorner = 4;
const int kInteractTexels = 16;
const int kInteractPadding = 6;
const int kGlyphCells = 16;
const int kGlyphTexels = 8;
const float kNormalOpacity = 80.0f;

enum EFrame
{
    EFrame_Rest,
    EFrame_Pressed,
    EFrame_On
};

enum EIcon
{
    EIcon_None = -1,
    EIcon_Jump,
    EIcon_Sneak,
    EIcon_FlyUp,
    EIcon_FlyDown,
    EIcon_SwimUp,
    EIcon_SwimDown,
    EIcon_Dismount,
    EIcon_Attack,
    EIcon_Use,
    EIcon_Pause,
    EIcon_Chat,
    EIcon_Inventory,
    EIcon_Crafting,
    EIcon_ArrowUp,
    EIcon_ArrowUpLeft,
    EIcon_ArrowDown,
    EIcon_ArrowLeft,
    EIcon_ArrowRight,
    EIcon_ArrowUpRight,
    EIcon_Sprint,
    EIcon_PickBlock,
    EIcon_Perspective,
    EIcon_Interact,
    EIcon_Look
};

const unsigned short kHighGlyphs[] = {0x00C7, 0x00FC, 0x00E9, 0x00E2, 0x00E4, 0x00E0, 0x00E5, 0x00E7, 0x00EA, 0x00EB,
                                      0x00E8, 0x00EF, 0x00EE, 0x00EC, 0x00C4, 0x00C5, 0x00C9, 0x00E6, 0x00C6, 0x00F4,
                                      0x00F6, 0x00F2, 0x00FB, 0x00F9, 0x00FF, 0x00D6, 0x00DC, 0x00F8, 0x00A3, 0x00D8,
                                      0x00D7, 0x0192, 0x00E1, 0x00ED, 0x00F3, 0x00FA, 0x00F1, 0x00D1, 0x00AA, 0x00BA,
                                      0x00BF, 0x00AE, 0x00AC, 0x00BD, 0x00BC, 0x00A1, 0x00AB, 0x00BB};

const char* const kVertexSource = R"(#version 330 core
layout(location = 0) in vec2 aPosition;
layout(location = 1) in vec2 aTexCoord;
layout(location = 2) in vec4 aColour;
uniform vec2 uScreen;
out vec2 vTexCoord;
out vec4 vColour;
void main()
{
    vec2 clip = aPosition / uScreen * 2.0 - 1.0;
    gl_Position = vec4(clip.x, -clip.y, 0.0, 1.0);
    vTexCoord = aTexCoord;
    vColour = aColour;
}
)";

const char* const kFragmentSource = R"(#version 330 core
in vec2 vTexCoord;
in vec4 vColour;
uniform sampler2D uArt;
out vec4 oColour;
void main()
{
    oColour = texture(uArt, vTexCoord) * vColour;
}
)";

struct Vertex
{
    float x;
    float y;
    float u;
    float v;
    unsigned char colour[4];
};

struct OverlayState
{
    bool initialised;
    bool available;
    GLuint program;
    GLint screenLocation;
    GLint artLocation;
    GLuint vertexArray;
    GLuint vertexBuffer;
    GLuint texture;
    int textureWidth;
    int textureHeight;
};

OverlayState sOverlay;

struct FontState
{
    bool loaded;
    std::vector<unsigned char> pixels;
    int width;
    int height;
    int advances[kGlyphCells * kGlyphCells];
    GLuint texture;
};

FontState sFont;

unsigned long long Now()
{
    return SDL_GetTicksNS();
}

int ButtonAction(int button)
{
    static const int kActions[] = {EControllerActions_Jump,
                                   EControllerActions_Sneak,
                                   EControllerActions_Action,
                                   EControllerActions_Use,
                                   EControllerActions_Pause,
                                   EControllerActions_Chat,
                                   EControllerActions_Inventory,
                                   EControllerActions_Crafting,
                                   -1,
                                   -1,
                                   EControllerActions_ChangeCameraMode,
                                   -1};
    return kActions[button];
}

void LoadFont()
{
    if (sFont.loaded)
        return;
    sFont.loaded = true;
    std::string path = PcContentRoot() + "/Common/res/font/default.png";
    int width = 0;
    int height = 0;
    int channels = 0;
    unsigned char* pixels = stbi_load(path.c_str(), &width, &height, &channels, 4);
    if (pixels == NULL || width % kGlyphCells != 0 || height % kGlyphCells != 0)
    {
        LOG_ERROR(sLog, "Touch controls font %s could not be loaded", path.c_str());
        if (pixels != NULL)
            stbi_image_free(pixels);
        return;
    }
    sFont.width = width;
    sFont.height = height;
    sFont.pixels.assign(pixels, pixels + width * height * 4);
    stbi_image_free(pixels);
    const int cellWidth = width / kGlyphCells;
    const int cellHeight = height / kGlyphCells;
    for (int glyph = 0; glyph < kGlyphCells * kGlyphCells; ++glyph)
    {
        const int left = (glyph % kGlyphCells) * cellWidth;
        const int top = (glyph / kGlyphCells) * cellHeight;
        int right = -1;
        for (int y = 0; y < cellHeight; ++y)
        {
            for (int x = 0; x < cellWidth; ++x)
            {
                if (sFont.pixels[((top + y) * width + left + x) * 4 + 3] != 0 && x > right)
                    right = x;
            }
        }
        sFont.advances[glyph] = right < 0 ? 0 : right + 2;
    }
    sFont.advances[' '] = cellWidth / 2;
}

int GlyphFor(wchar_t character)
{
    if (character >= 32 && character < 127)
        return (int)character;
    for (size_t index = 0; index < sizeof(kHighGlyphs) / sizeof(kHighGlyphs[0]); ++index)
    {
        if ((wchar_t)kHighGlyphs[index] == character)
            return 0x80 + (int)index;
    }
    return -1;
}

float TextWidth(const std::wstring& text)
{
    LoadFont();
    if (sFont.pixels.empty() || text.empty())
        return -1.0f;
    const float texel = (float)kGlyphTexels / (float)(sFont.width / kGlyphCells);
    float width = 0.0f;
    for (size_t index = 0; index < text.size(); ++index)
    {
        int glyph = GlyphFor(text[index]);
        if (glyph < 0)
            return -1.0f;
        width += sFont.advances[glyph] * texel;
    }
    return width - texel;
}

GLuint CompileShader(GLenum type, const char* source)
{
    GLuint shader = Gl::CreateShader(type);
    Gl::ShaderSource(shader, 1, &source, NULL);
    Gl::CompileShader(shader);
    GLint status = 0;
    Gl::GetShaderiv(shader, Gl::COMPILE_STATUS, &status);
    if (status == 0)
    {
        char log[1024];
        GLsizei length = 0;
        Gl::GetShaderInfoLog(shader, sizeof(log), &length, log);
        LOG_ERROR(sLog, "Touch controls shader failed: %.*s", (int)length, log);
        Gl::DeleteShader(shader);
        return 0;
    }
    return shader;
}

bool CreateProgram()
{
    GLuint vertex = CompileShader(Gl::VERTEX_SHADER, kVertexSource);
    GLuint fragment = CompileShader(Gl::FRAGMENT_SHADER, kFragmentSource);
    if (vertex == 0 || fragment == 0)
    {
        if (vertex != 0)
            Gl::DeleteShader(vertex);
        if (fragment != 0)
            Gl::DeleteShader(fragment);
        return false;
    }

    sOverlay.program = Gl::CreateProgram();
    Gl::AttachShader(sOverlay.program, vertex);
    Gl::AttachShader(sOverlay.program, fragment);
    Gl::LinkProgram(sOverlay.program);
    Gl::DeleteShader(vertex);
    Gl::DeleteShader(fragment);

    GLint status = 0;
    Gl::GetProgramiv(sOverlay.program, Gl::LINK_STATUS, &status);
    if (status == 0)
    {
        LOG_ERROR(sLog, "Touch controls program failed to link");
        Gl::DeleteProgram(sOverlay.program);
        sOverlay.program = 0;
        return false;
    }
    sOverlay.screenLocation = Gl::GetUniformLocation(sOverlay.program, "uScreen");
    sOverlay.artLocation = Gl::GetUniformLocation(sOverlay.program, "uArt");
    return true;
}

bool LoadArt()
{
    std::string path = PcResourceRoot() + "/ui/touch.png";
    int width = 0;
    int height = 0;
    int channels = 0;
    unsigned char* pixels = stbi_load(path.c_str(), &width, &height, &channels, 4);
    if (pixels == NULL)
    {
        LOG_ERROR(sLog, "Touch controls art %s could not be loaded", path.c_str());
        return false;
    }
    Gl::GenTextures(1, &sOverlay.texture);
    Gl::BindTexture(Gl::TEXTURE_2D, sOverlay.texture);
    Gl::PixelStorei(Gl::UNPACK_ALIGNMENT, 4);
    Gl::TexImage2D(Gl::TEXTURE_2D, 0, (GLint)Gl::RGBA, width, height, 0, Gl::RGBA, Gl::UNSIGNED_BYTE, pixels);
    Gl::TexParameteri(Gl::TEXTURE_2D, Gl::TEXTURE_MIN_FILTER, (GLint)Gl::NEAREST);
    Gl::TexParameteri(Gl::TEXTURE_2D, Gl::TEXTURE_MAG_FILTER, (GLint)Gl::NEAREST);
    Gl::TexParameteri(Gl::TEXTURE_2D, Gl::TEXTURE_WRAP_S, (GLint)Gl::CLAMP_TO_EDGE);
    Gl::TexParameteri(Gl::TEXTURE_2D, Gl::TEXTURE_WRAP_T, (GLint)Gl::CLAMP_TO_EDGE);
    stbi_image_free(pixels);
    sOverlay.textureWidth = width;
    sOverlay.textureHeight = height;
    return true;
}

void Initialise()
{
    sOverlay.initialised = true;
    if (!CreateProgram() || !LoadArt())
        return;

    Gl::GenVertexArrays(1, &sOverlay.vertexArray);
    Gl::GenBuffers(1, &sOverlay.vertexBuffer);
    Gl::BindVertexArray(sOverlay.vertexArray);
    Gl::BindBuffer(Gl::ARRAY_BUFFER, sOverlay.vertexBuffer);
    Gl::EnableVertexAttribArray(0);
    Gl::VertexAttribPointer(0, 2, Gl::FLOAT, Gl::FALSE_VALUE, sizeof(Vertex), (const void*)offsetof(Vertex, x));
    Gl::EnableVertexAttribArray(1);
    Gl::VertexAttribPointer(1, 2, Gl::FLOAT, Gl::FALSE_VALUE, sizeof(Vertex), (const void*)offsetof(Vertex, u));
    Gl::EnableVertexAttribArray(2);
    Gl::VertexAttribPointer(2, 4, Gl::UNSIGNED_BYTE, Gl::TRUE_VALUE, sizeof(Vertex),
                            (const void*)offsetof(Vertex, colour));
    Gl::BindVertexArray(0);
    sOverlay.available = true;
}

void PushTinted(std::vector<Vertex>& vertices, float x, float y, float width, float height, int texelX, int texelY,
                int texelWidth, int texelHeight, unsigned char red, unsigned char green, unsigned char blue,
                unsigned char alpha)
{
    float u0 = (float)texelX / (float)sOverlay.textureWidth;
    float v0 = (float)texelY / (float)sOverlay.textureHeight;
    float u1 = (float)(texelX + texelWidth) / (float)sOverlay.textureWidth;
    float v1 = (float)(texelY + texelHeight) / (float)sOverlay.textureHeight;
    Vertex corners[4] = {{x, y, u0, v0, {red, green, blue, alpha}},
                         {x + width, y, u1, v0, {red, green, blue, alpha}},
                         {x + width, y + height, u1, v1, {red, green, blue, alpha}},
                         {x, y + height, u0, v1, {red, green, blue, alpha}}};
    vertices.push_back(corners[0]);
    vertices.push_back(corners[1]);
    vertices.push_back(corners[2]);
    vertices.push_back(corners[0]);
    vertices.push_back(corners[2]);
    vertices.push_back(corners[3]);
}

void PushSprite(std::vector<Vertex>& vertices, float x, float y, float width, float height, int texelX, int texelY,
                int texelWidth, int texelHeight, unsigned char alpha)
{
    PushTinted(vertices, x, y, width, height, texelX, texelY, texelWidth, texelHeight, 255, 255, 255, alpha);
}

void PushBlock(std::vector<Vertex>& vertices, float x, float y, float width, float height, unsigned char red,
               unsigned char green, unsigned char blue, unsigned char alpha)
{
    PushTinted(vertices, x, y, width, height, kAtlasWhiteLeft + 1, kAtlasPadTop + 1, 2, 2, red, green, blue, alpha);
}

void PushNumber(std::vector<Vertex>& vertices, float centreX, float bottom, float scale, int number)
{
    char digits[16];
    int count = 0;
    do
    {
        digits[count++] = (char)(number % 10);
        number /= 10;
    } while (number > 0 && count < 15);
    float width = (count * (kDigitWidth + 1) - 1) * scale;
    float x = std::floor(centreX - width * 0.5f + 0.5f);
    float y = std::floor(bottom - kDigitHeight * scale + 0.5f);
    for (int pass = 0; pass < 2; ++pass)
    {
        float offset = pass == 0 ? scale : 0.0f;
        unsigned char shade = pass == 0 ? 40 : 255;
        for (int index = 0; index < count; ++index)
        {
            int digit = digits[count - 1 - index];
            PushTinted(vertices, x + offset + index * (kDigitWidth + 1) * scale, y + offset, kDigitWidth * scale,
                       kDigitHeight * scale, kAtlasDigitsLeft + digit * (kDigitWidth + 1), kAtlasPadTop, kDigitWidth,
                       kDigitHeight, shade, shade, shade, 255);
        }
    }
}

void PushButton(std::vector<Vertex>& vertices, float x, float y, float size, int frame, int icon, unsigned char alpha)
{
    x = std::floor(x + 0.5f);
    y = std::floor(y + 0.5f);
    PushSprite(vertices, x, y, size, size, frame * kButtonTexels, 0, kButtonTexels, kButtonTexels, alpha);
    if (icon == EIcon_None)
        return;
    float texel = size / (float)kButtonTexels;
    float inset = (float)(kButtonTexels - kIconTexels) * 0.5f * texel;
    PushSprite(vertices, x + inset, y + inset, kIconTexels * texel, kIconTexels * texel, icon * kIconTexels,
               kAtlasIconTop, kIconTexels, kIconTexels, alpha);
}

void PushPanel(std::vector<Vertex>& vertices, float x, float y, float width, float height, float texel, int frame,
               unsigned char alpha)
{
    x = std::floor(x + 0.5f);
    y = std::floor(y + 0.5f);
    const float corner = kPanelCorner * texel;
    const int left = frame * kButtonTexels;
    const float xs[4] = {x, x + corner, x + width - corner, x + width};
    const float ys[4] = {y, y + corner, y + height - corner, y + height};
    const int us[4] = {left, left + kPanelCorner, left + kButtonTexels - kPanelCorner, left + kButtonTexels};
    const int vs[4] = {0, kPanelCorner, kButtonTexels - kPanelCorner, kButtonTexels};
    for (int row = 0; row < 3; ++row)
    {
        for (int column = 0; column < 3; ++column)
        {
            PushSprite(vertices, xs[column], ys[row], xs[column + 1] - xs[column], ys[row + 1] - ys[row], us[column],
                       vs[row], us[column + 1] - us[column], vs[row + 1] - vs[row], alpha);
        }
    }
}

void PushGlyphs(std::vector<Vertex>& vertices, const std::wstring& text, float x, float y, float scale,
                unsigned char shade, unsigned char alpha)
{
    const int cellWidth = sFont.width / kGlyphCells;
    const int cellHeight = sFont.height / kGlyphCells;
    const float texel = scale * (float)kGlyphTexels / (float)cellWidth;
    for (size_t index = 0; index < text.size(); ++index)
    {
        const int glyph = GlyphFor(text[index]);
        if (glyph < 0)
            continue;
        const int advance = sFont.advances[glyph];
        if (advance > 1)
        {
            const float u0 = (float)((glyph % kGlyphCells) * cellWidth) / (float)sFont.width;
            const float v0 = (float)((glyph / kGlyphCells) * cellHeight) / (float)sFont.height;
            const float u1 = u0 + (float)(advance - 1) / (float)sFont.width;
            const float v1 = v0 + (float)cellHeight / (float)sFont.height;
            const float right = x + (advance - 1) * texel;
            const float bottom = y + cellHeight * texel;
            Vertex corners[4] = {{x, y, u0, v0, {shade, shade, shade, alpha}},
                                 {right, y, u1, v0, {shade, shade, shade, alpha}},
                                 {right, bottom, u1, v1, {shade, shade, shade, alpha}},
                                 {x, bottom, u0, v1, {shade, shade, shade, alpha}}};
            vertices.push_back(corners[0]);
            vertices.push_back(corners[1]);
            vertices.push_back(corners[2]);
            vertices.push_back(corners[0]);
            vertices.push_back(corners[2]);
            vertices.push_back(corners[3]);
        }
        x += advance * texel;
    }
}

void PushText(std::vector<Vertex>& vertices, const std::wstring& text, float x, float y, float scale,
              unsigned char alpha)
{
    x = std::floor(x + 0.5f);
    y = std::floor(y + 0.5f);
    PushGlyphs(vertices, text, x + scale, y + scale, scale, 63, alpha);
    PushGlyphs(vertices, text, x, y, scale, 255, alpha);
}

void DrawBatch(const std::vector<Vertex>& vertices, GLuint texture)
{
    if (vertices.empty() || texture == 0)
        return;
    Gl::BindTexture(Gl::TEXTURE_2D, texture);
    Gl::BufferData(Gl::ARRAY_BUFFER, (GLsizeiptr)(vertices.size() * sizeof(Vertex)), vertices.data(), Gl::STREAM_DRAW);
    Gl::DrawArrays(Gl::TRIANGLES, 0, (GLsizei)vertices.size());
}

bool UploadFont()
{
    if (sFont.texture != 0)
        return true;
    LoadFont();
    if (sFont.pixels.empty())
        return false;
    Gl::GenTextures(1, &sFont.texture);
    Gl::BindTexture(Gl::TEXTURE_2D, sFont.texture);
    Gl::PixelStorei(Gl::UNPACK_ALIGNMENT, 4);
    Gl::TexImage2D(Gl::TEXTURE_2D, 0, (GLint)Gl::RGBA, sFont.width, sFont.height, 0, Gl::RGBA, Gl::UNSIGNED_BYTE,
                   sFont.pixels.data());
    Gl::TexParameteri(Gl::TEXTURE_2D, Gl::TEXTURE_MIN_FILTER, (GLint)Gl::NEAREST);
    Gl::TexParameteri(Gl::TEXTURE_2D, Gl::TEXTURE_MAG_FILTER, (GLint)Gl::NEAREST);
    Gl::TexParameteri(Gl::TEXTURE_2D, Gl::TEXTURE_WRAP_S, (GLint)Gl::CLAMP_TO_EDGE);
    Gl::TexParameteri(Gl::TEXTURE_2D, Gl::TEXTURE_WRAP_T, (GLint)Gl::CLAMP_TO_EDGE);
    return true;
}

float Distance(float x0, float y0, float x1, float y1)
{
    float dx = x1 - x0;
    float dy = y1 - y0;
    return std::sqrt(dx * dx + dy * dy);
}

float Clamp(float value, float low, float high)
{
    return value < low ? low : (value > high ? high : value);
}
}

bool PcTouch::Rect::Contains(float px, float py) const
{
    return width > 0.0f && px >= x && px < x + width && py >= y && py < y + height;
}

PcTouch::PcTouch()
{
    mWidth = 1.0f;
    mHeight = 1.0f;
    mInUse = false;
    mSeen = false;
    std::memset(&mHud, 0, sizeof(mHud));
    mHud.opacity = 1.0f;
    mAimTarget = C_4JInput::ETouchAim_None;
    mSprintLatched = false;
    mPickArmed = false;
    mLastFlyDown = 0;
    std::memset(mDown, 0, sizeof(mDown));
    std::memset(mPressed, 0, sizeof(mPressed));
    std::memset(mHoldUntil, 0, sizeof(mHoldUntil));
    mMoveX = 0.0f;
    mMoveY = 0.0f;
    mMoving = false;
    mLookX = 0.0f;
    mLookY = 0.0f;
    mHotbarSlot = -1;
    mAiming = false;
    mAimX = 0.0f;
    mAimY = 0.0f;
    mMenuDown = false;
    mMenuMoved = false;
    mMenuX = 0.0f;
    mMenuY = 0.0f;
    mMenuClicks = 0;
    mMenuPinned = false;
    mMenuHold = -1.0f;
    mMenuSplit = 0.0f;
    mMenuWheel = 0;
    mHoldAmount = 0;
    mHoldTotal = 0;
    mLastTap = 0;
    mLastTapX = 0.0f;
    mLastTapY = 0.0f;
    mContext = EContext_Menu;
    mContainerMenu = false;
}

void PcTouch::SetScreen(float width, float height)
{
    if (width > 0.0f && height > 0.0f)
    {
        mWidth = width;
        mHeight = height;
    }
}

bool PcTouch::InUse() const
{
    return mInUse;
}

bool PcTouch::Touching() const
{
    return !mFingers.empty();
}

bool PcTouch::Seen() const
{
    return mSeen;
}

void PcTouch::SetInUse(bool inUse)
{
    if (mInUse == inUse)
        return;
    mInUse = inUse;
    if (!inUse)
        Cancel();
}

void PcTouch::SetHud(const Hud& hud)
{
    mHud = hud;
    mHud.updated = Now();
}

bool PcTouch::HasHud() const
{
    return mHud.updated != 0 && Now() - mHud.updated < kHudTimeout;
}

bool PcTouch::GameHud() const
{
    return mContext == EContext_Game && HasHud();
}

void PcTouch::SetAimTarget(int target)
{
    mAimTarget = target;
}

void PcTouch::SetInteract(const wchar_t* label)
{
    if (label == NULL)
        mInteract.clear();
    else if (mInteract != label)
        mInteract = label;
}

PcTouch::Layout PcTouch::BuildLayout() const
{
    Layout layout;
    std::memset(&layout, 0, sizeof(layout));
    const float width = mWidth;
    const float height = mHeight;
    float scale = std::floor(height / 240.0f + 0.5f);
    if (scale < 2.0f)
        scale = 2.0f;
    float topScale = scale - 1.0f < 2.0f ? 2.0f : scale - 1.0f;
    layout.scale = scale;
    layout.topScale = topScale;

    const unsigned int flags = mHud.flags;
    const unsigned int options = mHud.options;
    const int scheme = mHud.scheme;
    const bool spectator = (flags & C_4JInput::ETouchHud_Spectator) != 0;
    const bool flying = (flags & C_4JInput::ETouchHud_Flying) != 0;
    const bool swimming = (flags & C_4JInput::ETouchHud_Swimming) != 0;
    const bool riding = (flags & C_4JInput::ETouchHud_Riding) != 0;
    const bool crosshair = scheme == C_4JInput::ETouchScheme_JoystickCrosshair;
    const bool leftHanded = (options & C_4JInput::ETouchOption_LeftHanded) != 0;
    const bool swapped = (options & C_4JInput::ETouchOption_SwapJumpSneak) != 0;

    const float button = kButtonTexels * scale;
    const float edge = 8.0f * scale;
    const float gap = 4.0f * scale;

    if ((flags & C_4JInput::ETouchHud_Hotbar) != 0 && mHud.hotbarWidth > 0.0f)
    {
        layout.hotbar.x = mHud.hotbarLeft * width;
        layout.hotbar.y = mHud.hotbarTop * height;
        layout.hotbar.width = mHud.hotbarWidth * width;
        layout.hotbar.height = mHud.hotbarHeight * height;
    }

    layout.shown[EButton_Jump] = true;
    layout.shown[EButton_Pause] = true;
    layout.shown[EButton_Chat] = !spectator;
    if (scheme == C_4JInput::ETouchScheme_DPadTap)
        layout.shown[EButton_Sneak] = flying || swimming || riding;
    else
        layout.shown[EButton_Sneak] = true;
    layout.shown[EButton_Attack] = crosshair && !spectator;
    layout.shown[EButton_Use] = layout.shown[EButton_Attack];
    layout.shown[EButton_Sprint] = scheme != C_4JInput::ETouchScheme_DPadTap && !spectator && !riding;
    layout.shown[EButton_PickBlock] = (options & C_4JInput::ETouchOption_PickBlock) != 0 && !spectator;
    layout.shown[EButton_Perspective] = (options & C_4JInput::ETouchOption_Perspective) != 0;

    const float rightColumn = width - edge - button;
    const float leftColumn = rightColumn - button - gap;
    float bottomRow = height - edge - button - gap;
    Rect& jump = layout.buttons[EButton_Jump];
    jump.width = jump.height = button;
    jump.x = swapped ? leftColumn : rightColumn;
    Rect& sneak = layout.buttons[EButton_Sneak];
    sneak.width = sneak.height = button;
    sneak.x = swapped ? rightColumn : leftColumn;
    Rect& sprint = layout.buttons[EButton_Sprint];
    sprint.width = sprint.height = button;
    sprint.x = leftColumn - button - gap;

    if (layout.hotbar.width > 0.0f && !spectator)
    {
        float slotScale = std::floor(layout.hotbar.height / kButtonTexels + 0.5f);
        if (slotScale < 1.0f)
            slotScale = 1.0f;
        float slot = kButtonTexels * slotScale;
        Rect& inventory = layout.buttons[EButton_Inventory];
        inventory.width = inventory.height = slot;
        inventory.y = layout.hotbar.y + layout.hotbar.height - slot;
        Rect& crafting = layout.buttons[EButton_Crafting];
        crafting = inventory;
        float rightmost = layout.hotbar.x + layout.hotbar.width;
        if (leftHanded)
        {
            inventory.x = layout.hotbar.x - 2.0f * scale - slot;
            crafting.x = inventory.x - 2.0f * scale - slot;
        }
        else
        {
            inventory.x = rightmost + 2.0f * scale;
            crafting.x = inventory.x + slot + 2.0f * scale;
            rightmost = crafting.x + crafting.width;
        }
        layout.shown[EButton_Inventory] = true;
        layout.shown[EButton_Crafting] = true;
        const float clusterLeft = layout.shown[EButton_Sprint] ? sprint.x : leftColumn;
        if (rightmost + gap > clusterLeft)
            bottomRow = layout.hotbar.y - gap - button;
    }
    jump.y = bottomRow;
    sneak.y = bottomRow;
    sprint.y = bottomRow;

    Rect& attack = layout.buttons[EButton_Attack];
    attack.width = attack.height = button;
    attack.x = leftColumn;
    attack.y = bottomRow - button - gap;
    Rect& use = layout.buttons[EButton_Use];
    use = attack;
    use.x = rightColumn;
    Rect& pickBlock = layout.buttons[EButton_PickBlock];
    pickBlock.width = pickBlock.height = button;
    pickBlock.x = rightColumn;
    pickBlock.y = (crosshair ? use.y : bottomRow) - button - gap;

    const float topButton = kButtonTexels * topScale;
    Rect& pause = layout.buttons[EButton_Pause];
    pause.width = pause.height = topButton;
    pause.x = width - 4.0f * scale - topButton;
    pause.y = 4.0f * scale;
    Rect& chat = layout.buttons[EButton_Chat];
    chat = pause;
    chat.x = pause.x - topButton - 2.0f * scale;
    Rect& perspective = layout.buttons[EButton_Perspective];
    perspective = chat;
    if (!layout.shown[EButton_Chat])
        perspective.x = chat.x;
    else
        perspective.x = chat.x - topButton - 2.0f * scale;

    if (!mInteract.empty() && layout.hotbar.width > 0.0f && !spectator)
    {
        const float interactHeight = kInteractTexels * scale;
        const float textWidth = TextWidth(mInteract);
        float interactWidth = interactHeight;
        layout.interactText = textWidth > 0.0f;
        if (layout.interactText)
            interactWidth = std::floor(textWidth * scale + 0.5f) + 2.0f * kInteractPadding * scale;
        if (interactWidth < 2.0f * interactHeight && layout.interactText)
            interactWidth = 2.0f * interactHeight;
        float status = (flags & C_4JInput::ETouchHud_Survival) != 0 ? layout.hotbar.height : 0.0f;
        Rect& interact = layout.buttons[EButton_Interact];
        interact.width = interactWidth;
        interact.height = interactHeight;
        interact.x = layout.hotbar.x + (layout.hotbar.width - interactWidth) * 0.5f;
        interact.y = layout.hotbar.y - status - gap - interactHeight;
        layout.shown[EButton_Interact] = true;
    }

    layout.icons[EButton_Jump] = flying ? EIcon_FlyUp : (swimming ? EIcon_SwimUp : EIcon_Jump);
    layout.icons[EButton_Sneak] = flying ? EIcon_FlyDown
                                         : (swimming ? EIcon_SwimDown : (riding ? EIcon_Dismount : EIcon_Sneak));
    layout.icons[EButton_Attack] = EIcon_Attack;
    layout.icons[EButton_Use] = EIcon_Use;
    layout.icons[EButton_Pause] = EIcon_Pause;
    layout.icons[EButton_Chat] = EIcon_Chat;
    layout.icons[EButton_Inventory] = EIcon_Inventory;
    layout.icons[EButton_Crafting] = EIcon_Crafting;
    layout.icons[EButton_Sprint] = EIcon_Sprint;
    layout.icons[EButton_PickBlock] = EIcon_PickBlock;
    layout.icons[EButton_Perspective] = EIcon_Perspective;
    layout.icons[EButton_Interact] = layout.interactText ? EIcon_None : EIcon_Interact;

    float zoneRight = width * 0.45f;
    if (layout.hotbar.width > 0.0f)
    {
        float hotbarLeft = layout.shown[EButton_Crafting] && leftHanded ? layout.buttons[EButton_Crafting].x
                                                                        : layout.hotbar.x;
        if (hotbarLeft - gap < zoneRight)
            zoneRight = hotbarLeft - gap;
    }
    layout.joystickZone.x = 0.0f;
    layout.joystickZone.y = height * 0.35f;
    layout.joystickZone.width = zoneRight;
    layout.joystickZone.height = height - layout.joystickZone.y;
    layout.padSize = kPadTexels * scale;
    layout.travel = layout.padSize / 3.0f;
    layout.padX = edge + gap + layout.padSize * 0.5f;
    layout.padY = height - edge - gap - layout.padSize * 0.5f;

    layout.dpadCell = button;
    layout.dpad.width = layout.dpad.height = button * 3.0f;
    layout.dpad.x = edge + gap;
    layout.dpad.y = height - edge - gap - layout.dpad.height;
    return layout;
}

PcTouch::Finger* PcTouch::FindFinger(unsigned long long id)
{
    for (size_t index = 0; index < mFingers.size(); ++index)
    {
        if (mFingers[index].id == id)
            return &mFingers[index];
    }
    return NULL;
}

void PcTouch::PressAction(int action)
{
    if (action < 0 || action >= PcKeyboardMouse::ACTION_COUNT)
        return;
    mPressed[action] = true;
    unsigned long long until = Now() + kMinimumPress;
    if (until > mHoldUntil[action])
        mHoldUntil[action] = until;
}

void PcTouch::PulseAt(int action, float x, float y)
{
    Pulse pulse;
    pulse.action = action;
    pulse.until = Now() + kTapPulse;
    pulse.x = x;
    pulse.y = y;
    mPulses.push_back(pulse);
    PressAction(action);
}

bool PcTouch::FlyingDown() const
{
    return (mHud.flags & C_4JInput::ETouchHud_Flying) != 0;
}

void PcTouch::ButtonDown(int button)
{
    const unsigned long long now = Now();
    switch (button)
    {
    case EButton_Sneak:
        if (SneakButtonSneaks())
            break;
        if (FlyingDown() && mLastFlyDown != 0 && now - mLastFlyDown < kDoubleTap)
        {
            QueuedPress first = {EControllerActions_Jump, now};
            QueuedPress second = {EControllerActions_Jump, now + kFlyStopGap};
            mQueued.push_back(first);
            mQueued.push_back(second);
            mLastFlyDown = 0;
        }
        else
        {
            mLastFlyDown = FlyingDown() ? now : 0;
        }
        PressAction(EControllerActions_Sneak);
        break;
    case EButton_Sprint:
        mSprintLatched = !mSprintLatched;
        break;
    case EButton_PickBlock:
        if (mHud.scheme == C_4JInput::ETouchScheme_JoystickCrosshair)
            PressAction(EControllerActions_PickBlock);
        else
            mPickArmed = !mPickArmed;
        break;
    case EButton_Interact:
        PulseAt(EControllerActions_Use, mWidth * 0.5f, mHeight * 0.5f);
        break;
    default:
        PressAction(ButtonAction(button));
        break;
    }
}

int PcTouch::DPadCell(const Layout& layout, float x, float y) const
{
    const Rect& pad = layout.dpad;
    float reach = layout.dpadCell * 0.5f;
    if (x < pad.x - reach || x >= pad.x + pad.width + reach || y < pad.y - reach || y >= pad.y + pad.height + reach)
        return -1;
    int column = (int)Clamp((x - pad.x) / layout.dpadCell, 0.0f, 2.0f);
    int row = (int)Clamp((y - pad.y) / layout.dpadCell, 0.0f, 2.0f);
    return row * 3 + column;
}

void PcTouch::DPadMove(int cell, float* x, float* y) const
{
    static const float kMoves[9][2] = {{-1.0f, 1.0f}, {0.0f, 1.0f}, {1.0f, 1.0f},  {-1.0f, 0.0f}, {0.0f, 0.0f},
                                       {1.0f, 0.0f},  {0.0f, 0.0f}, {0.0f, -1.0f}, {0.0f, 0.0f}};
    *x = 0.0f;
    *y = 0.0f;
    if (cell >= 0 && cell < 9)
    {
        *x = kMoves[cell][0];
        *y = kMoves[cell][1];
    }
}

void PcTouch::FingerDown(unsigned long long id, float x, float y, EContext context)
{
    Finger finger;
    std::memset(&finger, 0, sizeof(finger));
    finger.id = id;
    finger.x = finger.startX = finger.lastX = x;
    finger.y = finger.startY = finger.lastY = y;
    finger.downTime = Now();
    finger.world = EWorld_Pending;
    finger.button = -1;
    finger.role = ERole_None;

    if (context == EContext_Menu)
    {
        bool menuFinger = false;
        for (size_t index = 0; index < mFingers.size(); ++index)
        {
            if (mFingers[index].role == ERole_Menu)
                menuFinger = true;
        }
        if (!menuFinger)
        {
            finger.role = ERole_Menu;
            mMenuDown = true;
            mMenuMoved = true;
            mMenuX = x;
            mMenuY = y;
        }
        mFingers.push_back(finger);
        return;
    }

    if (!HasHud())
    {
        mFingers.push_back(finger);
        return;
    }

    Layout layout = BuildLayout();
    for (int button = 0; button < EButton_Count && finger.role == ERole_None; ++button)
    {
        if (layout.shown[button] && layout.buttons[button].Contains(x, y))
        {
            finger.role = ERole_Button;
            finger.button = button;
            ButtonDown(button);
        }
    }
    if (finger.role == ERole_None && layout.hotbar.Contains(x, y) && (mHud.flags & C_4JInput::ETouchHud_Spectator) == 0)
    {
        finger.role = ERole_Hotbar;
        int slot = (int)Clamp((x - layout.hotbar.x) / (layout.hotbar.width / 9.0f), 0.0f, 8.0f);
        finger.button = slot;
        finger.nextDrop = finger.downTime + kDropDelay;
        mHotbarSlot = slot;
    }
    if (finger.role == ERole_None)
    {
        bool movementTaken = false;
        for (size_t index = 0; index < mFingers.size(); ++index)
        {
            if (mFingers[index].role == ERole_Joystick || mFingers[index].role == ERole_DPad)
                movementTaken = true;
        }
        if (mHud.scheme == C_4JInput::ETouchScheme_DPadTap)
        {
            int cell = DPadCell(layout, x, y);
            bool shownCell = cell == 1 || cell == 3 || cell == 4 || cell == 5 || cell == 7;
            if (shownCell && layout.dpad.Contains(x, y) && !movementTaken)
            {
                finger.role = ERole_DPad;
                finger.button = cell;
                if (cell == 4)
                    PressAction(EControllerActions_Sneak);
            }
        }
        else if (layout.joystickZone.Contains(x, y) && !movementTaken)
        {
            finger.role = ERole_Joystick;
            float half = layout.padSize * 0.5f;
            finger.originX = Clamp(x, half, mWidth - half);
            finger.originY = Clamp(y, half, mHeight - half);
        }
    }
    if (finger.role == ERole_None)
        finger.role = ERole_World;
    mFingers.push_back(finger);
}

void PcTouch::FingerMoved(Finger& finger, float x, float y)
{
    finger.x = x;
    finger.y = y;
    float slop = kSlop * mHeight;
    switch (finger.role)
    {
    case ERole_Menu:
        if (mContainerMenu)
        {
            MenuFingerMoved(finger);
            break;
        }
        mMenuX = x;
        mMenuY = y;
        mMenuMoved = true;
        if (!finger.moved && Distance(finger.startX, finger.startY, x, y) > slop)
            finger.moved = true;
        break;
    case ERole_World:
        if (!finger.moved && Distance(finger.startX, finger.startY, x, y) > slop)
        {
            finger.moved = true;
            finger.lastX = x;
            finger.lastY = y;
            if (finger.world == EWorld_Pending)
                finger.world = EWorld_Looking;
        }
        if (finger.moved)
        {
            mLookX += (x - finger.lastX) / mWidth * 4.0f;
            mLookY += (y - finger.lastY) / mHeight * 2.0f;
            finger.lastX = x;
            finger.lastY = y;
        }
        break;
    case ERole_Hotbar:
    {
        Layout layout = BuildLayout();
        if (layout.hotbar.width > 0.0f && y >= layout.hotbar.y - layout.hotbar.height)
        {
            int slot = (int)Clamp((x - layout.hotbar.x) / (layout.hotbar.width / 9.0f), 0.0f, 8.0f);
            if (slot != finger.button)
            {
                finger.button = slot;
                finger.nextDrop = Now() + kDropDelay;
                mHotbarSlot = slot;
            }
        }
        break;
    }
    case ERole_DPad:
    {
        finger.button = DPadCell(BuildLayout(), x, y);
        break;
    }
    default:
        break;
    }
}

void PcTouch::MenuFingerMoved(Finger& finger)
{
    const float slop = kSlop * mHeight;
    const unsigned long long now = Now();
    if ((finger.menu == EMenuGesture_Pending || finger.menu == EMenuGesture_Hold)
        && Distance(finger.startX, finger.startY, finger.x, finger.y) > slop)
    {
        if (finger.menu == EMenuGesture_Hold || now - finger.downTime >= kDragDwell)
        {
            finger.menu = EMenuGesture_Drag;
            mMenuClicks |= C_4JInput::EPcMenuClick_DragStart;
            mMenuPinned = true;
            mMenuHold = -1.0f;
        }
        else
        {
            finger.menu = EMenuGesture_Swipe;
            finger.lastY = finger.startY;
        }
    }
    if (finger.menu == EMenuGesture_Swipe)
    {
        const float step = kSwipeStep * mHeight;
        while (finger.y - finger.lastY >= step)
        {
            ++mMenuWheel;
            finger.lastY += step;
        }
        while (finger.lastY - finger.y >= step)
        {
            --mMenuWheel;
            finger.lastY -= step;
        }
    }
}

void PcTouch::MenuFingerUp(Finger& finger)
{
    const unsigned long long now = Now();
    mMenuDown = false;
    if (!mMenuPinned)
    {
        mMenuX = finger.x;
        mMenuY = finger.y;
        mMenuMoved = true;
    }
    switch (finger.menu)
    {
    case EMenuGesture_Drag:
        mMenuClicks |= C_4JInput::EPcMenuClick_DragEnd;
        break;
    case EMenuGesture_Hold:
        mMenuSplit = Clamp((float)(now - finger.downTime - kSplitHold) / (float)kSplitFill, 0.0f, 1.0f);
        mMenuClicks |= C_4JInput::EPcMenuClick_Split;
        break;
    case EMenuGesture_Swipe:
        break;
    default:
        if (mLastTap != 0 && now - mLastTap < kDoubleTap
            && Distance(mLastTapX, mLastTapY, finger.x, finger.y) < 2.0f * kSlop * mHeight)
        {
            mMenuClicks |= C_4JInput::EPcMenuClick_Auto;
            mLastTap = 0;
        }
        else
        {
            mMenuClicks |= C_4JInput::EPcMenuClick_Primary;
            mLastTap = now;
            mLastTapX = finger.x;
            mLastTapY = finger.y;
        }
        break;
    }
    mMenuHold = -1.0f;
}

bool PcTouch::SneakButtonSneaks() const
{
    const unsigned int other = C_4JInput::ETouchHud_Flying | C_4JInput::ETouchHud_Swimming
                               | C_4JInput::ETouchHud_Riding;
    return (mHud.flags & other) == 0;
}

void PcTouch::UpdateSneakButton(Finger& finger, unsigned long long now)
{
    const bool holdMode = (mHud.flags & C_4JInput::ETouchHud_HoldToSneak) != 0;
    if (!finger.held && now - finger.downTime >= kSneakHold)
    {
        finger.held = true;
        if (!holdMode && (mHud.flags & C_4JInput::ETouchHud_Sneaking) == 0)
            PressAction(EControllerActions_Sneak);
    }
    if (finger.held && holdMode)
        mDown[EControllerActions_Sneak] = true;
}

void PcTouch::FingerUp(Finger& finger)
{
    if (finger.role == ERole_Button && finger.button == EButton_Sneak && SneakButtonSneaks())
    {
        const bool holdMode = (mHud.flags & C_4JInput::ETouchHud_HoldToSneak) != 0;
        if (!finger.held)
            PressAction(EControllerActions_Sneak);
        else if (!holdMode && (mHud.flags & C_4JInput::ETouchHud_Sneaking) != 0)
            PressAction(EControllerActions_Sneak);
        return;
    }
    if (finger.role == ERole_Menu)
    {
        if (mContainerMenu)
        {
            MenuFingerUp(finger);
            return;
        }
        mMenuDown = false;
        mMenuX = finger.x;
        mMenuY = finger.y;
        return;
    }
    if (finger.role == ERole_World && finger.world == EWorld_Pending && !finger.moved
        && mHud.scheme != C_4JInput::ETouchScheme_JoystickCrosshair
        && (mHud.flags & C_4JInput::ETouchHud_Spectator) == 0)
    {
        int action = mAimTarget == C_4JInput::ETouchAim_Entity ? EControllerActions_Action : EControllerActions_Use;
        if (mPickArmed)
        {
            action = EControllerActions_PickBlock;
            mPickArmed = false;
        }
        PulseAt(action, finger.x, finger.y);
    }
}

void PcTouch::HandleEvent(const SDL_Event& event, EContext context)
{
    const unsigned long long id = (unsigned long long)event.tfinger.fingerID;
    const float x = event.tfinger.x * mWidth;
    const float y = event.tfinger.y * mHeight;
    switch (event.type)
    {
    case SDL_EVENT_FINGER_DOWN:
    {
        mInUse = true;
        mSeen = true;
        Finger* existing = FindFinger(id);
        if (existing != NULL)
            FingerUp(*existing);
        for (size_t index = 0; index < mFingers.size(); ++index)
        {
            if (mFingers[index].id == id)
            {
                mFingers.erase(mFingers.begin() + index);
                break;
            }
        }
        if (context != mContext)
        {
            Cancel();
            mContext = context;
        }
        FingerDown(id, x, y, context);
        break;
    }
    case SDL_EVENT_FINGER_MOTION:
    {
        Finger* finger = FindFinger(id);
        if (finger != NULL)
            FingerMoved(*finger, x, y);
        break;
    }
    case SDL_EVENT_FINGER_UP:
    case SDL_EVENT_FINGER_CANCELED:
        for (size_t index = 0; index < mFingers.size(); ++index)
        {
            if (mFingers[index].id != id)
                continue;
            if (event.type == SDL_EVENT_FINGER_UP)
            {
                FingerMoved(mFingers[index], x, y);
                FingerUp(mFingers[index]);
            }
            else if (mFingers[index].role == ERole_Menu)
            {
                mMenuDown = false;
            }
            mFingers.erase(mFingers.begin() + index);
            break;
        }
        break;
    default:
        break;
    }
}

void PcTouch::Cancel()
{
    for (size_t index = 0; index < mFingers.size(); ++index)
    {
        mFingers[index].role = ERole_None;
        mFingers[index].button = -1;
    }
    mPulses.clear();
    mQueued.clear();
    mPickArmed = false;
    mLastFlyDown = 0;
    std::memset(mDown, 0, sizeof(mDown));
    std::memset(mPressed, 0, sizeof(mPressed));
    std::memset(mHoldUntil, 0, sizeof(mHoldUntil));
    mMoveX = 0.0f;
    mMoveY = 0.0f;
    mMoving = false;
    mLookX = 0.0f;
    mLookY = 0.0f;
    mHotbarSlot = -1;
    mAiming = false;
    mMenuDown = false;
    mMenuMoved = false;
    mMenuClicks = 0;
    mMenuPinned = false;
    mMenuHold = -1.0f;
    mMenuWheel = 0;
    mHoldAmount = 0;
    mHoldTotal = 0;
}

void PcTouch::Update(EContext context, bool containerMenu)
{
    if (context != mContext)
    {
        Cancel();
        mContext = context;
    }
    mContainerMenu = context == EContext_Menu && containerMenu;

    const unsigned long long now = Now();
    std::memset(mDown, 0, sizeof(mDown));
    mMoveX = 0.0f;
    mMoveY = 0.0f;
    mMoving = false;
    mAiming = false;

    for (size_t index = 0; index < mPulses.size();)
    {
        if (now >= mPulses[index].until)
            mPulses.erase(mPulses.begin() + index);
        else
            ++index;
    }
    for (size_t index = 0; index < mQueued.size();)
    {
        if (now >= mQueued[index].at)
        {
            PressAction(mQueued[index].action);
            mQueued.erase(mQueued.begin() + index);
        }
        else
        {
            ++index;
        }
    }

    if (context == EContext_Menu)
    {
        for (size_t index = 0; index < mFingers.size(); ++index)
        {
            Finger& finger = mFingers[index];
            if (finger.role != ERole_Menu || !mContainerMenu)
                continue;
            if (finger.menu == EMenuGesture_Pending && now - finger.downTime >= kSplitHold)
                finger.menu = EMenuGesture_Hold;
            if (finger.menu == EMenuGesture_Hold)
                mMenuHold = Clamp((float)(now - finger.downTime - kSplitHold) / (float)kSplitFill, 0.0f, 1.0f);
            if (!mMenuPinned && (mMenuX != finger.x || mMenuY != finger.y))
            {
                mMenuX = finger.x;
                mMenuY = finger.y;
                mMenuMoved = true;
            }
        }
        return;
    }

    if (!HasHud())
        return;

    const Layout layout = BuildLayout();
    if (!layout.shown[EButton_Sprint])
        mSprintLatched = false;
    if (!layout.shown[EButton_PickBlock])
        mPickArmed = false;
    const bool tapScheme = mHud.scheme != C_4JInput::ETouchScheme_JoystickCrosshair;
    const unsigned long long hold = mHud.scheme == C_4JInput::ETouchScheme_DPadTap ? kHoldDPad : kHoldJoystick;
    const Finger* aimFinger = NULL;
    const Finger* pendingFinger = NULL;

    for (size_t index = 0; index < mFingers.size(); ++index)
    {
        Finger& finger = mFingers[index];
        switch (finger.role)
        {
        case ERole_Button:
            if (finger.button == EButton_Sneak && SneakButtonSneaks())
                UpdateSneakButton(finger, now);
            else if (ButtonAction(finger.button) >= 0)
                mDown[ButtonAction(finger.button)] = true;
            break;
        case ERole_Joystick:
        {
            float dx = finger.x - finger.originX;
            float dy = finger.y - finger.originY;
            float reach = std::sqrt(dx * dx + dy * dy) / layout.travel;
            if (reach > kDeadZone)
            {
                float strength = Clamp((reach - kDeadZone) / (1.0f - kDeadZone), 0.0f, 1.0f);
                float length = std::sqrt(dx * dx + dy * dy);
                mMoveX = dx / length * strength;
                mMoveY = -dy / length * strength;
                mMoving = true;
                const bool easySprint = (mHud.options & C_4JInput::ETouchOption_EasySprint) != 0;
                if ((easySprint && reach >= kSprintReach && mMoveY > 0.8f) || (mSprintLatched && mMoveY > 0.0f))
                    mDown[EControllerActions_Sprint] = true;
            }
            break;
        }
        case ERole_DPad:
            if (finger.button == 4)
            {
                if (DPadCell(layout, finger.startX, finger.startY) == 4)
                    mDown[EControllerActions_Sneak] = true;
            }
            else if (finger.button >= 0)
            {
                DPadMove(finger.button, &mMoveX, &mMoveY);
                mMoving = mMoveX != 0.0f || mMoveY != 0.0f;
            }
            break;
        case ERole_Hotbar:
            if (now >= finger.nextDrop)
            {
                PressAction(EControllerActions_Drop);
                finger.nextDrop = now + kDropRepeat;
            }
            break;
        case ERole_World:
            if (!tapScheme || (mHud.flags & C_4JInput::ETouchHud_Spectator) != 0)
                break;
            if (finger.world == EWorld_Pending && !finger.moved && now - finger.downTime >= hold)
                finger.world = EWorld_Holding;
            if (finger.world == EWorld_Holding)
            {
                bool useItem = (mHud.flags & C_4JInput::ETouchHud_HoldToUse) != 0
                               && mAimTarget != C_4JInput::ETouchAim_Entity;
                mDown[useItem ? EControllerActions_Use : EControllerActions_Action] = true;
                aimFinger = &finger;
            }
            else if (finger.world == EWorld_Pending && pendingFinger == NULL)
            {
                pendingFinger = &finger;
            }
            break;
        default:
            break;
        }
    }

    if (tapScheme)
    {
        if (aimFinger != NULL)
        {
            mAiming = true;
            mAimX = aimFinger->x / mWidth;
            mAimY = aimFinger->y / mHeight;
        }
        else if (!mPulses.empty())
        {
            mAiming = true;
            mAimX = mPulses.back().x / mWidth;
            mAimY = mPulses.back().y / mHeight;
        }
        else if (pendingFinger != NULL)
        {
            mAiming = true;
            mAimX = pendingFinger->x / mWidth;
            mAimY = pendingFinger->y / mHeight;
        }
    }
    for (size_t index = 0; index < mPulses.size(); ++index)
        mDown[mPulses[index].action] = true;
}

void PcTouch::ApplyActions(PcKeyboardMouse& keyboardMouse)
{
    const unsigned long long now = Now();
    bool down[PcKeyboardMouse::ACTION_COUNT];
    for (int action = 0; action < PcKeyboardMouse::ACTION_COUNT; ++action)
        down[action] = mDown[action] || mPressed[action] || now < mHoldUntil[action];
    keyboardMouse.SetTouchInput(down, mPressed, mMoveX, mMoveY, mMoving);
    std::memset(mPressed, 0, sizeof(mPressed));
}

bool PcTouch::GetAim(float* x, float* y) const
{
    if (!mAiming || !GameHud())
        return false;
    *x = mAimX;
    *y = mAimY;
    return true;
}

bool PcTouch::ConsumeLook(float* x, float* y)
{
    if (mLookX == 0.0f && mLookY == 0.0f)
        return false;
    *x = mLookX;
    *y = mLookY;
    mLookX = 0.0f;
    mLookY = 0.0f;
    return true;
}

bool PcTouch::ConsumeHotbar(int* slot)
{
    if (mHotbarSlot < 0)
        return false;
    *slot = mHotbarSlot;
    mHotbarSlot = -1;
    return true;
}

bool PcTouch::MenuPointer(float* x, float* y) const
{
    *x = mMenuX / mWidth;
    *y = mMenuY / mHeight;
    return mInUse;
}

bool PcTouch::MenuDown() const
{
    return mMenuDown;
}

bool PcTouch::ConsumeMenuMoved()
{
    bool moved = mMenuMoved;
    mMenuMoved = false;
    return moved;
}

unsigned int PcTouch::ConsumeMenuClicks()
{
    unsigned int clicks = mMenuClicks;
    mMenuClicks = 0;
    mMenuPinned = false;
    return clicks;
}

bool PcTouch::MenuHold(float* progress) const
{
    if (mMenuHold < 0.0f)
        return false;
    *progress = mMenuHold;
    return true;
}

float PcTouch::ConsumeMenuSplit()
{
    float split = mMenuSplit;
    mMenuSplit = 0.0f;
    return split;
}

int PcTouch::ConsumeMenuWheel()
{
    int steps = mMenuWheel;
    mMenuWheel = 0;
    return steps;
}

void PcTouch::SetMenuHoldAmount(int amount, int total)
{
    mHoldAmount = amount;
    mHoldTotal = total;
}

int PcTouch::PromptArt(int action) const
{
    const unsigned int flags = mHud.flags;
    const bool flying = (flags & C_4JInput::ETouchHud_Flying) != 0;
    const bool swimming = (flags & C_4JInput::ETouchHud_Swimming) != 0;
    const bool riding = (flags & C_4JInput::ETouchHud_Riding) != 0;
    int icon = EIcon_None;
    switch (action)
    {
    case EControllerActions_MoveForward:
        if (mHud.scheme != C_4JInput::ETouchScheme_DPadTap)
            return PC_PROMPT_ART_TOUCH_JOYSTICK;
        icon = EIcon_ArrowUp;
        break;
    case EControllerActions_MoveBackward:
        if (mHud.scheme != C_4JInput::ETouchScheme_DPadTap)
            return PC_PROMPT_ART_TOUCH_JOYSTICK;
        icon = EIcon_ArrowDown;
        break;
    case EControllerActions_MoveLeft:
        if (mHud.scheme != C_4JInput::ETouchScheme_DPadTap)
            return PC_PROMPT_ART_TOUCH_JOYSTICK;
        icon = EIcon_ArrowLeft;
        break;
    case EControllerActions_MoveRight:
        if (mHud.scheme != C_4JInput::ETouchScheme_DPadTap)
            return PC_PROMPT_ART_TOUCH_JOYSTICK;
        icon = EIcon_ArrowRight;
        break;
    case EControllerActions_LookUp:
    case EControllerActions_LookDown:
    case EControllerActions_LookLeft:
    case EControllerActions_LookRight:
        icon = EIcon_Look;
        break;
    case EControllerActions_Jump:
    case EControllerActions_FlyUp:
        icon = flying ? EIcon_FlyUp : (swimming ? EIcon_SwimUp : EIcon_Jump);
        break;
    case EControllerActions_Sneak:
    case EControllerActions_FlyDown:
        icon = flying ? EIcon_FlyDown : (swimming ? EIcon_SwimDown : (riding ? EIcon_Dismount : EIcon_Sneak));
        break;
    case EControllerActions_Action:
        icon = EIcon_Attack;
        break;
    case EControllerActions_Use:
        icon = EIcon_Use;
        break;
    case EControllerActions_Pause:
        icon = EIcon_Pause;
        break;
    case EControllerActions_Chat:
        icon = EIcon_Chat;
        break;
    case EControllerActions_Inventory:
        icon = EIcon_Inventory;
        break;
    case EControllerActions_Crafting:
        icon = EIcon_Crafting;
        break;
    case EControllerActions_Sprint:
        icon = EIcon_Sprint;
        break;
    case EControllerActions_PickBlock:
        icon = EIcon_PickBlock;
        break;
    case EControllerActions_ChangeCameraMode:
        icon = EIcon_Perspective;
        break;
    default:
        break;
    }
    return icon == EIcon_None ? -1 : PC_PROMPT_ART_TOUCH_BASE + icon;
}

void PcTouch::Draw(int width, int height)
{
    if (width <= 0 || height <= 0 || !mInUse)
        return;
    const bool splitBar = mContext == EContext_Menu && mContainerMenu && mMenuHold >= 0.0f && mHoldTotal > 0;
    if (!splitBar && !GameHud())
        return;
    if (!sOverlay.initialised)
        Initialise();
    if (!sOverlay.available)
        return;

    SetScreen((float)width, (float)height);
    const Layout layout = BuildLayout();
    std::vector<Vertex> vertices;
    std::vector<Vertex> text;
    if (splitBar)
    {
        const float scale = layout.scale;
        const float barWidth = 24.0f * scale;
        const float barHeight = 3.0f * scale;
        const float left = std::floor(mMenuX - barWidth * 0.5f + 0.5f);
        const float top = std::floor(mMenuY - 16.0f * scale + 0.5f);
        const float fill = (float)mHoldAmount / (float)mHoldTotal;
        PushBlock(vertices, left - scale, top - scale, barWidth + 2.0f * scale, barHeight + 2.0f * scale, 16, 16, 16,
                  220);
        PushBlock(vertices, left, top, barWidth, barHeight, 88, 88, 88, 255);
        PushBlock(vertices, left, top, std::floor(barWidth * fill + 0.5f), barHeight, 236, 236, 236, 255);
        PushNumber(vertices, mMenuX, top - 2.0f * scale, scale, mHoldAmount);
    }
    else
    {
        const float opacity = Clamp(mHud.opacity, 0.0f, 1.0f);
        const unsigned char rest = (unsigned char)(200.0f * opacity + 0.5f);
        const unsigned char active = (unsigned char)(255.0f * opacity + 0.5f);

        bool pressed[EButton_Count];
        std::memset(pressed, 0, sizeof(pressed));
        const Finger* joystick = NULL;
        int dpadCell = -1;
        bool dpadSneak = false;
        for (size_t index = 0; index < mFingers.size(); ++index)
        {
            const Finger& finger = mFingers[index];
            if (finger.role == ERole_Button && finger.button >= 0)
                pressed[finger.button] = true;
            else if (finger.role == ERole_Joystick)
                joystick = &finger;
            else if (finger.role == ERole_DPad)
            {
                dpadCell = finger.button;
                if (finger.button == 4 && DPadCell(layout, finger.startX, finger.startY) == 4)
                    dpadSneak = true;
            }
        }
        const bool sneaking = (mHud.flags & C_4JInput::ETouchHud_Sneaking) != 0;

        if (mHud.scheme == C_4JInput::ETouchScheme_DPadTap)
        {
            static const int kIcons[9] = {EIcon_ArrowUpLeft, EIcon_ArrowUp,   EIcon_ArrowUpRight,
                                          EIcon_ArrowLeft,   EIcon_Sneak,     EIcon_ArrowRight,
                                          EIcon_None,        EIcon_ArrowDown, EIcon_None};
            const bool forward = dpadCell >= 0 && dpadCell <= 2;
            for (int cell = 0; cell < 9; ++cell)
            {
                if (kIcons[cell] == EIcon_None || ((cell == 0 || cell == 2) && !forward))
                    continue;
                float x = layout.dpad.x + (cell % 3) * layout.dpadCell;
                float y = layout.dpad.y + (cell / 3) * layout.dpadCell;
                bool lit = cell == dpadCell && (cell != 4 || dpadSneak);
                int frame = lit ? EFrame_Pressed : (cell == 4 && sneaking ? EFrame_On : EFrame_Rest);
                PushButton(vertices, x, y, layout.dpadCell, frame, kIcons[cell], lit ? active : rest);
            }
        }
        else if (mHud.joystick == C_4JInput::ETouchJoystick_Visible
                 || (mHud.joystick == C_4JInput::ETouchJoystick_HiddenWhenUnused && joystick != NULL))
        {
            float padX = joystick != NULL ? joystick->originX : layout.padX;
            float padY = joystick != NULL ? joystick->originY : layout.padY;
            float half = layout.padSize * 0.5f;
            PushSprite(vertices, padX - half, padY - half, layout.padSize, layout.padSize, 0, kAtlasPadTop, kPadTexels,
                       kPadTexels, joystick != NULL ? active : rest);
            float knobX = padX;
            float knobY = padY;
            if (joystick != NULL)
            {
                float dx = joystick->x - padX;
                float dy = joystick->y - padY;
                float length = std::sqrt(dx * dx + dy * dy);
                if (length > layout.travel)
                {
                    dx = dx / length * layout.travel;
                    dy = dy / length * layout.travel;
                }
                knobX += dx;
                knobY += dy;
            }
            float knob = kButtonTexels * layout.scale;
            PushButton(vertices, knobX - knob * 0.5f, knobY - knob * 0.5f, knob,
                       joystick != NULL ? EFrame_Pressed : EFrame_Rest, EIcon_None, joystick != NULL ? active : rest);
        }

        for (int button = 0; button < EButton_Count; ++button)
        {
            if (!layout.shown[button])
                continue;
            const Rect& rect = layout.buttons[button];
            int frame = pressed[button] ? EFrame_Pressed : EFrame_Rest;
            if (!pressed[button] && button == EButton_Sneak && sneaking && layout.icons[button] == EIcon_Sneak)
                frame = EFrame_On;
            if (!pressed[button]
                && ((button == EButton_Sprint && mSprintLatched) || (button == EButton_PickBlock && mPickArmed)))
                frame = EFrame_On;
            unsigned char alpha = pressed[button] ? active : rest;
            if (button == EButton_Sprint && (mHud.flags & C_4JInput::ETouchHud_CanSprint) == 0)
                alpha = (unsigned char)(alpha / 2);
            if (button == EButton_Interact && layout.interactText)
            {
                PushPanel(vertices, rect.x, rect.y, rect.width, rect.height, layout.scale, frame, alpha);
                const float textWidth = std::floor(TextWidth(mInteract) * layout.scale + 0.5f);
                const float glyphHeight = kGlyphTexels * layout.scale;
                PushText(text, mInteract, rect.x + (rect.width - textWidth) * 0.5f,
                         rect.y + (rect.height - glyphHeight) * 0.5f, layout.scale, alpha);
                continue;
            }
            PushButton(vertices, rect.x, rect.y, rect.width, frame, layout.icons[button], alpha);
        }
    }
    if (vertices.empty())
        return;

    Gl::BindFramebuffer(Gl::FRAMEBUFFER, 0);
    Gl::Viewport(0, 0, width, height);
    Gl::Disable(Gl::DEPTH_TEST);
    Gl::Disable(Gl::STENCIL_TEST);
    Gl::Disable(Gl::SCISSOR_TEST);
    Gl::Disable(Gl::CULL_FACE);
    Gl::ColorMask(Gl::TRUE_VALUE, Gl::TRUE_VALUE, Gl::TRUE_VALUE, Gl::TRUE_VALUE);
    Gl::Enable(Gl::BLEND);
    Gl::BlendFuncSeparate(Gl::SRC_ALPHA, Gl::ONE_MINUS_SRC_ALPHA, Gl::ONE, Gl::ONE_MINUS_SRC_ALPHA);

    Gl::UseProgram(sOverlay.program);
    Gl::Uniform2f(sOverlay.screenLocation, (float)width, (float)height);
    Gl::Uniform1i(sOverlay.artLocation, 0);
    Gl::ActiveTexture(Gl::TEXTURE0);
    Gl::BindSampler(0, 0);

    Gl::BindVertexArray(sOverlay.vertexArray);
    Gl::BindBuffer(Gl::ARRAY_BUFFER, sOverlay.vertexBuffer);
    DrawBatch(vertices, sOverlay.texture);
    if (!text.empty() && UploadFont())
        DrawBatch(text, sFont.texture);
    Gl::BindVertexArray(0);
}

void PcTouch::Shutdown()
{
    if (!sOverlay.initialised)
        return;
    if (sOverlay.texture != 0)
        Gl::DeleteTextures(1, &sOverlay.texture);
    if (sOverlay.vertexBuffer != 0)
        Gl::DeleteBuffers(1, &sOverlay.vertexBuffer);
    if (sOverlay.vertexArray != 0)
        Gl::DeleteVertexArrays(1, &sOverlay.vertexArray);
    if (sOverlay.program != 0)
        Gl::DeleteProgram(sOverlay.program);
    std::memset(&sOverlay, 0, sizeof(sOverlay));
    if (sFont.texture != 0)
        Gl::DeleteTextures(1, &sFont.texture);
    sFont.texture = 0;
}
