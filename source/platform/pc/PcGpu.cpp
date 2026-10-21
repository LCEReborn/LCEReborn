#include "platform/pc/PcGpu.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "LceSpirv.hpp"

PcGpu& PcGraphics()
{
    static PcGpu gpu;
    return gpu;
}

void PcGpu::Check(bool success, const char* operation)
{
    if (!success)
    {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "%s: %s", operation, SDL_GetError());
        std::abort();
    }
}

bool PcGpu::Create(SDL_Window* window, bool debug)
{
    device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, debug, "vulkan");
    if (!device)
        return false;
    if (!SDL_ClaimWindowForGPUDevice(device, window))
    {
        SDL_DestroyGPUDevice(device);
        device = nullptr;
        return false;
    }
    mWindow = window;
    mVsync = true;
    mAcquired = false;
    swapchain = nullptr;
    swapFormat = SDL_GetGPUSwapchainTextureFormat(device, window);
    SDL_Log("Graphics backend: %s; video driver: %s", SDL_GetGPUDeviceDriver(device), SDL_GetCurrentVideoDriver());
    return true;
}

void PcGpu::Destroy()
{
    if (!device)
        return;
    if (mFrameCommand)
        Present();
    Submit(true);
    for (const auto& pipeline : mPipelines)
        SDL_ReleaseGPUGraphicsPipeline(device, pipeline.second);
    for (const auto& shader : mShaders)
        SDL_ReleaseGPUShader(device, shader.second);
    mPipelines.clear();
    mShaders.clear();
    mLastPipeline = nullptr;
    mLastPipelineKey.clear();
    if (mUpload)
        SDL_ReleaseGPUTransferBuffer(device, mUpload);
    mUpload = nullptr;
    mUploadSize = mUploadOffset = 0;
    SDL_ReleaseWindowFromGPUDevice(device, mWindow);
    SDL_DestroyGPUDevice(device);
    device = nullptr;
    mWindow = nullptr;
}

SDL_GPUCommandBuffer* PcGpu::Command()
{
    if (!mCommand)
    {
        mCommand = SDL_AcquireGPUCommandBuffer(device);
        Check(mCommand != nullptr, "acquire command buffer");
        BeginCommand();
    }
    return mCommand;
}

void PcGpu::BeginCommand()
{
    ++commandSerial;
    const float viewport[4] = {1, 1, 0, 0};
    SDL_PushGPUVertexUniformData(mCommand, 1, viewport, sizeof(viewport));
}

void PcGpu::SubmittedCommand()
{
    mCommand = nullptr;
    // An image acquired early stays on its own command buffer until the frame
    // moves onto it; any other acquired image was presented by this submission.
    if (!mFrameCommand)
    {
        swapchain = nullptr;
        mAcquired = false;
    }
}

void PcGpu::EndPass()
{
    if (pass)
    {
        SDL_EndGPURenderPass(pass);
        pass = nullptr;
        mBoundPipeline = nullptr;
    }
}

void PcGpu::Submit(bool wait)
{
    EndPass();
    SubmitBufferUploads();
    if (mCommand)
    {
        // A submission also presents an acquired swapchain image, including a
        // minimized window (which can return a successful acquire with no image).
        Check(SDL_SubmitGPUCommandBuffer(mCommand), "submit command buffer");
        SubmittedCommand();
    }
    mUploadOffset = 0;
    if (wait)
        Check(SDL_WaitForGPUIdle(device), "wait for GPU");
}

void PcGpu::SubmitBufferUploads()
{
    if (!mBufferCommand)
        return;
    SDL_EndGPUCopyPass(mBufferCopy);
    Check(SDL_SubmitGPUCommandBuffer(mBufferCommand), "submit geometry uploads");
    mBufferCommand = nullptr;
    mBufferCopy = nullptr;
}

void PcGpu::SetPresentMode(bool vsync)
{
    if (mVsync == vsync)
        return;
    Submit();
    SDL_GPUPresentMode mode = SDL_GPU_PRESENTMODE_VSYNC;
    if (!vsync)
    {
        if (SDL_WindowSupportsGPUPresentMode(device, mWindow, SDL_GPU_PRESENTMODE_MAILBOX))
            mode = SDL_GPU_PRESENTMODE_MAILBOX;
        else if (SDL_WindowSupportsGPUPresentMode(device, mWindow, SDL_GPU_PRESENTMODE_IMMEDIATE))
            mode = SDL_GPU_PRESENTMODE_IMMEDIATE;
    }
    Check(SDL_SetGPUSwapchainParameters(device, mWindow, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, mode),
          "set presentation mode");
    mVsync = vsync;
}

void PcGpu::WaitForFrame(bool vsync)
{
    if (!device || mAcquired)
        return;
    SetPresentMode(vsync);
    mFrameCommand = SDL_AcquireGPUCommandBuffer(device);
    Check(mFrameCommand != nullptr, "acquire command buffer");
    Check(SDL_WaitAndAcquireGPUSwapchainTexture(mFrameCommand, mWindow, &swapchain, &swapWidth, &swapHeight),
          "acquire swapchain image");
    mAcquired = true;
}

bool PcGpu::AcquireSwapchain(bool vsync)
{
    if (mFrameCommand)
    {
        // Send the frame drawn so far, then finish it on the image's command buffer.
        Submit();
        mCommand = mFrameCommand;
        mFrameCommand = nullptr;
        BeginCommand();
        return swapchain != nullptr;
    }
    if (mAcquired)
        return swapchain != nullptr;
    SetPresentMode(vsync);
    EndPass();
    Check(SDL_WaitAndAcquireGPUSwapchainTexture(Command(), mWindow, &swapchain, &swapWidth, &swapHeight),
          "acquire swapchain image");
    mAcquired = true;
    return swapchain != nullptr;
}

void PcGpu::Present()
{
    // A frame that drew nothing to the screen still has to give its image back.
    if (mFrameCommand && AcquireSwapchain(mVsync))
    {
        SDL_GPUColorTargetInfo target = {};
        target.texture = swapchain;
        target.load_op = SDL_GPU_LOADOP_CLEAR;
        target.store_op = SDL_GPU_STOREOP_STORE;
        target.clear_color.a = 1.0f;
        BeginPass(&target, 1);
    }
    Submit();
}

SDL_GPURenderPass* PcGpu::BeginPass(const SDL_GPUColorTargetInfo* colours, int count,
                                    const SDL_GPUDepthStencilTargetInfo* depth)
{
    if (!pass)
    {
        pass = SDL_BeginGPURenderPass(Command(), colours, count, depth);
        Check(pass != nullptr, "begin render pass");
        ++passSerial;
        mBoundPipeline = nullptr;
    }
    return pass;
}

void PcGpu::BindPipeline(SDL_GPUGraphicsPipeline* pipeline)
{
    // Rebinding even the same SDL pipeline invalidates its descriptor sets.
    // Keep the existing binding while drawing successive chunks/UI batches.
    if (mBoundPipeline != pipeline)
    {
        SDL_BindGPUGraphicsPipeline(pass, pipeline);
        mBoundPipeline = pipeline;
    }
}

SDL_GPUTexture* PcGpu::Texture(int width, int height, SDL_GPUTextureFormat format, SDL_GPUTextureUsageFlags usage,
                               int levels)
{
    SDL_GPUTextureCreateInfo info = {};
    info.type = SDL_GPU_TEXTURETYPE_2D;
    info.format = format;
    info.usage = usage;
    info.width = width;
    info.height = height;
    info.layer_count_or_depth = 1;
    info.num_levels = levels;
    SDL_GPUTexture* texture = SDL_CreateGPUTexture(device, &info);
    Check(texture != nullptr, "create texture");
    return texture;
}

SDL_GPUBuffer* PcGpu::Buffer(unsigned int size, SDL_GPUBufferUsageFlags usage)
{
    SDL_GPUBufferCreateInfo info = {};
    info.usage = usage;
    info.size = size;
    SDL_GPUBuffer* buffer = SDL_CreateGPUBuffer(device, &info);
    Check(buffer != nullptr, "create buffer");
    return buffer;
}

SDL_GPUTransferBuffer* PcGpu::Stage(const void* data, Uint32 size, Uint32& offset)
{
    if (mUploadSize < size)
    {
        if (mUpload)
            SDL_ReleaseGPUTransferBuffer(device, mUpload);
        mUploadSize = SDL_max(size, 16u * 1024 * 1024);
        SDL_GPUTransferBufferCreateInfo info = {};
        info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        info.size = mUploadSize;
        mUpload = SDL_CreateGPUTransferBuffer(device, &info);
        Check(mUpload != nullptr, "create upload staging buffer");
        mUploadOffset = 0;
    }
    offset = (mUploadOffset + 15u) & ~15u;
    if (offset + size > mUploadSize)
        offset = 0;
    // Cycle only at a wrap or new submission. Other writes occupy disjoint
    // ranges, so previously recorded copies keep their source bytes intact.
    void* mapped = SDL_MapGPUTransferBuffer(device, mUpload, offset == 0);
    Check(mapped != nullptr, "map upload staging buffer");
    std::memcpy(static_cast<Uint8*>(mapped) + offset, data, size);
    SDL_UnmapGPUTransferBuffer(device, mUpload);
    mUploadOffset = offset + size;
    return mUpload;
}

void PcGpu::Upload(SDL_GPUTexture* texture, int level, int width, int height, const void* pixels,
                   unsigned int bytesPerPixel)
{
    if (!pixels)
        return;
    EndPass();
    SDL_GPUTextureTransferInfo source = {};
    source.transfer_buffer = Stage(pixels, width * height * bytesPerPixel, source.offset);
    SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(Command());
    SDL_GPUTextureRegion region = {};
    region.texture = texture;
    region.mip_level = level;
    region.w = width;
    region.h = height;
    region.d = 1;
    // Keep previous mip levels and previous uses of this texture in this frame.
    SDL_UploadToGPUTexture(copy, &source, &region, false);
    SDL_EndGPUCopyPass(copy);
}

void PcGpu::Upload(SDL_GPUBuffer* buffer, unsigned int offset, const void* data, unsigned int size, bool cycle)
{
    if (!size)
        return;
    SDL_GPUTransferBufferLocation source = {};
    source.transfer_buffer = Stage(data, size, source.offset);
    if (!mBufferCommand)
    {
        mBufferCommand = SDL_AcquireGPUCommandBuffer(device);
        Check(mBufferCommand != nullptr, "acquire geometry upload command buffer");
        mBufferCopy = SDL_BeginGPUCopyPass(mBufferCommand);
        Check(mBufferCopy != nullptr, "begin geometry uploads");
    }
    SDL_GPUBufferRegion destination = {buffer, offset, size};
    SDL_UploadToGPUBuffer(mBufferCopy, &source, &destination, cycle);
}

void PcGpu::Download(SDL_GPUTexture* texture, int width, int height, void* pixels)
{
    EndPass();
    SDL_GPUTransferBufferCreateInfo info = {};
    info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
    info.size = width * height * 4;
    SDL_GPUTransferBuffer* transfer = SDL_CreateGPUTransferBuffer(device, &info);
    Check(transfer != nullptr, "create readback transfer");
    SDL_GPUTextureRegion region = {};
    region.texture = texture;
    region.w = width;
    region.h = height;
    region.d = 1;
    SDL_GPUTextureTransferInfo destination = {};
    destination.transfer_buffer = transfer;
    SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(Command());
    SDL_DownloadFromGPUTexture(copy, &region, &destination);
    SDL_EndGPUCopyPass(copy);
    SubmitBufferUploads();
    SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(mCommand);
    Check(fence != nullptr, "submit readback");
    SubmittedCommand();
    mUploadOffset = 0;
    Check(SDL_WaitForGPUFences(device, true, &fence, 1), "wait for readback");
    SDL_ReleaseGPUFence(device, fence);
    void* mapped = SDL_MapGPUTransferBuffer(device, transfer, false);
    Check(mapped != nullptr, "map readback");
    std::memcpy(pixels, mapped, info.size);
    SDL_UnmapGPUTransferBuffer(device, transfer);
    SDL_ReleaseGPUTransferBuffer(device, transfer);
}

SDL_GPUShader* PcGpu::Shader(int index, bool vertex)
{
    int key = index * 2 + (vertex ? 1 : 0);
    auto found = mShaders.find(key);
    if (found != mShaders.end())
        return found->second;
    const LceSpirvShader& code = vertex ? lceVertexShaders[index] : lcePixelShaders[index];
    SDL_GPUShaderCreateInfo info = {};
    info.code = reinterpret_cast<const Uint8*>(code.words);
    info.code_size = code.size;
    info.entrypoint = "main";
    info.format = SDL_GPU_SHADERFORMAT_SPIRV;
    info.stage = vertex ? SDL_GPU_SHADERSTAGE_VERTEX : SDL_GPU_SHADERSTAGE_FRAGMENT;
    info.num_samplers = code.samplers;
    info.num_uniform_buffers = code.uniforms;
    SDL_GPUShader* shader = SDL_CreateGPUShader(device, &info);
    Check(shader != nullptr, "create SPIR-V shader");
    mShaders[key] = shader;
    return shader;
}

SDL_GPUGraphicsPipeline* PcGpu::Pipeline(const SDL_GPUGraphicsPipelineCreateInfo& info)
{
    // SDL structures are zero-initialized by callers. Include pointed-to state
    // in the key, rather than the addresses of temporary descriptions.
    SDL_GPUGraphicsPipelineCreateInfo keyInfo = info;
    keyInfo.vertex_input_state.vertex_buffer_descriptions = nullptr;
    keyInfo.vertex_input_state.vertex_attributes = nullptr;
    keyInfo.target_info.color_target_descriptions = nullptr;
    // Reuse the key storage: this path is hit for every chunk draw, whereas
    // pipeline creation is rare. Consecutive draws usually share one pipeline.
    std::string& key = mPipelineKey;
    key.assign(reinterpret_cast<const char*>(&keyInfo), sizeof(keyInfo));
    if (info.vertex_input_state.num_vertex_buffers)
        key.append(reinterpret_cast<const char*>(info.vertex_input_state.vertex_buffer_descriptions),
                   info.vertex_input_state.num_vertex_buffers * sizeof(SDL_GPUVertexBufferDescription));
    if (info.vertex_input_state.num_vertex_attributes)
        key.append(reinterpret_cast<const char*>(info.vertex_input_state.vertex_attributes),
                   info.vertex_input_state.num_vertex_attributes * sizeof(SDL_GPUVertexAttribute));
    key.append(reinterpret_cast<const char*>(info.target_info.color_target_descriptions),
               info.target_info.num_color_targets * sizeof(SDL_GPUColorTargetDescription));
    if (mLastPipeline && key == mLastPipelineKey)
        return mLastPipeline;
    auto found = mPipelines.find(key);
    if (found != mPipelines.end())
    {
        mLastPipelineKey = key;
        mLastPipeline = found->second;
        return mLastPipeline;
    }
    SDL_GPUGraphicsPipeline* pipeline = SDL_CreateGPUGraphicsPipeline(device, &info);
    Check(pipeline != nullptr, "create graphics pipeline");
    mPipelines[key] = pipeline;
    mLastPipelineKey = key;
    mLastPipeline = pipeline;
    return pipeline;
}
