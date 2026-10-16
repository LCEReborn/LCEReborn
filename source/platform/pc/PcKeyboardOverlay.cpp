#include "platform/pc/PcKeyboardOverlay.hpp"

#include <SDL3/SDL.h>

#include <stddef.h>
#include <stdio.h>

#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "platform/pc/GlApi.hpp"
#include "platform/pc/PcInputEvents.hpp"
#include "platform/pc/PcPaths.hpp"
#include "vendor/stb_truetype.hpp"

namespace
{
const int kAtlasSize = 512;
const int kFirstGlyph = 32;
const int kGlyphCount = 224;
const float kBakeHeight = 32.0f;
const float kReferenceHeight = 720.0f;
const float kTextHeight = 22.0f;
const unsigned long long kCaretPeriod = 1000000000ull;

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
uniform sampler2D uGlyphs;
out vec4 oColour;
void main()
{
    float coverage = vTexCoord.x < 0.0 ? 1.0 : texture(uGlyphs, vTexCoord).r;
    oColour = vec4(vColour.rgb, vColour.a * coverage);
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

struct Colour
{
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
};

struct OverlayState
{
    bool initialised;
    bool available;
    bool hasFont;
    GLuint program;
    GLint screenLocation;
    GLint glyphLocation;
    GLuint vertexArray;
    GLuint vertexBuffer;
    GLuint atlas;
    stbtt_bakedchar glyphs[kGlyphCount];
};

OverlayState sOverlay;

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
        fprintf(stderr, "Keyboard overlay shader failed: %.*s\n", (int)length, log);
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
        fprintf(stderr, "Keyboard overlay program failed to link\n");
        Gl::DeleteProgram(sOverlay.program);
        sOverlay.program = 0;
        return false;
    }

    sOverlay.screenLocation = Gl::GetUniformLocation(sOverlay.program, "uScreen");
    sOverlay.glyphLocation = Gl::GetUniformLocation(sOverlay.program, "uGlyphs");
    return true;
}

bool LoadFont()
{
    std::string path = PcContentRoot() + "/Common/Media/font/Mojangles.ttf";
    std::ifstream file(path.c_str(), std::ios::binary);
    if (!file)
    {
        fprintf(stderr, "Keyboard overlay font missing: %s\n", path.c_str());
        return false;
    }

    std::vector<unsigned char> fontData((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    std::vector<unsigned char> bitmap(kAtlasSize * kAtlasSize);
    if (stbtt_BakeFontBitmap(fontData.data(), 0, kBakeHeight, bitmap.data(), kAtlasSize, kAtlasSize, kFirstGlyph,
                             kGlyphCount, sOverlay.glyphs)
        <= 0)
    {
        fprintf(stderr, "Keyboard overlay font atlas too small for %s\n", path.c_str());
        return false;
    }

    Gl::GenTextures(1, &sOverlay.atlas);
    Gl::BindTexture(Gl::TEXTURE_2D, sOverlay.atlas);
    Gl::PixelStorei(Gl::UNPACK_ALIGNMENT, 1);
    Gl::TexImage2D(Gl::TEXTURE_2D, 0, (GLint)Gl::RED, kAtlasSize, kAtlasSize, 0, Gl::RED, Gl::UNSIGNED_BYTE,
                   bitmap.data());
    Gl::PixelStorei(Gl::UNPACK_ALIGNMENT, 4);
    Gl::TexParameteri(Gl::TEXTURE_2D, Gl::TEXTURE_MIN_FILTER, (GLint)Gl::NEAREST);
    Gl::TexParameteri(Gl::TEXTURE_2D, Gl::TEXTURE_MAG_FILTER, (GLint)Gl::NEAREST);
    Gl::TexParameteri(Gl::TEXTURE_2D, Gl::TEXTURE_WRAP_S, (GLint)Gl::CLAMP_TO_EDGE);
    Gl::TexParameteri(Gl::TEXTURE_2D, Gl::TEXTURE_WRAP_T, (GLint)Gl::CLAMP_TO_EDGE);
    return true;
}

void Initialise()
{
    sOverlay.initialised = true;
    if (!CreateProgram())
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

    sOverlay.hasFont = LoadFont();
    sOverlay.available = true;
}

void PushQuad(std::vector<Vertex>& vertices, float x0, float y0, float x1, float y1, float u0, float v0, float u1,
              float v1, Colour colour)
{
    Vertex corners[4] = {{x0, y0, u0, v0, {colour.r, colour.g, colour.b, colour.a}},
                         {x1, y0, u1, v0, {colour.r, colour.g, colour.b, colour.a}},
                         {x1, y1, u1, v1, {colour.r, colour.g, colour.b, colour.a}},
                         {x0, y1, u0, v1, {colour.r, colour.g, colour.b, colour.a}}};
    vertices.push_back(corners[0]);
    vertices.push_back(corners[1]);
    vertices.push_back(corners[2]);
    vertices.push_back(corners[0]);
    vertices.push_back(corners[2]);
    vertices.push_back(corners[3]);
}

void PushRect(std::vector<Vertex>& vertices, float x0, float y0, float x1, float y1, Colour colour)
{
    PushQuad(vertices, x0, y0, x1, y1, -1.0f, -1.0f, -1.0f, -1.0f, colour);
}

int GlyphIndex(wchar_t character)
{
    unsigned int code = static_cast<unsigned int>(character);
    if (code < static_cast<unsigned int>(kFirstGlyph) || code >= static_cast<unsigned int>(kFirstGlyph + kGlyphCount))
        code = '?';
    return static_cast<int>(code) - kFirstGlyph;
}

float TextWidth(const std::wstring& text, size_t begin, float scale)
{
    float width = 0.0f;
    for (size_t index = begin; index < text.size(); ++index)
        width += sOverlay.glyphs[GlyphIndex(text[index])].xadvance * scale;
    return width;
}

float PushText(std::vector<Vertex>& vertices, const std::wstring& text, size_t begin, float x, float baseline,
               float scale, Colour colour)
{
    for (size_t index = begin; index < text.size(); ++index)
    {
        const stbtt_bakedchar& glyph = sOverlay.glyphs[GlyphIndex(text[index])];
        float x0 = x + glyph.xoff * scale;
        float y0 = baseline + glyph.yoff * scale;
        float x1 = x0 + (glyph.x1 - glyph.x0) * scale;
        float y1 = y0 + (glyph.y1 - glyph.y0) * scale;
        PushQuad(vertices, x0, y0, x1, y1, glyph.x0 / (float)kAtlasSize, glyph.y0 / (float)kAtlasSize,
                 glyph.x1 / (float)kAtlasSize, glyph.y1 / (float)kAtlasSize, colour);
        x += glyph.xadvance * scale;
    }
    return x;
}

void BuildOverlay(std::vector<Vertex>& vertices, const std::wstring& title, const std::wstring& text, float width,
                  float height)
{
    const Colour dim = {0, 0, 0, 150};
    const Colour border = {0, 0, 0, 255};
    const Colour panel = {198, 198, 198, 255};
    const Colour highlight = {255, 255, 255, 255};
    const Colour shade = {85, 85, 85, 255};
    const Colour titleColour = {64, 64, 64, 255};
    const Colour focus = {255, 255, 0, 255};
    const Colour field = {108, 108, 108, 255};
    const Colour textColour = {224, 224, 224, 255};

    float unit = height / kReferenceHeight;
    float textScale = kTextHeight * unit / kBakeHeight;
    float edge = 2.0f * unit;
    float padding = 20.0f * unit;
    float lineHeight = kTextHeight * unit * 1.4f;

    float panelWidth = width * 0.5f < 640.0f * unit ? 640.0f * unit : width * 0.5f;
    if (panelWidth > width - 2.0f * padding)
        panelWidth = width - 2.0f * padding;
    float fieldHeight = lineHeight + 2.0f * edge + 8.0f * unit;
    float panelHeight = padding * 3.0f + lineHeight + fieldHeight;
    float left = (width - panelWidth) * 0.5f;
    float top = height * 0.4f - panelHeight * 0.5f;
    float right = left + panelWidth;
    float bottom = top + panelHeight;

    PushRect(vertices, 0.0f, 0.0f, width, height, dim);
    PushRect(vertices, left - edge, top - edge, right + edge, bottom + edge, border);
    PushRect(vertices, left, top, right, bottom, panel);
    PushRect(vertices, left, top, right - edge, top + edge, highlight);
    PushRect(vertices, left, top, left + edge, bottom - edge, highlight);
    PushRect(vertices, left + edge, bottom - edge, right, bottom, shade);
    PushRect(vertices, right - edge, top + edge, right, bottom, shade);

    float fieldLeft = left + padding;
    float fieldRight = right - padding;
    float fieldTop = top + padding * 2.0f + lineHeight;
    float fieldBottom = fieldTop + fieldHeight;
    PushRect(vertices, fieldLeft - edge, fieldTop - edge, fieldRight + edge, fieldBottom + edge, focus);
    PushRect(vertices, fieldLeft, fieldTop, fieldRight, fieldBottom, field);

    if (!sOverlay.hasFont)
        return;

    PushText(vertices, title, 0, left + padding, top + padding + kTextHeight * unit, textScale, titleColour);

    std::wstring shown = text;
    if ((SDL_GetTicksNS() % kCaretPeriod) < kCaretPeriod / 2)
        shown.push_back(L'_');
    float available = fieldRight - fieldLeft - 2.0f * padding * 0.5f;
    size_t begin = 0;
    while (begin < shown.size() && TextWidth(shown, begin, textScale) > available)
        ++begin;
    float baseline = fieldTop + (fieldHeight + kTextHeight * unit * 0.8f) * 0.5f;
    PushText(vertices, shown, begin, fieldLeft + padding * 0.5f, baseline, textScale, textColour);
}
}

void PcKeyboardOverlayDraw(int width, int height)
{
    std::wstring title;
    std::wstring text;
    if (width <= 0 || height <= 0 || !PcInputGetKeyboardText(&title, &text))
        return;

    if (!sOverlay.initialised)
        Initialise();
    if (!sOverlay.available)
        return;

    std::vector<Vertex> vertices;
    BuildOverlay(vertices, title, text, (float)width, (float)height);

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
    Gl::Uniform1i(sOverlay.glyphLocation, 0);
    Gl::ActiveTexture(Gl::TEXTURE0);
    Gl::BindSampler(0, 0);
    Gl::BindTexture(Gl::TEXTURE_2D, sOverlay.atlas);

    Gl::BindVertexArray(sOverlay.vertexArray);
    Gl::BindBuffer(Gl::ARRAY_BUFFER, sOverlay.vertexBuffer);
    Gl::BufferData(Gl::ARRAY_BUFFER, (GLsizeiptr)(vertices.size() * sizeof(Vertex)), vertices.data(),
                   Gl::STREAM_DRAW);
    Gl::DrawArrays(Gl::TRIANGLES, 0, (GLsizei)vertices.size());
    Gl::BindVertexArray(0);
}

void PcKeyboardOverlayShutdown()
{
    if (!sOverlay.initialised)
        return;
    if (sOverlay.atlas != 0)
        Gl::DeleteTextures(1, &sOverlay.atlas);
    if (sOverlay.vertexBuffer != 0)
        Gl::DeleteBuffers(1, &sOverlay.vertexBuffer);
    if (sOverlay.vertexArray != 0)
        Gl::DeleteVertexArrays(1, &sOverlay.vertexArray);
    if (sOverlay.program != 0)
        Gl::DeleteProgram(sOverlay.program);
    sOverlay = OverlayState();
}
