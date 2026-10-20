#include "platform/pc/PcKeyboardOverlay.hpp"

#include <SDL3/SDL.h>

#include <stddef.h>
#include <stdio.h>

#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "platform/pc/PcGpu.hpp"
#include "platform/pc/RenderShaders.hpp"
#include "platform/pc/Log.hpp"
#include "platform/pc/PcInputEvents.hpp"
#include "platform/pc/PcPaths.hpp"
#include "vendor/stb_truetype.hpp"

namespace
{
const Logger sLog("KeyboardOverlay");
const int kAtlasSize = 512;
const int kFirstGlyph = 32;
const int kGlyphCount = 224;
const float kBakeHeight = 32.0f;
const float kReferenceHeight = 720.0f;
const float kTextHeight = 22.0f;
const unsigned long long kCaretPeriod = 1000000000ull;

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
    SDL_GPUBuffer* vertexBuffer;
    unsigned int bufferSize;
    SDL_GPUTexture* atlas;
    SDL_GPUSampler* sampler;
    stbtt_bakedchar glyphs[kGlyphCount];
};

OverlayState sOverlay;

bool LoadFont()
{
    std::string path = PcContentRoot() + "/Common/Media/font/Mojangles.ttf";
    std::ifstream file(path.c_str(), std::ios::binary);
    if (!file)
    {
        LOG_ERROR(sLog, "Keyboard overlay font missing: %s", path.c_str());
        return false;
    }

    std::vector<unsigned char> fontData((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    std::vector<unsigned char> bitmap(kAtlasSize * kAtlasSize);
    if (stbtt_BakeFontBitmap(fontData.data(), 0, kBakeHeight, bitmap.data(), kAtlasSize, kAtlasSize, kFirstGlyph,
                             kGlyphCount, sOverlay.glyphs)
        <= 0)
    {
        LOG_ERROR(sLog, "Keyboard overlay font atlas too small for %s", path.c_str());
        return false;
    }

    sOverlay.atlas = PcGraphics().Texture(kAtlasSize, kAtlasSize, SDL_GPU_TEXTUREFORMAT_R8_UNORM,
                                          SDL_GPU_TEXTUREUSAGE_SAMPLER);
    PcGraphics().Upload(sOverlay.atlas, 0, kAtlasSize, kAtlasSize, bitmap.data(), 1);
    return true;
}

void Initialise()
{
    sOverlay.initialised = true;
    SDL_GPUSamplerCreateInfo info = {};
    info.address_mode_u = info.address_mode_v = info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sOverlay.sampler = SDL_CreateGPUSampler(PcGraphics().device, &info);
    PcGraphics().Check(sOverlay.sampler != NULL, "create keyboard sampler");
    sOverlay.hasFont = LoadFont();
    if (!sOverlay.hasFont)
    {
        unsigned char white = 255;
        sOverlay.atlas = PcGraphics().Texture(1, 1, SDL_GPU_TEXTUREFORMAT_R8_UNORM, SDL_GPU_TEXTUREUSAGE_SAMPLER);
        PcGraphics().Upload(sOverlay.atlas, 0, 1, 1, &white, 1);
    }
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

float TextWidth(const std::wstring& text, size_t begin, float scale, size_t end = std::wstring::npos)
{
    float width = 0.0f;
    for (size_t index = begin; index < text.size() && index < end; ++index)
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

void SetTextInputArea(float left, float top, float right, float bottom, float caret, float width, float height)
{
    SDL_Window* window = SDL_GetKeyboardFocus();
    if (!window)
        return;
    int logicalWidth = 0, logicalHeight = 0;
    SDL_GetWindowSize(window, &logicalWidth, &logicalHeight);
    // Wayland IME rectangles use surface coordinates, while this overlay is
    // rendered in buffer pixels. These differ on fractional/high-DPI displays.
    float scaleX = logicalWidth / width, scaleY = logicalHeight / height;
    SDL_Rect area = {static_cast<int>(left * scaleX), static_cast<int>(top * scaleY),
                     SDL_max(1, static_cast<int>(SDL_ceilf((right - left) * scaleX))),
                     SDL_max(1, static_cast<int>(SDL_ceilf((bottom - top) * scaleY)))};
    int offset = SDL_clamp(static_cast<int>((caret - left) * scaleX), 0, area.w);
    SDL_Rect previous;
    int previousOffset = 0;
    if (!SDL_GetTextInputArea(window, &previous, &previousOffset) || previous.x != area.x || previous.y != area.y
        || previous.w != area.w || previous.h != area.h || previousOffset != offset)
        SDL_SetTextInputArea(window, &area, offset);
}

void BuildOverlay(std::vector<Vertex>& vertices, const PcKeyboardText& text, float width,
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
    {
        SetTextInputArea(fieldLeft, fieldTop, fieldRight, fieldBottom, fieldLeft, width, height);
        return;
    }

    PushText(vertices, text.title, 0, left + padding, top + padding + kTextHeight * unit, textScale, titleColour);

    const std::wstring shown = text.text + text.composition.text;
    size_t caretIndex = text.composition.text.empty() ? shown.size()
                                                     : text.text.size() + text.composition.cursor;
    float available = fieldRight - fieldLeft - padding - edge;
    size_t begin = 0;
    while (begin < shown.size() && TextWidth(shown, begin, textScale) > available)
        ++begin;
    // Keep a caret in the middle of a long preedit visible, clipping the tail
    // instead of scrolling the caret outside the field.
    begin = SDL_min(begin, caretIndex);
    size_t visibleEnd = shown.size();
    while (visibleEnd > caretIndex && TextWidth(shown, begin, textScale, visibleEnd) > available)
        --visibleEnd;
    float baseline = fieldTop + (fieldHeight + kTextHeight * unit * 0.8f) * 0.5f;
    float textLeft = fieldLeft + padding * 0.5f;
    auto position = [&](size_t index) {
        return textLeft + TextWidth(shown, begin, textScale, SDL_clamp(index, begin, visibleEnd));
    };
    float caret = position(caretIndex);
    if (!text.composition.text.empty())
    {
        float preedit = position(text.text.size());
        float end = position(visibleEnd);
        PushRect(vertices, preedit, baseline + edge, end, baseline + 2 * edge, focus);
        float selectedEnd = position(caretIndex + text.composition.selection);
        if (text.composition.selection != 0)
            PushRect(vertices, caret, fieldTop + edge, selectedEnd, fieldBottom - edge, shade);
    }
    PushText(vertices, shown.substr(begin, visibleEnd - begin), 0, textLeft, baseline, textScale, textColour);
    if (!text.composition.text.empty() || (SDL_GetTicksNS() % kCaretPeriod) < kCaretPeriod / 2)
        PushRect(vertices, caret, baseline - kTextHeight * unit, caret + edge, baseline + edge, textColour);
    SetTextInputArea(fieldLeft, fieldTop, fieldRight, fieldBottom, caret, width, height);
}
}

void PcKeyboardOverlayDraw(int width, int height)
{
    PcKeyboardText text;
    if (width <= 0 || height <= 0 || !PcGraphics().swapchain || !PcInputGetKeyboardText(&text))
        return;

    if (!sOverlay.initialised)
        Initialise();
    if (!sOverlay.available)
        return;

    std::vector<Vertex> vertices;
    BuildOverlay(vertices, text, (float)width, (float)height);

    PcGpu& gpu = PcGraphics();
    if (!gpu.swapchain || vertices.empty())
        return;
    const unsigned int size = vertices.size() * sizeof(Vertex);
    if (sOverlay.bufferSize < size)
    {
        gpu.EndPass();
        if (sOverlay.vertexBuffer)
            SDL_ReleaseGPUBuffer(gpu.device, sOverlay.vertexBuffer);
        sOverlay.bufferSize = size * 2;
        sOverlay.vertexBuffer = gpu.Buffer(sOverlay.bufferSize, SDL_GPU_BUFFERUSAGE_VERTEX);
    }
    gpu.Upload(sOverlay.vertexBuffer, 0, vertices.data(), size, true);
    SDL_GPUGraphicsPipelineCreateInfo info = {};
    info.vertex_shader = gpu.Shader(RenderVertexShader_Keyboard, true);
    info.fragment_shader = gpu.Shader(RenderPixelShader_Keyboard, false);
    info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    info.rasterizer_state.enable_depth_clip = true;
    SDL_GPUVertexBufferDescription buffer = {};
    buffer.pitch = sizeof(Vertex);
    SDL_GPUVertexAttribute attributes[3] = {};
    attributes[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
    attributes[0].offset = offsetof(Vertex, x);
    attributes[1].location = 1;
    attributes[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
    attributes[1].offset = offsetof(Vertex, u);
    attributes[2].location = 2;
    attributes[2].format = SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM;
    attributes[2].offset = offsetof(Vertex, colour);
    info.vertex_input_state.vertex_buffer_descriptions = &buffer;
    info.vertex_input_state.num_vertex_buffers = 1;
    info.vertex_input_state.vertex_attributes = attributes;
    info.vertex_input_state.num_vertex_attributes = 3;
    SDL_GPUColorTargetDescription colour = {};
    colour.format = gpu.swapFormat;
    colour.blend_state.enable_blend = true;
    colour.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
    colour.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    colour.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
    colour.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    colour.blend_state.color_blend_op = colour.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
    info.target_info.color_target_descriptions = &colour;
    info.target_info.num_color_targets = 1;
    SDL_GPUColorTargetInfo target = {};
    target.texture = gpu.swapchain;
    target.load_op = SDL_GPU_LOADOP_LOAD;
    target.store_op = SDL_GPU_STOREOP_STORE;
    SDL_GPUGraphicsPipeline* pipeline = gpu.Pipeline(info);
    SDL_GPURenderPass* pass = gpu.BeginPass(&target, 1);
    gpu.BindPipeline(pipeline);
    float screen[4] = {(float)width, (float)height, 0, 0};
    SDL_PushGPUVertexUniformData(gpu.Command(), 0, screen, sizeof(screen));
    SDL_GPUTextureSamplerBinding atlas = {sOverlay.atlas, sOverlay.sampler};
    SDL_BindGPUFragmentSamplers(pass, 0, &atlas, 1);
    SDL_GPUBufferBinding binding = {sOverlay.vertexBuffer, 0};
    SDL_BindGPUVertexBuffers(pass, 0, &binding, 1);
    SDL_DrawGPUPrimitives(pass, vertices.size(), 1, 0, 0);
}

void PcKeyboardOverlayShutdown()
{
    if (!sOverlay.initialised)
        return;
    PcGraphics().EndPass();
    if (sOverlay.atlas)
        SDL_ReleaseGPUTexture(PcGraphics().device, sOverlay.atlas);
    if (sOverlay.vertexBuffer)
        SDL_ReleaseGPUBuffer(PcGraphics().device, sOverlay.vertexBuffer);
    if (sOverlay.sampler)
        SDL_ReleaseGPUSampler(PcGraphics().device, sOverlay.sampler);
    sOverlay = OverlayState();
}
