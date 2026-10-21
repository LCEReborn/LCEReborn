#ifndef PLATFORM_PC_PCGPU_HPP
#define PLATFORM_PC_PCGPU_HPP

#include <SDL3/SDL.h>
#include <map>
#include <string>

// SDL GPU is used exclusively with its Vulkan backend. It owns Vulkan synchronization,
// resource retirement, the Wayland/X11 surface and swapchain; the game owns render passes.
class PcGpu
{
public:
    bool Create(SDL_Window* window, bool debug);
    void Destroy();
    SDL_GPUCommandBuffer* Command();
    void EndPass();
    void Submit(bool wait = false);
    // Waits for the next swapchain image before the frame reads its input, so
    // the wait for the display is not added to the age of that input. The image
    // is held on a command buffer of its own; work submitted during the frame
    // leaves it alone, and AcquireSwapchain finishes the frame on it.
    void WaitForFrame(bool vsync);
    bool AcquireSwapchain(bool vsync);
    void Present();
    SDL_GPURenderPass* BeginPass(const SDL_GPUColorTargetInfo* colours, int count,
                                 const SDL_GPUDepthStencilTargetInfo* depth = nullptr);
    void BindPipeline(SDL_GPUGraphicsPipeline* pipeline);
    SDL_GPUTexture* Texture(int width, int height, SDL_GPUTextureFormat format, SDL_GPUTextureUsageFlags usage,
                            int levels = 1);
    void Upload(SDL_GPUTexture* texture, int level, int width, int height, const void* pixels,
                unsigned int bytesPerPixel = 4);
    SDL_GPUBuffer* Buffer(unsigned int size, SDL_GPUBufferUsageFlags usage);
    // Geometry writes prepare new/cycled buffers or disjoint ranges. They are
    // submitted before rendering so streaming vertices does not split passes.
    void Upload(SDL_GPUBuffer* buffer, unsigned int offset, const void* data, unsigned int size, bool cycle = false);
    void Download(SDL_GPUTexture* texture, int width, int height, void* pixels);
    SDL_GPUShader* Shader(int index, bool vertex);
    SDL_GPUGraphicsPipeline* Pipeline(const SDL_GPUGraphicsPipelineCreateInfo& info);
    void Check(bool success, const char* operation);

    SDL_GPUDevice* device = nullptr;
    SDL_GPURenderPass* pass = nullptr;
    SDL_GPUTexture* swapchain = nullptr;
    Uint32 swapWidth = 0;
    Uint32 swapHeight = 0;
    SDL_GPUTextureFormat swapFormat = SDL_GPU_TEXTUREFORMAT_INVALID;
    Uint64 commandSerial = 0;
    Uint64 passSerial = 0;

private:
    void SubmitBufferUploads();
    void SetPresentMode(bool vsync);
    void BeginCommand();
    void SubmittedCommand();
    SDL_GPUTransferBuffer* Stage(const void* data, Uint32 size, Uint32& offset);
    SDL_Window* mWindow = nullptr;
    SDL_GPUCommandBuffer* mCommand = nullptr;
    SDL_GPUCommandBuffer* mFrameCommand = nullptr;
    SDL_GPUCommandBuffer* mBufferCommand = nullptr;
    SDL_GPUCopyPass* mBufferCopy = nullptr;
    SDL_GPUGraphicsPipeline* mBoundPipeline = nullptr;
    bool mVsync = true;
    bool mAcquired = false;
    SDL_GPUTransferBuffer* mUpload = nullptr;
    Uint32 mUploadSize = 0;
    Uint32 mUploadOffset = 0;
    std::map<int, SDL_GPUShader*> mShaders;
    std::map<std::string, SDL_GPUGraphicsPipeline*> mPipelines;
    std::string mPipelineKey;
    std::string mLastPipelineKey;
    SDL_GPUGraphicsPipeline* mLastPipeline = nullptr;
};

PcGpu& PcGraphics();

#endif
