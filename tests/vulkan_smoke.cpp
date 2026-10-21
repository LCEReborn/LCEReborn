#include "platform/pc/PcGpu.hpp"
#include "platform/pc/RenderShaders.hpp"

#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static void Require(bool condition, const char* message)
{
    if (!condition)
    {
        std::fprintf(stderr, "FAIL: %s (%s)\n", message, SDL_GetError());
        std::exit(1);
    }
}

static void Pixel(const unsigned char* pixels, int x, int y, int r, int g, int b, int a = 255)
{
    const unsigned char* p = pixels + (y * 32 + x) * 4;
    if (std::abs(p[0] - r) > 2 || std::abs(p[1] - g) > 2 || std::abs(p[2] - b) > 2 || std::abs(p[3] - a) > 2)
    {
        std::fprintf(stderr, "FAIL: pixel (%d,%d): expected %d,%d,%d,%d, got %u,%u,%u,%u\n", x, y, r, g, b, a, p[0],
                     p[1], p[2], p[3]);
        std::exit(1);
    }
}

static SDL_GPUGraphicsPipelineCreateInfo QuadPipeline(PcGpu& gpu, int pixelShader,
                                                      SDL_GPUColorTargetDescription* colour)
{
    SDL_GPUGraphicsPipelineCreateInfo info = {};
    info.vertex_shader = gpu.Shader(RenderVertexShader_ScreenQuad, true);
    info.fragment_shader = gpu.Shader(pixelShader, false);
    info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLESTRIP;
    info.rasterizer_state.enable_depth_clip = true;
    info.target_info.color_target_descriptions = colour;
    info.target_info.num_color_targets = 1;
    return info;
}

static void Geometry(PcGpu& gpu, SDL_GPUTexture* output, SDL_GPUSampler* sampler)
{
    struct Vertex
    {
        float position[3];
        float uv[2];
        unsigned char colour[4];
        signed char normal[4];
        short light[2];
    };
    static_assert(sizeof(Vertex) == 32, "game vertex stride");
    SDL_GPUTextureFormat depthFormat = SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT;
    if (!SDL_GPUTextureSupportsFormat(gpu.device, depthFormat, SDL_GPU_TEXTURETYPE_2D,
                                      SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET))
        depthFormat = SDL_GPU_TEXTUREFORMAT_D32_FLOAT_S8_UINT;
    SDL_GPUTexture* depthTexture = gpu.Texture(32, 32, depthFormat, SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET);
    SDL_GPUTexture* white = gpu.Texture(1, 1, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, SDL_GPU_TEXTUREUSAGE_SAMPLER);
    unsigned char whitePixel[4] = {255, 255, 255, 255};
    gpu.Upload(white, 0, 1, 1, whitePixel);
    SDL_GPUBuffer* vertices = gpu.Buffer(8 * sizeof(Vertex), SDL_GPU_BUFFERUSAGE_VERTEX);
    SDL_GPUBuffer* indices = gpu.Buffer(6 * sizeof(unsigned int), SDL_GPU_BUFFERUSAGE_INDEX);
    const unsigned int quadIndices[6] = {0, 1, 2, 0, 2, 3};
    gpu.Upload(indices, 0, quadIndices, sizeof(quadIndices));
    SDL_GPUVertexBufferDescription buffer = {};
    buffer.pitch = sizeof(Vertex);
    SDL_GPUVertexAttribute attributes[5] = {};
    const SDL_GPUVertexElementFormat formats[] = {
        SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM, SDL_GPU_VERTEXELEMENTFORMAT_BYTE4,
        SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, SDL_GPU_VERTEXELEMENTFORMAT_SHORT2};
    const unsigned int offsets[] = {0, 20, 24, 12, 28};
    for (int i = 0; i < 5; ++i)
    {
        attributes[i].location = i;
        attributes[i].format = formats[i];
        attributes[i].offset = offsets[i];
    }
    SDL_GPUColorTargetDescription colour = {};
    colour.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    SDL_GPUGraphicsPipelineCreateInfo info = {};
    info.vertex_shader = gpu.Shader(RenderVertexShader_Standard, true);
    info.fragment_shader = gpu.Shader(RenderPixelShader_Default, false);
    info.vertex_input_state.vertex_buffer_descriptions = &buffer;
    info.vertex_input_state.num_vertex_buffers = 1;
    info.vertex_input_state.vertex_attributes = attributes;
    info.vertex_input_state.num_vertex_attributes = 5;
    info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    info.rasterizer_state.enable_depth_clip = true;
    info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_BACK;
    info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
    info.depth_stencil_state.enable_depth_test = info.depth_stencil_state.enable_depth_write = true;
    info.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS;
    info.depth_stencil_state.enable_stencil_test = true;
    info.depth_stencil_state.write_mask = info.depth_stencil_state.compare_mask = 255;
    info.depth_stencil_state.front_stencil_state.compare_op = SDL_GPU_COMPAREOP_ALWAYS;
    info.depth_stencil_state.front_stencil_state.fail_op = info.depth_stencil_state.front_stencil_state.depth_fail_op =
        SDL_GPU_STENCILOP_KEEP;
    info.depth_stencil_state.front_stencil_state.pass_op = SDL_GPU_STENCILOP_REPLACE;
    info.depth_stencil_state.back_stencil_state = info.depth_stencil_state.front_stencil_state;
    info.target_info.color_target_descriptions = &colour;
    info.target_info.num_color_targets = 1;
    info.target_info.has_depth_stencil_target = true;
    info.target_info.depth_stencil_format = depthFormat;
    SDL_GPUGraphicsPipeline* pipeline = gpu.Pipeline(info);
    float vc[256][4] = {};
    float pc[32][4] = {};
    for (int matrix = 0; matrix < 4; ++matrix)
        for (int axis = 0; axis < 4; ++axis)
            vc[matrix * 4 + axis][axis] = 1;
    for (int component = 0; component < 4; ++component)
        pc[0][component] = pc[4][component] = 1;
    pc[31][0] = 0.2f;
    SDL_GPUColorTargetInfo target = {};
    target.texture = output;
    target.load_op = SDL_GPU_LOADOP_CLEAR;
    target.store_op = SDL_GPU_STOREOP_STORE;
    SDL_GPUDepthStencilTargetInfo depth = {};
    depth.texture = depthTexture;
    depth.clear_depth = 1;
    depth.load_op = depth.stencil_load_op = SDL_GPU_LOADOP_CLEAR;
    depth.store_op = depth.stencil_store_op = SDL_GPU_STOREOP_STORE;
    gpu.BeginPass(&target, 1, &depth);
    gpu.EndPass();
    target.load_op = depth.load_op = depth.stencil_load_op = SDL_GPU_LOADOP_LOAD;
    SDL_GPUTextureSamplerBinding textures[4] = {{white, sampler}, {white, sampler}, {white, sampler}, {white, sampler}};
    auto draw = [&](float z, int r, int g, int b, unsigned int offset = 0, float left = -1, float right = 1,
                    bool cycle = true) {
        Vertex data[4] = {};
        const float positions[4][2] = {{left, 1}, {left, -1}, {right, -1}, {right, 1}};
        for (int i = 0; i < 4; ++i)
        {
            data[i].position[0] = positions[i][0];
            data[i].position[1] = positions[i][1];
            data[i].position[2] = z;
            data[i].uv[0] = data[i].uv[1] = 0.5f;
            data[i].colour[0] = 255;
            data[i].colour[1] = b;
            data[i].colour[2] = g;
            data[i].colour[3] = r;
        }
        SDL_GPURenderPass* activePass = gpu.pass;
        gpu.Upload(vertices, offset, data, sizeof(data), cycle);
        Require(gpu.pass == activePass, "geometry streaming preserves the render pass");
        auto pass = gpu.BeginPass(&target, 1, &depth);
        gpu.BindPipeline(pipeline);
        SDL_SetGPUStencilReference(pass, 7);
        SDL_PushGPUVertexUniformData(gpu.Command(), 0, vc, 18 * 16);
        SDL_PushGPUFragmentUniformData(gpu.Command(), 0, pc, sizeof(pc));
        SDL_BindGPUVertexSamplers(pass, 0, textures, 1);
        SDL_BindGPUFragmentSamplers(pass, 0, textures, 4);
        SDL_GPUBufferBinding vertex = {vertices, offset}, index = {indices, 0};
        SDL_BindGPUVertexBuffers(pass, 0, &vertex, 1);
        SDL_BindGPUIndexBuffer(pass, &index, SDL_GPU_INDEXELEMENTSIZE_32BIT);
        SDL_DrawGPUIndexedPrimitives(pass, 6, 1, 0, 0, 0);
    };
    draw(-0.5f, 255, 0, 0);
    draw(0.5f, 0, 255, 0);
    std::array<unsigned char, 32 * 32 * 4> pixels;
    gpu.Download(output, 32, 32, pixels.data());
    Pixel(pixels.data(), 8, 8, 255, 0, 0);
    pc[0][3] = 0.1f;
    draw(-0.75f, 0, 0, 255);
    gpu.Download(output, 32, 32, pixels.data());
    Pixel(pixels.data(), 8, 8, 255, 0, 0);
    pc[0][3] = 1;
    info.depth_stencil_state.front_stencil_state.compare_op = SDL_GPU_COMPAREOP_EQUAL;
    info.depth_stencil_state.back_stencil_state = info.depth_stencil_state.front_stencil_state;
    pipeline = gpu.Pipeline(info);
    draw(-0.75f, 0, 0, 255);
    gpu.Download(output, 32, 32, pixels.data());
    Pixel(pixels.data(), 8, 8, 0, 0, 255);
    // Stream disjoint ranges into the same backing buffer while the render
    // pass remains open. Both draws must see their own vertices after the
    // preparatory copy command buffer is submitted ahead of rendering.
    draw(-0.9f, 255, 0, 0, 0, -1, 0);
    draw(-0.9f, 0, 255, 0, 4 * sizeof(Vertex), 0, 1, false);
    gpu.Download(output, 32, 32, pixels.data());
    Pixel(pixels.data(), 8, 8, 255, 0, 0);
    Pixel(pixels.data(), 24, 8, 0, 255, 0);
    SDL_ReleaseGPUBuffer(gpu.device, vertices);
    SDL_ReleaseGPUBuffer(gpu.device, indices);
    SDL_ReleaseGPUTexture(gpu.device, white);
    SDL_ReleaseGPUTexture(gpu.device, depthTexture);
}

int main(int argc, char** argv)
{
    const bool presentation = argc == 2 && std::strcmp(argv[1], "--present") == 0;
    Require(SDL_Init(SDL_INIT_VIDEO), "initialize SDL video");
    SDL_Window* window = SDL_CreateWindow(
        "LCE Vulkan test", 320, 240, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY
                                       | (presentation ? 0 : SDL_WINDOW_HIDDEN));
    Require(window != nullptr, "create Vulkan window");
    PcGpu gpu;
    Require(gpu.Create(window, true), "create Vulkan device");
    Require(std::strcmp(SDL_GetGPUDeviceDriver(gpu.device), "vulkan") == 0, "Vulkan selected");
    for (int i = 0; i < RenderVertexShader_Count; ++i)
        gpu.Shader(i, true);
    for (int i = 0; i < RenderPixelShader_Count; ++i)
        gpu.Shader(i, false);

    // The four texels distinguish both axes and channels. A GL-style flip in any
    // upload, fullscreen shader or readback path fails these pixel assertions.
    const unsigned char texels[] = {255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 255, 255};
    SDL_GPUTexture* source = gpu.Texture(2, 2, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, SDL_GPU_TEXTUREUSAGE_SAMPLER, 2);
    gpu.Upload(source, 0, 2, 2, texels);
    gpu.Upload(source, 1, 1, 1, texels);
    SDL_GPUSamplerCreateInfo samplerInfo = {};
    samplerInfo.address_mode_u = samplerInfo.address_mode_v = samplerInfo.address_mode_w =
        SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    SDL_GPUSampler* sampler = SDL_CreateGPUSampler(gpu.device, &samplerInfo);
    Require(sampler != nullptr, "create sampler");
    SDL_GPUTexture* output = gpu.Texture(32, 32, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
                                         SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER);
    SDL_GPUColorTargetDescription colour = {};
    colour.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    SDL_GPUColorTargetInfo target = {};
    target.texture = output;
    target.load_op = SDL_GPU_LOADOP_CLEAR;
    target.store_op = SDL_GPU_STOREOP_STORE;
    SDL_GPUTextureSamplerBinding binding = {source, sampler};
    float quad[8] = {0, 0, 1, 1, 1, 0, 0, 0};
    auto info = QuadPipeline(gpu, RenderPixelShader_Copy, &colour);
    SDL_GPUGraphicsPipeline* copyPipeline = gpu.Pipeline(info);
    Require(copyPipeline == gpu.Pipeline(info), "pipeline cache reuses immutable state");
    auto pass = gpu.BeginPass(&target, 1);
    SDL_BindGPUGraphicsPipeline(pass, copyPipeline);
    SDL_PushGPUVertexUniformData(gpu.Command(), 0, quad, sizeof(quad));
    SDL_BindGPUFragmentSamplers(pass, 0, &binding, 1);
    SDL_DrawGPUPrimitives(pass, 4, 1, 0, 0);
    std::array<unsigned char, 32 * 32 * 4> pixels;
    gpu.Download(output, 32, 32, pixels.data());
    Pixel(pixels.data(), 4, 4, 255, 0, 0);
    Pixel(pixels.data(), 28, 4, 0, 255, 0);
    Pixel(pixels.data(), 4, 28, 0, 0, 255);
    Pixel(pixels.data(), 28, 28, 255, 255, 255);

    // A background viewport twice the target width crops to the middle half
    // of the image. SDL's viewport remains inside the actual attachment.
    SDL_GPUTexture* strip = gpu.Texture(4, 1, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, SDL_GPU_TEXTUREUSAGE_SAMPLER);
    gpu.Upload(strip, 0, 4, 1, texels);
    pass = gpu.BeginPass(&target, 1);
    SDL_BindGPUGraphicsPipeline(pass, copyPipeline);
    SDL_PushGPUVertexUniformData(gpu.Command(), 0, quad, sizeof(quad));
    const float crop[4] = {2, 1, 0, 0};
    SDL_PushGPUVertexUniformData(gpu.Command(), RENDER_VIEWPORT_CONSTANT_BINDING, crop, sizeof(crop));
    SDL_GPUTextureSamplerBinding cropped = {strip, sampler};
    SDL_BindGPUFragmentSamplers(pass, 0, &cropped, 1);
    SDL_DrawGPUPrimitives(pass, 4, 1, 0, 0);
    gpu.Download(output, 32, 32, pixels.data());
    Pixel(pixels.data(), 4, 4, 0, 255, 0);
    Pixel(pixels.data(), 28, 4, 0, 0, 255);
    SDL_ReleaseGPUTexture(gpu.device, strip);

    // Texture updates must retain previously submitted uses and mip contents.
    unsigned char grey[] = {64, 64, 64, 255, 64, 64, 64, 255, 64, 64, 64, 255, 64, 64, 64, 255};
    gpu.Upload(source, 0, 2, 2, grey);
    info = QuadPipeline(gpu, RenderPixelShader_Gamma, &colour);
    auto gammaPipeline = gpu.Pipeline(info);
    pass = gpu.BeginPass(&target, 1);
    gpu.BindPipeline(gammaPipeline);
    SDL_PushGPUVertexUniformData(gpu.Command(), 0, quad, sizeof(quad));
    float gamma[4] = {0.5f, 0, 0, 0};
    SDL_PushGPUFragmentUniformData(gpu.Command(), 0, gamma, sizeof(gamma));
    SDL_BindGPUFragmentSamplers(pass, 0, &binding, 1);
    SDL_Rect scissor = {0, 0, 16, 32};
    SDL_SetGPUScissor(pass, &scissor);
    SDL_DrawGPUPrimitives(pass, 4, 1, 0, 0);
    gpu.Download(output, 32, 32, pixels.data());
    Pixel(pixels.data(), 4, 4, 128, 128, 128);
    Pixel(pixels.data(), 28, 4, 0, 0, 0, 0);

    // Weighted transparency uses three independently masked targets. Check the
    // real accumulation/resolve shaders, including preservation of main RGB.
    SDL_GPUTexture* accumulate = gpu.Texture(32, 32, SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT,
                                             SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER);
    SDL_GPUTexture* weight = gpu.Texture(32, 32, SDL_GPU_TEXTUREFORMAT_R16_FLOAT,
                                         SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER);
    SDL_GPUColorTargetInfo oit[3] = {};
    oit[0].texture = accumulate;
    oit[1].texture = output;
    oit[2].texture = weight;
    for (auto& attachment : oit)
    {
        attachment.load_op = SDL_GPU_LOADOP_CLEAR;
        attachment.store_op = SDL_GPU_STOREOP_STORE;
    }
    oit[0].clear_color = {0.5f, 0, 0, 0};
    oit[1].clear_color = {0, 0, 1, 0.5f};
    oit[2].clear_color = {1, 0, 0, 0};
    gpu.BeginPass(oit, 3);
    gpu.EndPass();
    colour.blend_state.enable_blend = true;
    colour.blend_state.color_blend_op = colour.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
    colour.blend_state.src_color_blendfactor = colour.blend_state.src_alpha_blendfactor =
        SDL_GPU_BLENDFACTOR_ONE_MINUS_DST_ALPHA;
    colour.blend_state.dst_color_blendfactor = colour.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_DST_ALPHA;
    info = QuadPipeline(gpu, RenderPixelShader_AlphaResolve, &colour);
    auto resolvePipeline = gpu.Pipeline(info);
    target.load_op = SDL_GPU_LOADOP_LOAD;
    pass = gpu.BeginPass(&target, 1);
    gpu.BindPipeline(resolvePipeline);
    SDL_PushGPUVertexUniformData(gpu.Command(), 0, quad, sizeof(quad));
    SDL_GPUTextureSamplerBinding resolveBindings[] = {{accumulate, sampler}, {weight, sampler}};
    SDL_BindGPUFragmentSamplers(pass, 0, resolveBindings, 2);
    SDL_DrawGPUPrimitives(pass, 4, 1, 0, 0);
    gpu.Download(output, 32, 32, pixels.data());
    Pixel(pixels.data(), 4, 4, 64, 0, 128, 191);

    Geometry(gpu, output, sampler);

    // Exercise native surface presentation, resize and VSync changes. An SDL
    // compositor may temporarily return no image during a resize; that is valid.
    // From frame 4 the image is taken at the start of the frame, as the game
    // does, and kept through a submission partway through the frame; frame 36
    // draws nothing to it.
    int presented = 0;
    for (int frame = 0; presentation && frame < 40; ++frame)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
        }
        if (frame == 8)
            Require(SDL_SetWindowSize(window, 480, 270), "resize window");
        if (frame == 16)
            Require(SDL_SetWindowSize(window, 256, 256), "resize window again");
        if (frame == 24)
        {
            Require(SDL_SetWindowFullscreen(window, true), "request fullscreen");
            Require(SDL_SyncWindow(window), "wait for fullscreen configure");
            Require((SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0, "fullscreen acknowledged");
        }
        if (frame == 28)
        {
            Require(SDL_SetWindowRelativeMouseMode(window, true), "enable native relative mouse mode");
            Require(SDL_GetWindowRelativeMouseMode(window), "relative mouse mode enabled");
        }
        if (frame == 32)
        {
            Require(SDL_SetWindowRelativeMouseMode(window, false), "release relative mouse mode");
            Require(!SDL_GetWindowRelativeMouseMode(window), "relative mouse mode released");
            Require(SDL_SetWindowFullscreen(window, false), "leave fullscreen");
            Require(SDL_SyncWindow(window), "wait for windowed configure");
            Require((SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) == 0, "windowed state acknowledged");
        }
        if (frame >= 4)
        {
            gpu.WaitForFrame(frame < 12);
            SDL_GPUColorTargetInfo offscreen = {};
            offscreen.texture = output;
            offscreen.load_op = SDL_GPU_LOADOP_CLEAR;
            offscreen.store_op = SDL_GPU_STOREOP_STORE;
            gpu.BeginPass(&offscreen, 1);
            gpu.Submit();
        }
        if (frame != 36 && gpu.AcquireSwapchain(frame < 12))
        {
            ++presented;
            int width = 0, height = 0;
            Require(SDL_GetWindowSizeInPixels(window, &width, &height), "query high-DPI surface pixels");
            Require(gpu.swapWidth == static_cast<unsigned int>(width)
                        && gpu.swapHeight == static_cast<unsigned int>(height),
                    "swapchain tracks configured pixel size");
            SDL_GPUColorTargetInfo swap = {};
            swap.texture = gpu.swapchain;
            swap.load_op = SDL_GPU_LOADOP_CLEAR;
            swap.store_op = SDL_GPU_STOREOP_STORE;
            swap.clear_color = {0, 0, 0, 1};
            SDL_GPUColorTargetDescription swapColour = {};
            swapColour.format = gpu.swapFormat;
            info = QuadPipeline(gpu, RenderPixelShader_Copy, &swapColour);
            pass = gpu.BeginPass(&swap, 1);
            gpu.BindPipeline(gpu.Pipeline(info));
            SDL_GPUTextureSamplerBinding image = {output, sampler};
            SDL_PushGPUVertexUniformData(gpu.Command(), 0, quad, sizeof(quad));
            SDL_BindGPUFragmentSamplers(pass, 0, &image, 1);
            SDL_DrawGPUPrimitives(pass, 4, 1, 0, 0);
        }
        gpu.Present();
        SDL_Delay(10);
    }
    Require(!presentation || presented > 0, "present at least one swapchain image");
    gpu.Submit(true);
    SDL_ReleaseGPUTexture(gpu.device, source);
    SDL_ReleaseGPUTexture(gpu.device, output);
    SDL_ReleaseGPUTexture(gpu.device, accumulate);
    SDL_ReleaseGPUTexture(gpu.device, weight);
    SDL_ReleaseGPUSampler(gpu.device, sampler);
    gpu.Destroy();
    SDL_DestroyWindow(window);
    SDL_Quit();
    std::puts(
        "PASS: SPIR-V shaders, texture/mip uploads, orientation, gamma, scissor, transparency resolve, indexed geometry, culling, depth, stencil, alpha testing and readback");
    if (presentation)
        std::puts("PASS: high-DPI resize, fullscreen, relative mouse capture, VSync changes and Vulkan presentation");
    return 0;
}
