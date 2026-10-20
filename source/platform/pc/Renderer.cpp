#include "game/client/render/Renderer.hpp"

#include <math.h>
#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <map>
#include <string>
#include <thread>
#include <vector>

#include "game/core/VideoSettings.hpp"
#include "platform/FileApi.hpp"
#include "platform/pc/PcGpu.hpp"
#include <algorithm>
#include "platform/pc/Log.hpp"
#include "platform/pc/PcCrashReport.hpp"
#include "platform/pc/PcKeyboardOverlay.hpp"
#include "platform/pc/PcWindow.hpp"
#include "platform/pc/RenderShaders.hpp"

struct D3DXIMAGE_INFO
{
    unsigned int Width;
    unsigned int Height;
    unsigned int Depth;
    unsigned int MipLevels;
    unsigned int Format;
    unsigned int ResourceType;
    unsigned int ImageFileFormat;
};

struct RenderSamplerState
{
    unsigned char mClampU;
    unsigned char mClampV;
    unsigned char mMagLinear;
    unsigned char mMinLinear;
    unsigned char mMipFilter;
};

namespace
{
const Logger sLog("Renderer");
std::string sGpuName;
const int TEXTURE_COUNT = 512;
const int TEXTURE_STAGE_COUNT = 4;
const int MAX_CHUNK_GRID_WIDTH = 2 * VideoSettings::RENDER_DISTANCE_MAX;
const int COMMAND_BUFFER_COUNT = MAX_CHUNK_GRID_WIDTH * MAX_CHUNK_GRID_WIDTH * 16 * 3 + 16000;
const int COMMAND_BUFFER_ID_COUNT = 0x800000;
const unsigned int COMMAND_BUFFER_INITIAL_CAPACITY = 0x1000;
const unsigned int DYNAMIC_VERTEX_BUDGET = 0x200000;
const int BONE_COUNT = 48;
const int VERTEX_CONSTANT_COUNT = 256;
const unsigned int STREAM_VERTEX_BUFFER_SIZE = 32 * 1024 * 1024;
const int UI_WIDTH = 1280;
const int UI_HEIGHT = 720;
const int DRC_WIDTH = 854;
const int DRC_HEIGHT = 480;
const int THUMBNAIL_LEVELS = 4;
const int THUMBNAIL_SIZE = 0x200;
const int THUMBNAIL_OUTPUT_SIZE = 0x40;
const unsigned short DEFAULT_GAMMA = 0x2AAA;

const unsigned int VERTEX_REG_WORLD_VIEW = 0x00;
const unsigned int VERTEX_REG_WORLD_VIEW2 = 0x10;
const unsigned int VERTEX_REG_PROJECTION = 0x20;
const unsigned int VERTEX_REG_TEXTURE = 0x30;
const unsigned int VERTEX_REG_VERTEX_TEXTURE_UV = 0x40;
const unsigned int VERTEX_REG_FOG = 0x44;
const unsigned int VERTEX_REG_LIGHTING = 0x48;
const unsigned int VERTEX_REG_TEXGEN_EYE = 0x5C;
const unsigned int VERTEX_REG_TEXGEN_OBJECT = 0x6C;
const unsigned int VERTEX_REG_UI_TEXTURE_SCALE = 0x84;
const unsigned int VERTEX_REG_SCALING_GRID_X = 0x88;
const unsigned int VERTEX_REG_SCALING_GRID_Y = 0x8C;
const unsigned int VERTEX_REG_SCALING_GRID_SCALE = 0x90;
const unsigned int VERTEX_REG_VERTEX_ALPHA = 0xA0;
const unsigned int VERTEX_REG_BONES = 0xA4;

const unsigned int PIXEL_REG_COLOURS = 0x00;
const unsigned int PIXEL_REG_FORCE_LOD = 0x20;
const unsigned int PIXEL_REG_LAYERED = 0x24;

const int COMPARE_LEQUAL = 3;
const int COMPARE_ALWAYS = 7;
const int BLEND_ONE = 1;

const unsigned int TEXTURE_FLAG_CLAMP_U = 1;
const unsigned int TEXTURE_FLAG_CLAMP_V = 2;
const unsigned int TEXTURE_FLAG_MIN_LINEAR = 4;
const unsigned int TEXTURE_FLAG_MAG_LINEAR = 8;

const unsigned char MIP_FILTER_NONE = 0;
const unsigned char MIP_FILTER_POINT = 1;
const unsigned char MIP_FILTER_LINEAR = 2;

const unsigned char CHANNEL_MASK_RGBA = 0xF;

const unsigned int VERTEX_STRIDES[C4JRender::eVertexType_Count] = {0x20, 0x10, 0x20, 0x20, 8, 8, 8, 0x14, 0x20, 0x20};

enum VertexLayout
{
    VertexLayout_Standard,
    VertexLayout_Terrain,
    VertexLayout_Position,
    VertexLayout_ColouredUI,
    VertexLayout_Count
};

enum RenderTarget
{
    RenderTarget_Main,
    RenderTarget_OrderIndependentAlpha
};

int LayoutForVertexType(int vertexType)
{
    switch (vertexType)
    {
    case C4JRender::eVertexType_Terrain:
        return VertexLayout_Terrain;
    case C4JRender::eVertexType_UI:
    case C4JRender::eVertexType_ScalingGridUI:
    case C4JRender::eVertexType_TiledUI:
        return VertexLayout_Position;
    case C4JRender::eVertexType_ColouredUI:
        return VertexLayout_ColouredUI;
    default:
        return VertexLayout_Standard;
    }
}

SDL_GPUCompareOp CompareFunction(int function)
{
    return (SDL_GPUCompareOp)(SDL_GPU_COMPAREOP_NEVER + (function & 7));
}

SDL_GPUBlendFactor BlendFunction(int function)
{
    static const SDL_GPUBlendFactor factors[] = {SDL_GPU_BLENDFACTOR_ZERO,
                                                 SDL_GPU_BLENDFACTOR_ONE,
                                                 SDL_GPU_BLENDFACTOR_SRC_COLOR,
                                                 SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_COLOR,
                                                 SDL_GPU_BLENDFACTOR_SRC_ALPHA,
                                                 SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                                                 SDL_GPU_BLENDFACTOR_DST_ALPHA,
                                                 SDL_GPU_BLENDFACTOR_ONE_MINUS_DST_ALPHA,
                                                 SDL_GPU_BLENDFACTOR_DST_COLOR,
                                                 SDL_GPU_BLENDFACTOR_ONE_MINUS_DST_COLOR,
                                                 SDL_GPU_BLENDFACTOR_SRC_ALPHA_SATURATE,
                                                 SDL_GPU_BLENDFACTOR_SRC_ALPHA,
                                                 SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                                                 SDL_GPU_BLENDFACTOR_CONSTANT_COLOR,
                                                 SDL_GPU_BLENDFACTOR_ONE_MINUS_CONSTANT_COLOR};
    return function >= 0 && function < 15 ? factors[function] : SDL_GPU_BLENDFACTOR_ONE;
}

SDL_GPUPrimitiveType PrimitiveMode(int primitiveType)
{
    switch (primitiveType)
    {
    case C4JRender::ePrimitiveType_TriangleStrip:
        return SDL_GPU_PRIMITIVETYPE_TRIANGLESTRIP;
    case C4JRender::ePrimitiveType_Lines:
        return SDL_GPU_PRIMITIVETYPE_LINELIST;
    case C4JRender::ePrimitiveType_LineStrip:
        return SDL_GPU_PRIMITIVETYPE_LINESTRIP;
    default:
        return SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    }
}

void FatalRendererError(const char* message)
{
    LOG_FATAL(sLog, "%s", message);
    PcCrashReport::Write(message);
    Log::Stop();
    abort();
}

void MatrixIdentity(float* matrix)
{
    memset(matrix, 0, sizeof(float) * 16);
    matrix[0] = 1.0f;
    matrix[5] = 1.0f;
    matrix[10] = 1.0f;
    matrix[15] = 1.0f;
}

void MatrixIdentity34(float* matrix)
{
    memset(matrix, 0, sizeof(float) * 12);
    matrix[0] = 1.0f;
    matrix[5] = 1.0f;
    matrix[10] = 1.0f;
}

void MatrixConcat(const float* a, const float* b, float* ab)
{
    float result[16];

    for (int row = 0; row < 4; ++row)
    {
        for (int column = 0; column < 4; ++column)
        {
            result[row * 4 + column] = a[row * 4 + 0] * b[0 * 4 + column] + a[row * 4 + 1] * b[1 * 4 + column]
                                       + a[row * 4 + 2] * b[2 * 4 + column] + a[row * 4 + 3] * b[3 * 4 + column];
        }
    }

    memcpy(ab, result, sizeof(result));
}

void MatrixTranspose(const float* source, float* destination)
{
    float result[16];

    for (int row = 0; row < 4; ++row)
    {
        for (int column = 0; column < 4; ++column)
        {
            result[column * 4 + row] = source[row * 4 + column];
        }
    }

    memcpy(destination, result, sizeof(result));
}

bool MatrixInverse(const float* source, float* inverse)
{
    float work[16];
    memcpy(work, source, sizeof(work));
    MatrixIdentity(inverse);

    for (int pivot = 0; pivot < 4; ++pivot)
    {
        float largest = 0.0f;
        int swap = pivot;

        for (int row = pivot; row < 4; ++row)
        {
            float magnitude = fabsf(work[row * 4 + pivot]);
            if (magnitude > largest)
            {
                largest = magnitude;
                swap = row;
            }
        }

        if (largest == 0.0f)
        {
            return false;
        }

        if (swap != pivot)
        {
            for (int column = 0; column < 4; ++column)
            {
                float swapped = work[pivot * 4 + column];
                work[pivot * 4 + column] = work[swap * 4 + column];
                work[swap * 4 + column] = swapped;
                swapped = inverse[pivot * 4 + column];
                inverse[pivot * 4 + column] = inverse[swap * 4 + column];
                inverse[swap * 4 + column] = swapped;
            }
        }

        float scale = 1.0f / work[pivot * 4 + pivot];
        for (int column = 0; column < 4; ++column)
        {
            work[pivot * 4 + column] *= scale;
            inverse[pivot * 4 + column] *= scale;
        }

        for (int row = 0; row < 4; ++row)
        {
            if (row != pivot)
            {
                float factor = work[row * 4 + pivot];
                for (int column = 0; column < 4; ++column)
                {
                    work[row * 4 + column] -= work[pivot * 4 + column] * factor;
                    inverse[row * 4 + column] -= inverse[pivot * 4 + column] * factor;
                }
            }
        }
    }

    return true;
}

void NormalizeVector(float* vector)
{
    float magnitude = vector[0] * vector[0] + vector[1] * vector[1] + vector[2] * vector[2];
    float scale = 1.0f / sqrtf(magnitude);
    vector[0] *= scale;
    vector[1] *= scale;
    vector[2] *= scale;
}

int ClampTextureLevels(int levels, int width, int height)
{
    int largest = width > height ? width : height;
    int maximum = 1;

    while (largest > 1)
    {
        largest >>= 1;
        ++maximum;
    }

    if (levels < 1)
    {
        levels = 1;
    }

    return levels > maximum ? maximum : levels;
}

void* AlignedAllocate(unsigned int size, unsigned int alignment)
{
    if (alignment < sizeof(void*))
    {
        alignment = sizeof(void*);
    }

    unsigned char* block = static_cast<unsigned char*>(malloc(size + alignment + sizeof(void*)));
    if (block == NULL)
    {
        return NULL;
    }

    uintptr_t start = reinterpret_cast<uintptr_t>(block + sizeof(void*));
    uintptr_t aligned = (start + alignment - 1) & ~static_cast<uintptr_t>(alignment - 1);
    reinterpret_cast<void**>(aligned)[-1] = block;
    return reinterpret_cast<void*>(aligned);
}

void AlignedFree(void* memory)
{
    if (memory != NULL)
    {
        free(reinterpret_cast<void**>(memory)[-1]);
    }
}

RenderSamplerState MakeSampler(bool bClamp, bool bLinear, unsigned char mipFilter)
{
    RenderSamplerState sampler;
    sampler.mClampU = bClamp ? 1 : 0;
    sampler.mClampV = bClamp ? 1 : 0;
    sampler.mMagLinear = bLinear ? 1 : 0;
    sampler.mMinLinear = bLinear ? 1 : 0;
    sampler.mMipFilter = mipFilter;
    return sampler;
}

struct PngMemoryWriter
{
    unsigned char* mStart;
    unsigned char* mCurrent;
    unsigned char* mEnd;
};

void PngWriteToMemory(png_structp png, png_bytep data, png_size_t size)
{
    PngMemoryWriter* writer = static_cast<PngMemoryWriter*>(png_get_io_ptr(png));
    size_t available = static_cast<size_t>(writer->mEnd - writer->mCurrent);
    size_t count = size < available ? size : available;
    memcpy(writer->mCurrent, data, count);
    writer->mCurrent += count;
}

void PngFlushMemory(png_structp)
{
}

thread_local Renderer* sContextOwner = NULL;
thread_local void* sContext = NULL;
}

Renderer* gRendererInstance = NULL;

static C4JCriticalSection sTotalAllocMutex;
static int sTotalCommandBufferBytes = 0;

Renderer* gRenderer = new Renderer;

struct Renderer::Texture
{
    bool mUsed;
    bool mHasStorage;
    int mWidth;
    int mHeight;
    int mLevels;
    int mStorageLevels;
    C4JRender::eTextureFormat mFormat;
    unsigned int mFlags;
    RenderSamplerState mSampler;
    SDL_GPUTexture* mName;
    unsigned int mGeneration;
};

struct Renderer::GpuState
{
    bool mDepthTest;
    bool mDepthWrite;
    int mDepthFunc;
    int mStencilFunc;
    unsigned char mStencilReference;
    unsigned char mStencilCompareMask;
    unsigned char mStencilWriteMask;
    bool mCullBack;
    bool mFrontFaceCCW;
    float mPolygonOffsetScale;
    float mPolygonOffsetBias;
    bool mBlendEnable;
    int mBlendSource;
    int mBlendDestination;
    float mBlendColour[4];
    bool mAlphaTestOn;
    float mAlphaReference;
    unsigned char mColourMask;
    float mViewport[4];
    int mScissor[4];
    int mPixelTexture[TEXTURE_STAGE_COUNT];
    RenderSamplerState mPixelSampler[TEXTURE_STAGE_COUNT];
    int mVertexTexture;
    RenderSamplerState mVertexSampler;
    int mVertexShader;
    int mPixelShader;
};

struct Renderer::DeferredCBuff
{
    DeferredCBuff(int id, CommandBuffer* buffer, int vertexType, C4JRender::ePixelShaderType pixelShaderType,
                  int primitiveType, const float* matrix);

    CommandBuffer* mBuffer;
    int mId;
    int mVertexType;
    C4JRender::ePixelShaderType mPixelShaderType;
    int mPrimitiveType;
    float mMatrix[16];
};

struct Renderer::Context
{
    Context(Renderer* renderer);

    Renderer* mRenderer;
    float mMatrixStack[4][16][16];
    bool mMatrixDirty[4];
    int mMatrixStackDepth[4];
    int mMatrixMode;
    int mBoundTexture[TEXTURE_STAGE_COUNT];
    bool mTextureIsMask[TEXTURE_STAGE_COUNT];
    bool mTextureIsVertex[TEXTURE_STAGE_COUNT];
    bool mFaceCull;
    bool mAlphaTestEnable;
    float mAlphaTestRef;
    bool mDepthMask;
    bool mDepthTestEnable;
    int mStencilFunc;
    int mDepthFunc;
    int mBlendSource;
    int mBlendDestination;
    bool mBlendEnable;
    bool mFogEnable;
    float mFogNearDistance;
    float mFogFarDistance;
    float mFogDensity;
    int mFogMode;
    bool mLightingEnable;
    bool mLightEnable[2];
    bool mLightingDirty;
    int mForceLOD;
    bool mOrderIndependentAlpha;
    float mVertexAlphaDistance;
    float mVertexAlpha;
    float mLightState[5][4];
    float mBoneMatrix[BONE_COUNT][16];
    int mNumBoneMatrices;
    unsigned int mDynamicVertexOffset;
    float mTexGenMatrix[2][4][4];
    CommandBuffer* mRecordingBuffer;
    int mRecordingId;
    C4JRender::eVertexType mRecordingVertexType;
    C4JRender::ePixelShaderType mRecordingPixelShaderType;
    C4JRender::ePrimitiveType mRecordingPrimitiveType;
    bool mDeferredMode;
    std::vector<DeferredCBuff> mDeferredBuffers;
    float mLastQuadUniform[4];
    float mViewportScale[4];
    float mLayeredColour[16][4];
    unsigned int mScissorRect[4];
    GpuState mGpu;
    float mVertexConstants[VERTEX_CONSTANT_COUNT][4];
    bool mVertexConstantsDirty;
    float mPixelConstants[RENDER_PIXEL_CONSTANT_COUNT][4];
    bool mPixelConstantsDirty;
};

class Renderer::CommandBuffer
{
public:
    enum eCommandType
    {
        eCommandType_Matrix,
        eCommandType_Vertices,
        eCommandType_BindTexture,
        eCommandType_Colour,
        eCommandType_DepthFunc,
        eCommandType_DepthMask,
        eCommandType_DepthTestEnable,
        eCommandType_LightingEnable,
        eCommandType_LightEnable,
        eCommandType_LightDirection,
        eCommandType_LightColour,
        eCommandType_LightAmbientColour,
        eCommandType_BlendEnable,
        eCommandType_BlendFunc,
        eCommandType_BlendFactor,
        eCommandType_FaceCull
    };

    struct Command
    {
        eCommandType mType;

        union
        {
            float mMatrix[16];
            float mColour[4];
            int mTexture;
            int mDepthFunc;
            bool mEnable;
            unsigned int mBlendFactor;

            struct
            {
                unsigned int mOffset;
                unsigned int mCount;
                unsigned char mLayer;
            } mVertices;

            struct
            {
                int mIndex;
                bool mEnable;
            } mLightEnable;

            struct
            {
                int mIndex;
                float mValue[4];
            } mLight;

            struct
            {
                int mSource;
                int mDestination;
            } mBlendFunc;
        };
    };

    CommandBuffer(bool bRecordState);
    ~CommandBuffer();

    void StartRecording();
    void EndRecording(unsigned int vertexStride);
    void Render(C4JRender::eVertexType vertexType, Context& context, int primitiveType, bool bApplyMatrix,
                unsigned char layerMask);
    bool IsBusy();

    void AddMatrix(const float* matrix);
    void AddVertices(unsigned int vertexStride, unsigned int numVertices, void* vertices, Context& context,
                     unsigned char* layers);
    void BindTexture(int texture);
    void SetColor(float red, float green, float blue, float alpha);
    void SetDepthFunc(int depthFunc);
    void SetDepthMask(bool bDepthMask);
    void SetDepthTestEnable(bool bDepthTest);
    void SetLightingEnable(bool bLighting);
    void SetLightEnable(int light, bool bEnable);
    void SetLightDirection(int light, float x, float y, float z);
    void SetLightColour(int light, float red, float green, float blue);
    void SetLightAmbientColour(float red, float green, float blue);
    void SetBlendEnable(bool bBlend);
    void SetBlendFunc(int source, int destination);
    void SetBlendFactor(unsigned int factor);
    void SetFaceCull(bool bFaceCull);

    int getNumVertices();
    bool EnsureUploaded(Device* device, int layout);

    unsigned char* mVertices;
    unsigned int mVerticesUsed;
    std::vector<Command> mCommands;
    unsigned int mVerticesCapacity;
    int mLastRenderedFrame;
    bool mRecordState;
    bool mHasVertices;
    SDL_GPUBuffer* mBuffer;
};

class Renderer::Device
{
public:
    struct TextureOperation
    {
        enum eType
        {
            eType_Storage,
            eType_Level,
            eType_Delete
        };
        eType mType;
        int mSlot;
        unsigned int mGeneration;
        int mLevel;
        int mWidth;
        int mHeight;
        SDL_GPUTexture* mName;
        bool mHasPixels;
        std::vector<unsigned char> mPixels;
    };

    Device();
    void Create(int width, int height);
    void Destroy();
    void CreateTargets(int width, int height);
    void DestroyTargets();
    bool IsRenderThread() const;
    SDL_GPUSampler* GetSampler(const RenderSamplerState& state);
    void BindFramebuffer(int framebuffer);
    void BindTextureUnit(int unit, SDL_GPUTexture* texture, SDL_GPUSampler* sampler);
    void BindVertexBuffer(SDL_GPUBuffer* buffer, int layout);
    void Invalidate();
    void FlushPending(Renderer& renderer);
    void QueueBufferDelete(SDL_GPUBuffer* buffer);
    void ExecuteTextureStorage(Texture& texture, const void* pixels);
    void ExecuteTextureLevel(Texture& texture, int level, int width, int height, const void* pixels);
    bool PrepareDraw(Renderer& renderer, Context& context);
    void DrawRange(int primitiveType, unsigned int firstVertex, unsigned int count);
    void DrawStream(Renderer& renderer, Context& context, const void* vertices, unsigned int size, unsigned int stride,
                    int layout, int primitiveType, unsigned int count);
    void DrawScreenQuad(int vertexShader, int pixelShader, const float* quad, float gamma);
    void ScreenState(const float* viewport = NULL, const int* scissor = NULL);
    void Clear(const GpuState& state, int flags, const float* colour);
    void BeginPass();
    bool BindPipeline(int primitiveType, bool vertexInput);
    void EnsureIndices(unsigned int count, bool fan);

    std::thread::id mThread;
    bool mReady = false;
    int mWidth = 0;
    int mHeight = 0;
    int mTarget = RenderTarget_Main;
    SDL_GPUBuffer* mVertexStream = NULL;
    unsigned int mStreamOffset = 0;
    SDL_GPUBuffer* mQuadIndexBuffer = NULL;
    SDL_GPUBuffer* mFanIndexBuffer = NULL;
    unsigned int mQuadIndexCapacity = 0;
    unsigned int mFanIndexCapacity = 0;
    SDL_GPUSampler* mSamplers[2][2][2][2][3] = {};
    const int mMainFramebuffer = 1;
    const int mAlphaFramebuffer = 2;
    SDL_GPUTexture* mMainColourTexture = NULL;
    SDL_GPUTexture* mMainDepthStencil = NULL;
    SDL_GPUTexture* mAlphaAccumulateTexture = NULL;
    SDL_GPUTexture* mAlphaWeightTexture = NULL;
    int mThumbnailFramebuffer[THUMBNAIL_LEVELS];
    SDL_GPUTexture* mThumbnailTexture[THUMBNAIL_LEVELS] = {};
    SDL_GPUTextureFormat mDepthFormat = SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT;
    int mCurrentFramebuffer = -1;
    SDL_GPUBuffer* mCurrentBuffer = NULL;
    int mCurrentLayout = VertexLayout_Standard;
    SDL_GPUTextureSamplerBinding mBindings[RENDER_LIGHT_TEXTURE_UNIT + 1] = {};
    GpuState mDrawState = {};
    int mDrawVertexShader = 0;
    int mDrawPixelShader = 0;
    bool mAlphaInitialise = false;
    bool mClearing = false;
    Uint64 mUniformSerial = 0;
    Context* mUniformContext = NULL;
    int mBoundVertexConstantCount = 0;
    bool mPixelConstantsBound = false;
    Uint64 mViewportUniformSerial = 0;
    int mViewportUniformBinding = -1;
    float mViewportUniform[4] = {};
    Uint64 mDynamicPassSerial = 0;
    SDL_GPUViewport mLastViewport = {};
    SDL_Rect mLastScissor = {};
    SDL_FColor mLastBlend = {};
    unsigned char mLastStencilReference = 0;
    Uint64 mIndexPassSerial = 0;
    SDL_GPUBuffer* mBoundIndexBuffer = NULL;
    C4JCriticalSection mPendingMutex;
    std::vector<TextureOperation> mTextureOperations;
    std::vector<SDL_GPUBuffer*> mDeletedBuffers;
};

Renderer::Device::Device()
{
    InitializeCriticalSection(&mPendingMutex);
    for (int level = 0; level < THUMBNAIL_LEVELS; ++level)
        mThumbnailFramebuffer[level] = 3 + level;
}

bool Renderer::Device::IsRenderThread() const
{
    return mReady && std::this_thread::get_id() == mThread;
}

void Renderer::Device::Invalidate()
{
    PcGraphics().EndPass();
    mCurrentFramebuffer = -1;
    mCurrentBuffer = NULL;
    mUniformContext = NULL;
}

void Renderer::Device::Create(int width, int height)
{
    mThread = std::this_thread::get_id();
    mReady = true;
    PcGpu& gpu = PcGraphics();
    mVertexStream = gpu.Buffer(STREAM_VERTEX_BUFFER_SIZE, SDL_GPU_BUFFERUSAGE_VERTEX);
    if (!SDL_GPUTextureSupportsFormat(gpu.device, mDepthFormat, SDL_GPU_TEXTURETYPE_2D,
                                      SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET))
        mDepthFormat = SDL_GPU_TEXTUREFORMAT_D32_FLOAT_S8_UINT;
    for (int level = 0, size = THUMBNAIL_SIZE; level < THUMBNAIL_LEVELS; ++level, size >>= 1)
    {
        mThumbnailTexture[level] = gpu.Texture(size, size, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
                                               SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET);
        SDL_GPUColorTargetInfo target = {};
        target.texture = mThumbnailTexture[level];
        target.load_op = SDL_GPU_LOADOP_CLEAR;
        target.store_op = SDL_GPU_STOREOP_STORE;
        gpu.BeginPass(&target, 1);
        gpu.EndPass();
    }
    CreateTargets(width, height);
}

void Renderer::Device::Destroy()
{
    if (!IsRenderThread())
        return;
    PcGpu& gpu = PcGraphics();
    DestroyTargets();
    for (int level = 0; level < THUMBNAIL_LEVELS; ++level)
        SDL_ReleaseGPUTexture(gpu.device, mThumbnailTexture[level]);
    for (auto& clampU : mSamplers)
        for (auto& clampV : clampU)
            for (auto& mag : clampV)
                for (auto& min : mag)
                    for (auto sampler : min)
                        if (sampler)
                            SDL_ReleaseGPUSampler(gpu.device, sampler);
    if (mQuadIndexBuffer)
        SDL_ReleaseGPUBuffer(gpu.device, mQuadIndexBuffer);
    if (mFanIndexBuffer)
        SDL_ReleaseGPUBuffer(gpu.device, mFanIndexBuffer);
    SDL_ReleaseGPUBuffer(gpu.device, mVertexStream);
    gpu.Submit(true);
    mReady = false;
}

void Renderer::Device::CreateTargets(int width, int height)
{
    PcGpu& gpu = PcGraphics();
    gpu.EndPass();
    mWidth = width;
    mHeight = height;
    const auto usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
    mMainColourTexture = gpu.Texture(width, height, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, usage);
    mAlphaAccumulateTexture = gpu.Texture(width, height, SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT, usage);
    mAlphaWeightTexture = gpu.Texture(width, height, SDL_GPU_TEXTUREFORMAT_R16_FLOAT, usage);
    mMainDepthStencil = gpu.Texture(width, height, mDepthFormat, SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET);
    SDL_GPUColorTargetInfo colours[3] = {};
    colours[0].texture = mMainColourTexture;
    colours[1].texture = mAlphaAccumulateTexture;
    colours[2].texture = mAlphaWeightTexture;
    for (auto& colour : colours)
    {
        colour.load_op = SDL_GPU_LOADOP_CLEAR;
        colour.store_op = SDL_GPU_STOREOP_STORE;
    }
    SDL_GPUDepthStencilTargetInfo depth = {};
    depth.texture = mMainDepthStencil;
    depth.clear_depth = 1.0f;
    depth.load_op = depth.stencil_load_op = SDL_GPU_LOADOP_CLEAR;
    depth.store_op = depth.stencil_store_op = SDL_GPU_STOREOP_STORE;
    gpu.BeginPass(colours, 3, &depth);
    Invalidate();
}

void Renderer::Device::DestroyTargets()
{
    PcGpu& gpu = PcGraphics();
    gpu.EndPass();
    if (mMainColourTexture)
    {
        SDL_ReleaseGPUTexture(gpu.device, mMainColourTexture);
        SDL_ReleaseGPUTexture(gpu.device, mMainDepthStencil);
        SDL_ReleaseGPUTexture(gpu.device, mAlphaAccumulateTexture);
        SDL_ReleaseGPUTexture(gpu.device, mAlphaWeightTexture);
    }
    mMainColourTexture = mMainDepthStencil = mAlphaAccumulateTexture = mAlphaWeightTexture = NULL;
    Invalidate();
}

SDL_GPUSampler* Renderer::Device::GetSampler(const RenderSamplerState& state)
{
    int mip = std::min<int>(state.mMipFilter, MIP_FILTER_LINEAR);
    SDL_GPUSampler*& sampler = mSamplers[!!state.mClampU][!!state.mClampV][!!state.mMagLinear][!!state.mMinLinear][mip];
    if (!sampler)
    {
        SDL_GPUSamplerCreateInfo info = {};
        info.min_filter = state.mMinLinear ? SDL_GPU_FILTER_LINEAR : SDL_GPU_FILTER_NEAREST;
        info.mag_filter = state.mMagLinear ? SDL_GPU_FILTER_LINEAR : SDL_GPU_FILTER_NEAREST;
        info.mipmap_mode = mip == MIP_FILTER_LINEAR ? SDL_GPU_SAMPLERMIPMAPMODE_LINEAR
                                                    : SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
        info.address_mode_u = state.mClampU ? SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE
                                            : SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
        info.address_mode_v = state.mClampV ? SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE
                                            : SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
        info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
        info.max_lod = mip == MIP_FILTER_NONE ? 0.0f : 1000.0f;
        sampler = SDL_CreateGPUSampler(PcGraphics().device, &info);
        PcGraphics().Check(sampler != NULL, "create sampler");
    }
    return sampler;
}

void Renderer::Device::BindFramebuffer(int framebuffer)
{
    if (mCurrentFramebuffer != framebuffer)
    {
        PcGraphics().EndPass();
        mCurrentFramebuffer = framebuffer;
    }
}

void Renderer::Device::BindTextureUnit(int unit, SDL_GPUTexture* texture, SDL_GPUSampler* sampler)
{
    mBindings[unit] = {texture, sampler};
}

void Renderer::Device::BindVertexBuffer(SDL_GPUBuffer* buffer, int layout)
{
    mCurrentBuffer = buffer;
    mCurrentLayout = layout;
}

void Renderer::Device::QueueBufferDelete(SDL_GPUBuffer* buffer)
{
    EnterCriticalSection(&mPendingMutex);
    mDeletedBuffers.push_back(buffer);
    LeaveCriticalSection(&mPendingMutex);
}

void Renderer::Device::ExecuteTextureStorage(Texture& texture, const void* pixels)
{
    PcGpu& gpu = PcGraphics();
    gpu.EndPass();
    if (texture.mName)
        SDL_ReleaseGPUTexture(gpu.device, texture.mName);
    texture.mName = gpu.Texture(texture.mWidth, texture.mHeight, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
                                SDL_GPU_TEXTUREUSAGE_SAMPLER, texture.mStorageLevels);
    gpu.Upload(texture.mName, 0, texture.mWidth, texture.mHeight, pixels);
}

void Renderer::Device::ExecuteTextureLevel(Texture& texture, int level, int width, int height, const void* pixels)
{
    if (texture.mName)
        PcGraphics().Upload(texture.mName, level, width, height, pixels);
}

void Renderer::Device::FlushPending(Renderer& renderer)
{
    EnterCriticalSection(&mPendingMutex);
    if (!mDeletedBuffers.empty())
        PcGraphics().EndPass();
    for (auto buffer : mDeletedBuffers)
        SDL_ReleaseGPUBuffer(PcGraphics().device, buffer);
    mDeletedBuffers.clear();
    LeaveCriticalSection(&mPendingMutex);

    EnterCriticalSection(&renderer.mTextureMutex);
    for (auto& operation : mTextureOperations)
    {
        if (operation.mType == TextureOperation::eType_Delete)
        {
            PcGraphics().EndPass();
            SDL_ReleaseGPUTexture(PcGraphics().device, operation.mName);
            continue;
        }
        Texture& texture = renderer.mTextures[operation.mSlot];
        if (texture.mGeneration != operation.mGeneration)
            continue;
        const void* pixels = operation.mHasPixels ? operation.mPixels.data() : NULL;
        if (operation.mType == TextureOperation::eType_Storage)
        {
            texture.mWidth = operation.mWidth;
            texture.mHeight = operation.mHeight;
            ExecuteTextureStorage(texture, pixels);
        }
        else
            ExecuteTextureLevel(texture, operation.mLevel, operation.mWidth, operation.mHeight, pixels);
    }
    mTextureOperations.clear();
    LeaveCriticalSection(&renderer.mTextureMutex);
}

bool Renderer::Device::PrepareDraw(Renderer& renderer, Context& context)
{
    if (!IsRenderThread() || renderer.mScreen != 0)
        return false;
    GpuState& state = context.mGpu;
    if (state.mVertexShader < 0 || state.mPixelShader < 0)
        return false;
    FlushPending(renderer);
    BindFramebuffer(mTarget == RenderTarget_OrderIndependentAlpha ? mAlphaFramebuffer : mMainFramebuffer);
    mDrawState = state;
    mDrawVertexShader = state.mVertexShader;
    mDrawPixelShader = state.mPixelShader;
    float alphaReference = state.mAlphaTestOn ? state.mAlphaReference : -3.4028235e38f;
    if (context.mPixelConstants[RENDER_PIXEL_CONSTANT_ALPHA_REFERENCE][0] != alphaReference)
    {
        context.mPixelConstants[RENDER_PIXEL_CONSTANT_ALPHA_REFERENCE][0] = alphaReference;
        context.mPixelConstantsDirty = true;
    }

    RenderSamplerState baseSampler = state.mPixelSampler[0];
    bool bSharpUI = (state.mPixelShader == RenderPixelShader_UI || state.mPixelShader == RenderPixelShader_UIColourAdd)
                    && baseSampler.mMagLinear == 0;
    float sharpUI = bSharpUI ? 1.0f : 0.0f;
    if (context.mPixelConstants[RENDER_PIXEL_CONSTANT_ALPHA_REFERENCE][1] != sharpUI)
    {
        context.mPixelConstants[RENDER_PIXEL_CONSTANT_ALPHA_REFERENCE][1] = sharpUI;
        context.mPixelConstantsDirty = true;
    }
    if (bSharpUI)
    {
        baseSampler.mMagLinear = 1;
        baseSampler.mMinLinear = 1;
    }

    PcGpu& gpu = PcGraphics();
    SDL_GPUCommandBuffer* command = gpu.Command();
    bool changed = mUniformSerial != gpu.commandSerial || mUniformContext != &context;
    int count = RenderVertexConstantCount(mDrawVertexShader);
    if (count > 0 && (changed || context.mVertexConstantsDirty || count > mBoundVertexConstantCount))
    {
        SDL_PushGPUVertexUniformData(command, RENDER_VERTEX_CONSTANT_BINDING, context.mVertexConstants, count * 16);
        context.mVertexConstantsDirty = false;
        mBoundVertexConstantCount = count;
    }
    if (changed || context.mPixelConstantsDirty || !mPixelConstantsBound)
    {
        SDL_PushGPUFragmentUniformData(command, RENDER_PIXEL_CONSTANT_BINDING, context.mPixelConstants,
                                       RENDER_PIXEL_CONSTANT_COUNT * 16);
        context.mPixelConstantsDirty = false;
        mPixelConstantsBound = true;
    }
    mUniformContext = &context;
    mUniformSerial = gpu.commandSerial;
    SDL_GPUTexture* defaultTexture = renderer.mTextures[renderer.mDefaultTexture].mName;
    for (int stage = 0; stage < TEXTURE_STAGE_COUNT; ++stage)
    {
        SDL_GPUTexture* texture = renderer.mTextures[state.mPixelTexture[stage]].mName;
        BindTextureUnit(stage, texture ? texture : defaultTexture,
                        GetSampler(stage == 0 ? baseSampler : state.mPixelSampler[stage]));
    }
    SDL_GPUTexture* light = renderer.mTextures[state.mVertexTexture].mName;
    BindTextureUnit(RENDER_LIGHT_TEXTURE_UNIT, light ? light : defaultTexture, GetSampler(state.mVertexSampler));
    return true;
}

void Renderer::Device::BeginPass()
{
    PcGpu& gpu = PcGraphics();
    if (gpu.pass)
        return;
    SDL_GPUColorTargetInfo colours[3] = {};
    SDL_GPUDepthStencilTargetInfo depth = {};
    int count = 1;
    bool hasDepth = mCurrentFramebuffer == mMainFramebuffer || mCurrentFramebuffer == mAlphaFramebuffer;
    if (mCurrentFramebuffer == 0)
        colours[0].texture = gpu.swapchain;
    else if (mCurrentFramebuffer == mMainFramebuffer)
        colours[0].texture = mMainColourTexture;
    else if (mCurrentFramebuffer == mAlphaFramebuffer)
    {
        colours[0].texture = mAlphaAccumulateTexture;
        colours[1].texture = mMainColourTexture;
        colours[2].texture = mAlphaWeightTexture;
        count = 3;
    }
    else
        colours[0].texture = mThumbnailTexture[mCurrentFramebuffer - 3];
    for (int i = 0; i < count; ++i)
    {
        colours[i].load_op = SDL_GPU_LOADOP_LOAD;
        colours[i].store_op = SDL_GPU_STOREOP_STORE;
    }
    depth.texture = mMainDepthStencil;
    depth.load_op = depth.stencil_load_op = SDL_GPU_LOADOP_LOAD;
    depth.store_op = depth.stencil_store_op = SDL_GPU_STOREOP_STORE;
    gpu.BeginPass(colours, count, hasDepth ? &depth : NULL);
}

bool Renderer::Device::BindPipeline(int primitiveType, bool vertexInput)
{
    PcGpu& gpu = PcGraphics();
    float width = mCurrentFramebuffer == 0 ? gpu.swapWidth : mWidth;
    float height = mCurrentFramebuffer == 0 ? gpu.swapHeight : mHeight;
    if (mCurrentFramebuffer >= 3)
        width = height = THUMBNAIL_SIZE >> (mCurrentFramebuffer - 3);
    float x = floorf(mDrawState.mViewport[0] + 0.5f);
    float y = floorf(mDrawState.mViewport[1] + 0.5f);
    float originalWidth = floorf(mDrawState.mViewport[2] + 0.5f);
    float originalHeight = floorf(mDrawState.mViewport[3] + 0.5f);
    float left = std::max(0.0f, x);
    float top = std::max(0.0f, y);
    float right = std::min(width, x + originalWidth);
    float bottom = std::min(height, y + originalHeight);
    if (right <= left || bottom <= top)
        return false;
    SDL_GPUViewport viewport = {left, top, right - left, bottom - top, 0, 1};
    float transform[4] = {originalWidth / viewport.w, originalHeight / viewport.h,
                          (2 * (x - left) + originalWidth - viewport.w) / viewport.w,
                          -(2 * (y - top) + originalHeight - viewport.h) / viewport.h};
    bool alpha = mCurrentFramebuffer == mAlphaFramebuffer;
    bool hasDepth = alpha || mCurrentFramebuffer == mMainFramebuffer;
    SDL_GPUGraphicsPipelineCreateInfo info = {};
    info.vertex_shader = gpu.Shader(mDrawVertexShader, true);
    info.fragment_shader = gpu.Shader(mDrawPixelShader, false);
    info.primitive_type = PrimitiveMode(primitiveType);
    info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    info.rasterizer_state.cull_mode = mDrawState.mCullBack ? SDL_GPU_CULLMODE_BACK : SDL_GPU_CULLMODE_NONE;
    info.rasterizer_state.front_face = mDrawState.mFrontFaceCCW ? SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE
                                                                : SDL_GPU_FRONTFACE_CLOCKWISE;
    info.rasterizer_state.enable_depth_clip = true;
    info.rasterizer_state.enable_depth_bias = mDrawState.mPolygonOffsetScale != 0 || mDrawState.mPolygonOffsetBias != 0;
    info.rasterizer_state.depth_bias_constant_factor = mDrawState.mPolygonOffsetBias;
    info.rasterizer_state.depth_bias_slope_factor = mDrawState.mPolygonOffsetScale;
    auto& depth = info.depth_stencil_state;
    depth.enable_depth_test = hasDepth && mDrawState.mDepthTest;
    depth.enable_depth_write = hasDepth && mDrawState.mDepthWrite;
    depth.compare_op = CompareFunction(mDrawState.mDepthFunc);
    depth.enable_stencil_test = hasDepth && !mClearing;
    depth.compare_mask = mDrawState.mStencilCompareMask;
    depth.write_mask = mDrawState.mStencilWriteMask;
    depth.front_stencil_state.fail_op = depth.front_stencil_state.depth_fail_op = SDL_GPU_STENCILOP_KEEP;
    depth.front_stencil_state.pass_op = SDL_GPU_STENCILOP_REPLACE;
    depth.front_stencil_state.compare_op = CompareFunction(mDrawState.mStencilFunc);
    depth.back_stencil_state = depth.front_stencil_state;
    SDL_GPUVertexBufferDescription buffer = {};
    SDL_GPUVertexAttribute attributes[5] = {};
    auto attribute = [&](int location, SDL_GPUVertexElementFormat format, int offset) {
        attributes[location].location = location;
        attributes[location].format = format;
        attributes[location].offset = offset;
    };
    if (vertexInput)
    {
        int count = 0;
        if (mCurrentLayout == VertexLayout_Terrain)
        {
            buffer.pitch = 16;
            attribute(0, SDL_GPU_VERTEXELEMENTFORMAT_SHORT4, 0);
            attribute(1, SDL_GPU_VERTEXELEMENTFORMAT_SHORT4, 8);
            count = 2;
        }
        else if (mCurrentLayout == VertexLayout_Position)
        {
            buffer.pitch = 8;
            attribute(0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, 0);
            count = 1;
        }
        else if (mCurrentLayout == VertexLayout_ColouredUI)
        {
            buffer.pitch = 20;
            attribute(0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, 0);
            attribute(1, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, 8);
            attribute(2, SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM, 16);
            count = 3;
        }
        else
        {
            buffer.pitch = 32;
            attribute(0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, 0);
            attribute(1, SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM, 20);
            attribute(2, SDL_GPU_VERTEXELEMENTFORMAT_BYTE4, 24);
            attribute(3, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, 12);
            attribute(4, SDL_GPU_VERTEXELEMENTFORMAT_SHORT2, 28);
            count = 5;
        }
        info.vertex_input_state.vertex_buffer_descriptions = &buffer;
        info.vertex_input_state.num_vertex_buffers = 1;
        info.vertex_input_state.vertex_attributes = attributes;
        info.vertex_input_state.num_vertex_attributes = count;
    }
    SDL_GPUColorTargetDescription colours[3] = {};
    colours[0].format = mCurrentFramebuffer == 0 ? gpu.swapFormat : SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    if (alpha)
    {
        colours[0].format = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
        colours[1].format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
        colours[2].format = SDL_GPU_TEXTUREFORMAT_R16_FLOAT;
    }
    for (int i = 0; i < (alpha ? 3 : 1); ++i)
    {
        auto& blend = colours[i].blend_state;
        blend.enable_color_write_mask = true;
        blend.color_write_mask = mDrawState.mColourMask;
        blend.enable_blend = mDrawState.mBlendEnable;
        blend.color_blend_op = blend.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
        blend.src_color_blendfactor = blend.src_alpha_blendfactor = BlendFunction(mDrawState.mBlendSource);
        blend.dst_color_blendfactor = blend.dst_alpha_blendfactor = BlendFunction(mDrawState.mBlendDestination);
        if (alpha && !mClearing)
        {
            blend.color_write_mask = i == 0 ? 7 : (i == 1 ? 8 : 1);
            if (mAlphaInitialise)
            {
                blend.enable_blend = false;
                blend.color_write_mask = i == 1 ? 8 : 15;
            }
            else
            {
                blend.enable_blend = true;
                blend.src_color_blendfactor = blend.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
                blend.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ZERO;
                blend.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
            }
        }
        if (alpha && mClearing && i != 0)
            blend.color_write_mask = 0;
    }
    info.target_info.color_target_descriptions = colours;
    info.target_info.num_color_targets = alpha ? 3 : 1;
    info.target_info.has_depth_stencil_target = hasDepth;
    info.target_info.depth_stencil_format = mDepthFormat;
    SDL_GPUGraphicsPipeline* pipeline = gpu.Pipeline(info);
    BeginPass();
    gpu.BindPipeline(pipeline);
    int viewportBinding = mDrawVertexShader == RenderVertexShader_Clear ? 0 : RENDER_VIEWPORT_CONSTANT_BINDING;
    if (mViewportUniformSerial != gpu.commandSerial || mViewportUniformBinding != viewportBinding
        || memcmp(mViewportUniform, transform, sizeof(transform)) != 0)
    {
        SDL_PushGPUVertexUniformData(gpu.Command(), viewportBinding, transform, sizeof(transform));
        memcpy(mViewportUniform, transform, sizeof(transform));
        mViewportUniformSerial = gpu.commandSerial;
        mViewportUniformBinding = viewportBinding;
    }
    bool newPass = mDynamicPassSerial != gpu.passSerial;
    if (newPass || memcmp(&mLastViewport, &viewport, sizeof(viewport)) != 0)
    {
        SDL_SetGPUViewport(gpu.pass, &viewport);
        mLastViewport = viewport;
    }
    SDL_Rect scissor = {mDrawState.mScissor[0], mDrawState.mScissor[1], mDrawState.mScissor[2], mDrawState.mScissor[3]};
    if (newPass || memcmp(&mLastScissor, &scissor, sizeof(scissor)) != 0)
    {
        SDL_SetGPUScissor(gpu.pass, &scissor);
        mLastScissor = scissor;
    }
    if (newPass || mLastStencilReference != mDrawState.mStencilReference)
    {
        SDL_SetGPUStencilReference(gpu.pass, mDrawState.mStencilReference);
        mLastStencilReference = mDrawState.mStencilReference;
    }
    SDL_FColor blend = {mDrawState.mBlendColour[0], mDrawState.mBlendColour[1], mDrawState.mBlendColour[2],
                        mDrawState.mBlendColour[3]};
    if (newPass || memcmp(&mLastBlend, &blend, sizeof(blend)) != 0)
    {
        SDL_SetGPUBlendConstants(gpu.pass, blend);
        mLastBlend = blend;
    }
    mDynamicPassSerial = gpu.passSerial;
    bool light = mDrawVertexShader == RenderVertexShader_Standard || mDrawVertexShader == RenderVertexShader_Terrain
                 || mDrawVertexShader == RenderVertexShader_StandardLit
                 || mDrawVertexShader == RenderVertexShader_TexGen || mDrawVertexShader == RenderVertexShader_Entity
                 || mDrawVertexShader == RenderVertexShader_EntityLit;
    if (light)
        SDL_BindGPUVertexSamplers(gpu.pass, 0, &mBindings[RENDER_LIGHT_TEXTURE_UNIT], 1);
    if (!mClearing && mDrawPixelShader != RenderPixelShader_AlphaAccumulate)
    {
        int count = mDrawPixelShader == RenderPixelShader_Copy || mDrawPixelShader == RenderPixelShader_Gamma
                        ? 1
                        : (mDrawPixelShader == RenderPixelShader_AlphaResolve ? 2 : 4);
        SDL_BindGPUFragmentSamplers(gpu.pass, 0, mBindings, count);
    }
    return true;
}

void Renderer::Device::EnsureIndices(unsigned int count, bool fan)
{
    unsigned int& capacity = fan ? mFanIndexCapacity : mQuadIndexCapacity;
    SDL_GPUBuffer*& buffer = fan ? mFanIndexBuffer : mQuadIndexBuffer;
    if (count <= capacity)
        return;
    capacity = std::max(count, capacity * 2);
    std::vector<unsigned int> indices((size_t)capacity * (fan ? 3 : 6));
    for (unsigned int i = 0; i < capacity; ++i)
    {
        if (fan)
        {
            indices[i * 3] = 0;
            indices[i * 3 + 1] = i + 1;
            indices[i * 3 + 2] = i + 2;
        }
        else
        {
            unsigned int quad[6] = {i * 4, i * 4 + 1, i * 4 + 2, i * 4, i * 4 + 2, i * 4 + 3};
            memcpy(&indices[i * 6], quad, sizeof(quad));
        }
    }
    PcGpu& gpu = PcGraphics();
    mBoundIndexBuffer = NULL;
    if (buffer)
        SDL_ReleaseGPUBuffer(gpu.device, buffer);
    buffer = gpu.Buffer(indices.size() * sizeof(unsigned int), SDL_GPU_BUFFERUSAGE_INDEX);
    gpu.Upload(buffer, 0, indices.data(), indices.size() * sizeof(unsigned int));
}

void Renderer::Device::DrawRange(int primitiveType, unsigned int firstVertex, unsigned int count)
{
    if (!count)
        return;
    bool quads = primitiveType == C4JRender::ePrimitiveType_Quads;
    bool fan = primitiveType == C4JRender::ePrimitiveType_TriangleFan;
    if ((quads && count < 4) || (fan && count < 3))
        return;
    unsigned int primitives = quads ? count / 4 : (fan ? count - 2 : 0);
    if (quads || fan)
        EnsureIndices(primitives, fan);
    if (!BindPipeline(primitiveType, true))
        return;
    SDL_GPUBufferBinding vertex = {mCurrentBuffer, 0};
    SDL_BindGPUVertexBuffers(PcGraphics().pass, 0, &vertex, 1);
    if (quads || fan)
    {
        SDL_GPUBufferBinding index = {fan ? mFanIndexBuffer : mQuadIndexBuffer, 0};
        if (mIndexPassSerial != PcGraphics().passSerial || mBoundIndexBuffer != index.buffer)
        {
            SDL_BindGPUIndexBuffer(PcGraphics().pass, &index, SDL_GPU_INDEXELEMENTSIZE_32BIT);
            mBoundIndexBuffer = index.buffer;
            mIndexPassSerial = PcGraphics().passSerial;
        }
        SDL_DrawGPUIndexedPrimitives(PcGraphics().pass, primitives * (fan ? 3 : 6), 1, 0, firstVertex, 0);
    }
    else
        SDL_DrawGPUPrimitives(PcGraphics().pass, count, 1, firstVertex, 0);
}

void Renderer::Device::DrawStream(Renderer& renderer, Context& context, const void* vertices, unsigned int size,
                                  unsigned int stride, int layout, int primitiveType, unsigned int count)
{
    if (!size || !PrepareDraw(renderer, context))
        return;
    unsigned int offset = (mStreamOffset + stride - 1) / stride * stride;
    if (offset + size > STREAM_VERTEX_BUFFER_SIZE)
        offset = 0;
    if (size > STREAM_VERTEX_BUFFER_SIZE)
        FatalRendererError("dynamic vertex upload exceeds stream capacity");
    PcGraphics().Upload(mVertexStream, offset, vertices, size, offset == 0);
    mStreamOffset = offset + size;
    BindVertexBuffer(mVertexStream, layout);
    DrawRange(primitiveType, offset / stride, count);
}

void Renderer::Device::ScreenState(const float* viewport, const int* scissor)
{
    memset(&mDrawState, 0, sizeof(mDrawState));
    mDrawState.mDepthFunc = COMPARE_ALWAYS;
    mDrawState.mStencilFunc = COMPARE_ALWAYS;
    mDrawState.mColourMask = CHANNEL_MASK_RGBA;
    int width = mCurrentFramebuffer == 0 ? PcGraphics().swapWidth : mWidth;
    int height = mCurrentFramebuffer == 0 ? PcGraphics().swapHeight : mHeight;
    if (mCurrentFramebuffer >= 3)
        width = height = THUMBNAIL_SIZE >> (mCurrentFramebuffer - 3);
    float fullViewport[4] = {0, 0, (float)width, (float)height};
    int fullScissor[4] = {0, 0, width, height};
    memcpy(mDrawState.mViewport, viewport ? viewport : fullViewport, sizeof(fullViewport));
    memcpy(mDrawState.mScissor, scissor ? scissor : fullScissor, sizeof(fullScissor));
}

void Renderer::Device::DrawScreenQuad(int vertexShader, int pixelShader, const float* quad, float gamma)
{
    mUniformContext = NULL;
    mDrawVertexShader = vertexShader;
    mDrawPixelShader = pixelShader;
    float vertex[4] = {quad[0], quad[1], quad[2], quad[3]};
    if (vertexShader == RenderVertexShader_ScreenQuad)
        SDL_PushGPUVertexUniformData(PcGraphics().Command(), 0, vertex, sizeof(vertex));
    if (pixelShader == RenderPixelShader_Gamma)
    {
        float pixel[4] = {gamma, 0, 0, 0};
        SDL_PushGPUFragmentUniformData(PcGraphics().Command(), 0, pixel, sizeof(pixel));
    }
    if (!BindPipeline(C4JRender::ePrimitiveType_TriangleStrip, false))
        return;
    SDL_DrawGPUPrimitives(PcGraphics().pass, 4, 1, 0, 0);
}

void Renderer::Device::Clear(const GpuState& state, int flags, const float* colour)
{
    BindFramebuffer(mTarget == RenderTarget_OrderIndependentAlpha ? mAlphaFramebuffer : mMainFramebuffer);
    ScreenState(state.mViewport, state.mScissor);
    mDrawState.mColourMask = flags & 2 ? 15 : 0;
    mDrawState.mDepthTest = mDrawState.mDepthWrite = (flags & 1) != 0;
    mDrawState.mDepthFunc = COMPARE_ALWAYS;
    mClearing = true;
    SDL_PushGPUFragmentUniformData(PcGraphics().Command(), 0, colour, 16);
    float quad[4] = {0, 0, 1, 1};
    DrawScreenQuad(RenderVertexShader_Clear, RenderPixelShader_Clear, quad, 1);
    mClearing = false;
}

Renderer::Context::Context(Renderer* renderer)
{
    memset(mMatrixStack, 0, sizeof(mMatrixStack));
    MatrixIdentity(mMatrixStack[0][0]);
    mMatrixStackDepth[0] = 0;
    MatrixIdentity(mMatrixStack[1][0]);
    mMatrixStackDepth[1] = 0;
    MatrixIdentity(mMatrixStack[2][0]);
    mMatrixStackDepth[2] = 0;
    MatrixIdentity(mMatrixStack[3][0]);
    mMatrixStackDepth[3] = 0;
    memset(mMatrixDirty, 0, sizeof(mMatrixDirty));

    mRenderer = renderer;
    mTextureIsVertex[0] = false;
    mBoundTexture[3] = 0;
    mAlphaTestEnable = false;
    mAlphaTestRef = 1.0f;
    mTextureIsMask[2] = false;
    mTextureIsMask[3] = false;
    mTextureIsMask[1] = false;
    mTextureIsMask[0] = false;
    mTextureIsVertex[2] = false;
    mTextureIsVertex[3] = false;
    mTextureIsVertex[1] = false;
    mMatrixMode = 0;
    mBoundTexture[0] = 0;
    mFaceCull = true;
    mDeferredMode = false;
    mDepthTestEnable = true;
    mBoundTexture[1] = 0;
    mBoundTexture[2] = 0;
    mDepthFunc = COMPARE_ALWAYS;
    mStencilFunc = COMPARE_ALWAYS;
    mBlendSource = BLEND_ONE;
    mBlendDestination = 0;
    mBlendEnable = false;
    mFogEnable = false;
    mFogNearDistance = 0.0f;
    mFogFarDistance = 0.0f;
    mFogDensity = 0.0f;
    mFogMode = 0;
    mRecordingBuffer = NULL;
    mRecordingId = 0;
    mRecordingVertexType = C4JRender::eVertexType_Default;
    mRecordingPixelShaderType = C4JRender::ePixelShaderType_Default;
    mRecordingPrimitiveType = C4JRender::ePrimitiveType_Triangles;
    mLightingEnable = false;
    mLightEnable[0] = false;
    mLightEnable[1] = false;
    mDepthMask = true;
    mForceLOD = -1;

    memset(mLightState, 0, sizeof(mLightState));
    memset(mBoneMatrix, 0, sizeof(mBoneMatrix));
    mNumBoneMatrices = 0;
    memset(mTexGenMatrix, 0, sizeof(mTexGenMatrix));
    memset(mLastQuadUniform, 0, sizeof(mLastQuadUniform));
    memset(mLayeredColour, 0, sizeof(mLayeredColour));
    memset(mScissorRect, 0, sizeof(mScissorRect));

    mLightingDirty = false;
    mViewportScale[1] = 1.0f;
    mVertexAlpha = 0.0f;
    mViewportScale[2] = 0.0f;
    mVertexAlphaDistance = 0.0f;
    mViewportScale[3] = 0.0f;
    mViewportScale[0] = 1.0f;
    mOrderIndependentAlpha = false;
    mDynamicVertexOffset = 0;

    memset(&mGpu, 0, sizeof(mGpu));
    mGpu.mDepthTest = true;
    mGpu.mDepthWrite = true;
    mGpu.mDepthFunc = COMPARE_ALWAYS;
    mGpu.mStencilFunc = COMPARE_ALWAYS;
    mGpu.mStencilWriteMask = 0xFF;
    mGpu.mCullBack = true;
    mGpu.mFrontFaceCCW = true;
    mGpu.mBlendSource = BLEND_ONE;
    mGpu.mBlendDestination = 0;
    mGpu.mAlphaTestOn = true;
    mGpu.mColourMask = CHANNEL_MASK_RGBA;
    mGpu.mVertexShader = -1;
    mGpu.mPixelShader = -1;
    for (int stage = 0; stage < TEXTURE_STAGE_COUNT; ++stage)
    {
        mGpu.mPixelSampler[stage] = MakeSampler(false, false, MIP_FILTER_POINT);
    }
    mGpu.mVertexSampler = MakeSampler(false, false, MIP_FILTER_POINT);

    memset(mVertexConstants, 0, sizeof(mVertexConstants));
    mVertexConstantsDirty = true;
    memset(mPixelConstants, 0, sizeof(mPixelConstants));
    mPixelConstantsDirty = true;
}

Renderer::DeferredCBuff::DeferredCBuff(int id, CommandBuffer* buffer, int vertexType,
                                       C4JRender::ePixelShaderType pixelShaderType, int primitiveType,
                                       const float* matrix)
{
    mId = id;
    mBuffer = buffer;
    mVertexType = vertexType;
    mPixelShaderType = pixelShaderType;
    mPrimitiveType = primitiveType;
    memcpy(mMatrix, matrix, sizeof(mMatrix));
}

Renderer::Renderer()
{
    gRendererInstance = this;

    memset(mClearColour, 0, sizeof(mClearColour));
    mLayout = ELayout_Screen;
    mDRCFrameStarted = false;
    mDisplayEnabled = false;
    mForegroundAcquired = false;
    mInitialised = false;
    mDevice = NULL;
    mDefaultTexture = 0;
    mGamma = DEFAULT_GAMMA;
    mFrameCount = 0;
    mCapturingThumbnail = false;
    mCurrentVertexShader = -1;
    mCurrentPixelShader = -1;
    mCurrentViewport = eViewportType_FullScreen;
    mTextureStateDirty = false;
    mTextures = NULL;
    mScreenWidth[0] = UI_WIDTH;
    mScreenHeight[0] = UI_HEIGHT;
    mScreenWidth[1] = DRC_WIDTH;
    mScreenHeight[1] = DRC_HEIGHT;
    mScreen = 0;
    mHiDef = true;
    mAllowStaticCommandBufferIds = true;
    mNextCommandBufferId = 0;
    mCommandBufferSlots = NULL;
    mCommandBuffers = NULL;
    mCommandBufferMatrices = NULL;
    mCommandBufferIds = NULL;
    mCommandBufferVertexTypes = NULL;
    mCommandBufferPixelShaderTypes = NULL;
    mCommandBufferPrimitiveTypes = NULL;
    mNumCommandBuffers = 0;
    mNumRetiredCommandBuffers = 0;
    mPeakCommandBuffers = 0;
    mPeakCommandBufferFrame = 0;
    mCommandBufferWarned = false;
    mCommandBufferVertsDrawn = 0;
    mCommandBufferVertsDrawnLastFrame = 0;
    mReservedMEM1 = NULL;
    mScreenGrabRequested = false;
    memset(mPixelColours, 0, sizeof(mPixelColours));
    mPixelUniformsDirty = true;
    memset(mLayeredParams, 0, sizeof(mLayeredParams));
    mLayeredParamsDirty = true;
}

Renderer::~Renderer()
{
    if (mDevice != NULL && mDevice->IsRenderThread())
    {
        PcKeyboardOverlayShutdown();
        mDevice->Destroy();
        PcWindowDestroy();
    }
}

void Renderer::Tick()
{
    CBuffTick();
}

void Renderer::UpdateGamma(unsigned short gamma)
{
    mGamma = gamma;
}

void Renderer::InitialiseContext(bool bReset)
{
    sContext = new Context(this);
    sContextOwner = this;
}

void Renderer::InitialiseContext()
{
    InitialiseContext(false);
}

Renderer::Context& Renderer::getContext()
{
    if (sContext == NULL || sContextOwner != this)
    {
        InitialiseContext(false);
    }

    return *static_cast<Context*>(sContext);
}

bool Renderer::IsRenderThread()
{
    return mDevice != NULL && mDevice->IsRenderThread();
}

void Renderer::SetVertexUniformReg(unsigned int offset, unsigned int count, const float* values)
{
    Context& context = getContext();

    if (offset >= VERTEX_CONSTANT_COUNT * 4)
    {
        return;
    }

    if (offset + count > VERTEX_CONSTANT_COUNT * 4)
    {
        count = VERTEX_CONSTANT_COUNT * 4 - offset;
    }

    float* destination = &context.mVertexConstants[0][0] + offset;
    if (memcmp(destination, values, count * sizeof(float)) != 0)
    {
        memcpy(destination, values, count * sizeof(float));
        context.mVertexConstantsDirty = true;
    }
}

void Renderer::SetPixelUniformReg(unsigned int offset, unsigned int count, const float* values)
{
    Context& context = getContext();
    unsigned int limit = RENDER_PIXEL_CONSTANT_ALPHA_REFERENCE * 4;

    if (offset >= limit)
    {
        return;
    }

    if (offset + count > limit)
    {
        count = limit - offset;
    }

    float* destination = &context.mPixelConstants[0][0] + offset;
    if (memcmp(destination, values, count * sizeof(float)) != 0)
    {
        memcpy(destination, values, count * sizeof(float));
        context.mPixelConstantsDirty = true;
    }
}

void Renderer::SetGpuShaders(int vertexShader, int pixelShader)
{
    Context& context = getContext();
    context.mGpu.mVertexShader = vertexShader;
    context.mGpu.mPixelShader = pixelShader;
}

void Renderer::ApplyGpuDepthStencil()
{
    Context& context = getContext();
    context.mGpu.mDepthTest = context.mDepthTestEnable;
    context.mGpu.mDepthWrite = context.mDepthMask;
    context.mGpu.mDepthFunc = context.mDepthFunc;
    context.mGpu.mStencilFunc = context.mStencilFunc;
}

void Renderer::ApplyGpuPolygonControl(bool bFrontFaceCCW)
{
    Context& context = getContext();
    context.mGpu.mFrontFaceCCW = bFrontFaceCCW;
    context.mGpu.mCullBack = context.mFaceCull;
}

void Renderer::ApplyGpuAlphaTest()
{
    Context& context = getContext();
    context.mGpu.mAlphaTestOn = true;
    context.mGpu.mAlphaReference = context.mAlphaTestEnable ? context.mAlphaTestRef : 0.0f;
}

void Renderer::RestoreGpuStateFromContext()
{
    Context& context = getContext();
    ApplyGpuDepthStencil();
    ApplyGpuPolygonControl(true);
    context.mGpu.mBlendEnable = context.mBlendEnable;
    context.mGpu.mBlendSource = context.mBlendSource;
    context.mGpu.mBlendDestination = context.mBlendDestination;
    ApplyGpuAlphaTest();
}

void Renderer::ApplyGpuViewportAndScissor(float x, float y, float width, float height)
{
    Context& context = getContext();
    context.mGpu.mViewport[0] = x;
    context.mGpu.mViewport[1] = y;
    context.mGpu.mViewport[2] = width;
    context.mGpu.mViewport[3] = height;
    context.mGpu.mScissor[0] = (int)x;
    context.mGpu.mScissor[1] = (int)y;
    context.mGpu.mScissor[2] = (int)width;
    context.mGpu.mScissor[3] = (int)height;
}

void Renderer::SetScreenScissor(unsigned int left, unsigned int top, unsigned int width, unsigned int height)
{
    Context& context = getContext();
    float scaleX = 1.0f;
    float scaleY = 1.0f;
    float offsetX = 0.0f;
    float offsetY = 0.0f;

    if (mScreen == 0)
    {
        float layoutWidth;
        float layoutHeight;
        GetLayoutRect(mLayout, &offsetX, &offsetY, &layoutWidth, &layoutHeight);
        scaleX = layoutWidth / (float)UI_WIDTH;
        scaleY = layoutHeight / (float)UI_HEIGHT;
    }

    float screenWidth = (float)mScreenWidth[mScreen];
    float screenHeight = (float)mScreenHeight[mScreen];
    float x0 = offsetX + (float)left * scaleX;
    float y0 = offsetY + (float)top * scaleY;
    float x1 = offsetX + ((float)left + (float)width) * scaleX;
    float y1 = offsetY + ((float)top + (float)height) * scaleY;

    x0 = x0 < 0.0f ? 0.0f : (x0 > screenWidth ? screenWidth : x0);
    y0 = y0 < 0.0f ? 0.0f : (y0 > screenHeight ? screenHeight : y0);
    x1 = x1 < x0 ? x0 : (x1 > screenWidth ? screenWidth : x1);
    y1 = y1 < y0 ? y0 : (y1 > screenHeight ? screenHeight : y1);

    context.mGpu.mScissor[0] = (int)floorf(x0 + 0.5f);
    context.mGpu.mScissor[1] = (int)floorf(y0 + 0.5f);
    context.mGpu.mScissor[2] = (int)floorf(x1 + 0.5f) - context.mGpu.mScissor[0];
    context.mGpu.mScissor[3] = (int)floorf(y1 + 0.5f) - context.mGpu.mScissor[1];
}

void Renderer::ResizeTargets()
{
    int drawableWidth = 0;
    int drawableHeight = 0;
    PcWindowGetDrawableSize(&drawableWidth, &drawableHeight);

    if (drawableWidth <= 0 || drawableHeight <= 0)
    {
        return;
    }

    int width = drawableWidth;
    int height = drawableHeight;

    width = width < 1 ? 1 : width;
    height = height < 1 ? 1 : height;

    if (width == mDevice->mWidth && height == mDevice->mHeight)
    {
        return;
    }

    mDevice->DestroyTargets();
    mDevice->CreateTargets(width, height);
    mScreenWidth[0] = width;
    mScreenHeight[0] = height;
}

void Renderer::Initialise(bool bFirstTime)
{
    if (bFirstTime)
    {
        InitialiseContext(true);

        mCommandBufferSlots = new int[COMMAND_BUFFER_ID_COUNT];
        mCommandBuffers = new CommandBuffer*[COMMAND_BUFFER_COUNT];
        mCommandBufferMatrices = new float[COMMAND_BUFFER_COUNT * 16];
        mCommandBufferIds = new int[COMMAND_BUFFER_COUNT];
        mCommandBufferVertexTypes = new unsigned char[COMMAND_BUFFER_COUNT];
        mCommandBufferPixelShaderTypes = new int[COMMAND_BUFFER_COUNT];
        mCommandBufferPrimitiveTypes = new unsigned char[COMMAND_BUFFER_COUNT];

        memset(mCommandBufferSlots, 0xFF, sizeof(int) * COMMAND_BUFFER_ID_COUNT);
        memset(mCommandBuffers, 0, sizeof(CommandBuffer*) * COMMAND_BUFFER_COUNT);
        memset(mCommandBufferMatrices, 0, sizeof(float) * COMMAND_BUFFER_COUNT * 16);
        memset(mCommandBufferIds, 0, sizeof(int) * COMMAND_BUFFER_COUNT);
        memset(mCommandBufferVertexTypes, 0, COMMAND_BUFFER_COUNT);
        memset(mCommandBufferPixelShaderTypes, 0, sizeof(int) * COMMAND_BUFFER_COUNT);
        memset(mCommandBufferPrimitiveTypes, 0, COMMAND_BUFFER_COUNT);

        mNumCommandBuffers = 0;
        mScreenGrabRequested = false;
        mDRCFrameStarted = false;
        mNumRetiredCommandBuffers = 0;
        PcCrashReport::AddSection("Command buffers", &Renderer::WriteCommandBufferReport);

        if (!PcWindowCreate(UI_WIDTH, UI_HEIGHT))
        {
            FatalRendererError("the SDL window and Vulkan device could not be created");
        }

        SDL_PropertiesID properties = SDL_GetGPUDeviceProperties(PcGraphics().device);
        const char* deviceName = SDL_GetStringProperty(properties, SDL_PROP_GPU_DEVICE_NAME_STRING, "");
        LOG_INFO(sLog, "%s / %s / %s", deviceName,
                 SDL_GetStringProperty(properties, SDL_PROP_GPU_DEVICE_DRIVER_NAME_STRING, ""),
                 SDL_GetStringProperty(properties, SDL_PROP_GPU_DEVICE_DRIVER_INFO_STRING, ""));
        sGpuName = deviceName;

        mDevice = new Device;
        mDevice->Create(UI_WIDTH, UI_HEIGHT);
        mScreenWidth[0] = UI_WIDTH;
        mScreenHeight[0] = UI_HEIGHT;

        float black[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        SetClearColour(black);

        mForegroundAcquired = true;
    }

    mDisplayEnabled = false;
    mHiDef = true;
    mScreenWidth[1] = DRC_WIDTH;
    mScreenHeight[1] = DRC_HEIGHT;
    ResizeTargets();

    if (bFirstTime)
    {
        mTextures = new Texture[TEXTURE_COUNT];
        memset(mTextures, 0, sizeof(Texture) * TEXTURE_COUNT);
        InitializeCriticalSection(&mCommandBufferMutex);
        InitializeCriticalSection(&mTextureMutex);
        InitializeCriticalSection(&sTotalAllocMutex);
        mDefaultTexture = TextureCreate();
        TextureBind(mDefaultTexture);

        unsigned char* white = new unsigned char[0x400];
        memset(white, 0xFF, 0x400);
        TextureData(0x10, 0x10, white, 0, C4JRender::eTextureFormat_RGBA8);
        delete[] white;

        mFrameCount = 0;
        mCapturingThumbnail = false;
        mGamma = DEFAULT_GAMMA;
        mNextCommandBufferId = 0;
        mAllowStaticCommandBufferIds = true;
    }

    ApplyGpuViewportAndScissor(0.0f, 0.0f, (float)mScreenWidth[0], (float)mScreenHeight[0]);

    for (int colour = 0; colour < 8; ++colour)
    {
        if (colour == 0 || colour >= 4)
        {
            mPixelColours[colour][0] = 1.0f;
            mPixelColours[colour][1] = 1.0f;
            mPixelColours[colour][2] = 1.0f;
            mPixelColours[colour][3] = 1.0f;
        }
    }

    memset(mPixelColours[3], 0, sizeof(mPixelColours[3]));
    mPixelUniformsDirty = true;
    memset(mLayeredParams, 0, sizeof(mLayeredParams));
    mLayeredParamsDirty = true;

    StateSetVertexTextureUV(0.0f, 0.0f);
    Context& context = getContext();
    context.mGpu.mVertexTexture = mDefaultTexture;
    context.mGpu.mVertexSampler = mTextures[mDefaultTexture].mSampler;
    for (int stage = 0; stage < TEXTURE_STAGE_COUNT; ++stage)
    {
        context.mGpu.mPixelTexture[stage] = mDefaultTexture;
        context.mGpu.mPixelSampler[stage] = mTextures[mDefaultTexture].mSampler;
    }

    mTextureStateDirty = false;
    mCurrentVertexShader = -1;
    mCurrentPixelShader = -1;
    mCommandBufferVertsDrawn = 0;
    mCommandBufferVertsDrawnLastFrame = 0;
    mInitialised = true;
}

void Renderer::Initialise()
{
    Initialise(true);
}

void Renderer::StartFrameInternal(int screen, bool bAlphaTest)
{
    mCurrentVertexShader = -1;
    mCurrentPixelShader = -1;
    TextureBindVertex(-1);
    TextureBind(-1);
    StateSetColour(1.0f, 1.0f, 1.0f, 1.0f);
    StateSetDepthFunc(3);
    StateSetDepthMask(true);
    StateSetStencil(7, 0, 0, 0);
    StateSetBlendEnable(true);
    StateSetBlendFunc(4, 5);
    StateSetBlendFactor(0xFFFFFFFF);
    StateSetAlphaTestEnable(false);
    StateSetAlphaFunc(4, 0.1f);
    StateSetFaceCull(true);
    StateSetLineWidth(1.0f);
    StateSetWriteEnable(true, true, true, true);
    StateSetDepthTestEnable(false);
    StateSetAlphaTestEnable(bAlphaTest);

    if (screen == 0)
    {
        mDevice->mTarget = RenderTarget_Main;
        mScreen = 0;
    }
    else
    {
        mScreen = screen;
    }

    ApplyGpuViewportAndScissor(0.0f, 0.0f, (float)mScreenWidth[mScreen], (float)mScreenHeight[mScreen]);
}

void Renderer::StartFrame()
{
    if (IsRenderThread())
    {
        mDevice->FlushPending(*this);
        ResizeTargets();
    }

    mLayout = ELayout_Screen;
    StartFrameInternal(0, true);
}

void Renderer::GetLayoutRect(ELayout layout, float* x, float* y, float* width, float* height)
{
    float screenWidth = (float)mScreenWidth[mScreen];
    float screenHeight = (float)mScreenHeight[mScreen];
    *x = 0.0f;
    *y = 0.0f;
    *width = screenWidth;
    *height = screenHeight;
    if (mScreen != 0 || layout == ELayout_Screen)
        return;

    float fittedWidth = screenHeight * (float)UI_WIDTH / (float)UI_HEIGHT;
    bool heightLimited = fittedWidth <= screenWidth;
    if (layout == ELayout_InterfaceBackground)
        heightLimited = !heightLimited;
    if (heightLimited)
    {
        *width = floorf(fittedWidth + 0.5f);
    }
    else
    {
        *height = floorf(screenWidth * (float)UI_HEIGHT / (float)UI_WIDTH + 0.5f);
    }
    *x = floorf((screenWidth - *width) * 0.5f);
    *y = floorf((screenHeight - *height) * 0.5f);
}

void Renderer::SetLayout(ELayout layout)
{
    if (layout == mLayout)
        return;
    mLayout = layout;
    StateSetViewport(mCurrentViewport);
}

float Renderer::GetScreenAspect()
{
    if (mScreenHeight[0] <= 0)
        return (float)UI_WIDTH / (float)UI_HEIGHT;
    return (float)mScreenWidth[0] / (float)mScreenHeight[0];
}

void Renderer::GetInterfaceArea(float* left, float* top, float* width, float* height)
{
    int screen = mScreen;
    mScreen = 0;
    float x;
    float y;
    float areaWidth;
    float areaHeight;
    GetLayoutRect(ELayout_Interface, &x, &y, &areaWidth, &areaHeight);
    mScreen = screen;
    float screenWidth = mScreenWidth[0] > 0 ? (float)mScreenWidth[0] : 1.0f;
    float screenHeight = mScreenHeight[0] > 0 ? (float)mScreenHeight[0] : 1.0f;
    *left = x / screenWidth;
    *top = y / screenHeight;
    *width = areaWidth / screenWidth;
    *height = areaHeight / screenHeight;
}

void Renderer::ResetRenderState()
{
    StartFrameInternal(0, false);
}

void Renderer::GammaCorrect()
{
    if (IsRenderThread())
    {
        int drawableWidth = 0;
        int drawableHeight = 0;
        PcWindowGetDrawableSize(&drawableWidth, &drawableHeight);

        if (drawableWidth > 0 && drawableHeight > 0)
        {
            if (!PcGraphics().AcquireSwapchain(PcWindowIsVSync()))
                return;
            mDevice->BindFramebuffer(0);
            mDevice->ScreenState();
            mDevice->BindTextureUnit(0, mDevice->mMainColourTexture,
                                     mDevice->GetSampler(MakeSampler(true, true, MIP_FILTER_NONE)));
            float quad[4] = {0.0f, 0.0f, 1.0f, 1.0f};
            float gamma = 1.0f / ((float)mGamma * 3.0517578125e-05f * 1.5f + 0.5f);
            mDevice->DrawScreenQuad(RenderVertexShader_ScreenQuad, RenderPixelShader_Gamma, quad, gamma);
        }
    }

    SetGpuShaders(RenderVertexShader_ScreenQuad, RenderPixelShader_Gamma);
    RestoreGpuStateFromContext();
    StateSetViewport(mCurrentViewport);
    mCurrentPixelShader = -1;
    mCurrentVertexShader = -1;
}

void Renderer::StartDRCFrame(bool bCopyFrame)
{
    GammaCorrect();
    StartFrameInternal(1, true);
    mDRCFrameStarted = true;
}

void Renderer::DoScreenGrabOnNextPresent()
{
    mScreenGrabRequested = true;
}

void Renderer::TEMP_PatchStateFromIggy()
{
    StateSetViewport(mCurrentViewport);
    mCurrentPixelShader = -1;
    mCurrentVertexShader = -1;
}

void Renderer::Present()
{
    Context& context = getContext();

    if (!mDRCFrameStarted)
    {
        GammaCorrect();
    }

    mDRCFrameStarted = false;

    if (IsRenderThread())
    {
        // A Wayland configure can arrive between rendering and acquisition.
        // Position the overlay against the image actually acquired this frame.
        PcKeyboardOverlayDraw(PcGraphics().swapWidth, PcGraphics().swapHeight);
        PcWindowSwap();
        mDevice->Invalidate();
        mDevice->mStreamOffset = 0;

        if (!mDisplayEnabled)
        {
            PcWindowShow();
            mDisplayEnabled = true;
        }
    }

    mCurrentPixelShader = -1;
    mCurrentVertexShader = -1;
    ++mFrameCount;

    context.mDynamicVertexOffset = 0;

    mCommandBufferVertsDrawnLastFrame = mCommandBufferVertsDrawn;
    mCommandBufferVertsDrawn = 0;
}

void Renderer::Clear(int flags)
{
    Context& context = getContext();
    GpuState& gpu = context.mGpu;

    if (IsRenderThread() && mScreen == 0 && (flags & 3) != 0)
    {
        mDevice->FlushPending(*this);
        mDevice->Clear(gpu, flags, mClearColour);
    }

    SetGpuShaders(RenderVertexShader_Clear, RenderPixelShader_Copy);
    RestoreGpuStateFromContext();
    gpu.mColourMask = CHANNEL_MASK_RGBA;
    gpu.mStencilCompareMask = 0;
    gpu.mStencilWriteMask = 0xFF;
    gpu.mStencilReference = 0;
    mCurrentVertexShader = -1;
    mCurrentPixelShader = -1;
}

void Renderer::SetClearColour(const float* colour)
{
    mClearColour[0] = colour[0];
    mClearColour[1] = colour[1];
    mClearColour[2] = colour[2];
    mClearColour[3] = colour[3];
}

bool Renderer::IsWidescreen()
{
    return true;
}

bool Renderer::IsHiDef()
{
    return mHiDef;
}

void Renderer::ConvertLinearToPng(ImageFileBuffer* buffer, unsigned char* pixels, unsigned int width,
                                  unsigned int height)
{
    int capacity = (int)((height * width * 24) / 5) + 0x100;
    void* png = malloc(capacity);
    int written = 0;
    SaveTextureDataToMemory(png, capacity, &written, width, height, (int*)pixels);
    buffer->mFormat = 0;
    buffer->mData = png;
    buffer->mSize = written;
}

void Renderer::CaptureThumbnail(ImageFileBuffer* buffer)
{
    float left = 0.0f;
    float right = 1.0f;
    float top = 0.0f;
    float bottom = 1.0f;

    switch (mCurrentViewport)
    {
    case C4JRender::eViewportType_FullScreen:
        break;
    case C4JRender::eViewportType_TopHalf:
        bottom = 0.5f;
        break;
    case C4JRender::eViewportType_BottomHalf:
        top = 0.5f;
        break;
    case C4JRender::eViewportType_LeftHalf:
    case C4JRender::eViewportType_RightHalf:
        right = 0.5f;
        break;
    case C4JRender::eViewportType_TopLeftQuarter:
        right = 0.5f;
        bottom = 0.5f;
        break;
    case C4JRender::eViewportType_TopRightQuarter:
        left = 0.5f;
        bottom = 0.5f;
        break;
    case C4JRender::eViewportType_BottomLeftQuarter:
        right = 0.5f;
        top = 0.5f;
        break;
    case C4JRender::eViewportType_BottomRightQuarter:
        left = 0.5f;
        top = 0.5f;
        break;
    default:
        break;
    }

    float aspect = IsWidescreen() ? 1.7777777910232544f : 1.3333333730697632f;
    float height = bottom - top;
    float scaledLeft = left * aspect;
    float scaledRight = right * aspect;
    float width = scaledRight - scaledLeft;

    if (width < height)
    {
        float inset = (height - width) * 0.5f;
        top = top + inset;
        height = (bottom - inset) - top;
    }
    else
    {
        float inset = (width - height) * 0.5f;
        scaledLeft = scaledLeft + inset;
        scaledRight = scaledRight - inset;
    }

    if (!IsRenderThread())
    {
        buffer->mFormat = 0;
        buffer->mData = NULL;
        buffer->mSize = 0;
        return;
    }

    mDevice->FlushPending(*this);
    SDL_GPUSampler* pointSampler = mDevice->GetSampler(MakeSampler(true, false, MIP_FILTER_NONE));

    int size = THUMBNAIL_SIZE;
    float quad[4];
    quad[0] = scaledLeft / aspect;
    quad[1] = top;
    quad[2] = scaledRight / aspect - scaledLeft / aspect;
    quad[3] = height;
    mDevice->BindFramebuffer(mDevice->mThumbnailFramebuffer[0]);
    mDevice->ScreenState();
    mDevice->BindTextureUnit(0, mDevice->mMainColourTexture, pointSampler);
    mDevice->DrawScreenQuad(RenderVertexShader_ScreenQuad, RenderPixelShader_Copy, quad, 1.0f);

    for (int level = 1; level < THUMBNAIL_LEVELS; ++level)
    {
        size >>= 1;
        quad[0] = 0.0f;
        quad[1] = 0.0f;
        quad[2] = 1.0f;
        quad[3] = 1.0f;
        mDevice->BindFramebuffer(mDevice->mThumbnailFramebuffer[level]);
        mDevice->ScreenState();
        mDevice->BindTextureUnit(0, mDevice->mThumbnailTexture[level - 1], pointSampler);
        mDevice->DrawScreenQuad(RenderVertexShader_ScreenQuad, RenderPixelShader_Copy, quad, 1.0f);
    }

    std::vector<unsigned char> pixels(THUMBNAIL_OUTPUT_SIZE * THUMBNAIL_OUTPUT_SIZE * 4);
    PcGraphics().Download(mDevice->mThumbnailTexture[THUMBNAIL_LEVELS - 1], THUMBNAIL_OUTPUT_SIZE,
                          THUMBNAIL_OUTPUT_SIZE, pixels.data());
    ConvertLinearToPng(buffer, &pixels[0], THUMBNAIL_OUTPUT_SIZE, THUMBNAIL_OUTPUT_SIZE);

    SetGpuShaders(RenderVertexShader_ScreenQuad, RenderPixelShader_Copy);
    RestoreGpuStateFromContext();
    StateSetViewport(mCurrentViewport);
    mCurrentPixelShader = -1;
    mCurrentVertexShader = -1;
}

void Renderer::CaptureScreen(ImageFileBuffer* buffer, _XSOCIAL_PREVIEWIMAGE* previewImage)
{
    buffer->mFormat = 0;
    buffer->mData = NULL;
    buffer->mSize = 0;
    if (!IsRenderThread())
        return;
    std::vector<unsigned char> pixels((size_t)mDevice->mWidth * mDevice->mHeight * 4);
    PcGraphics().Download(mDevice->mMainColourTexture, mDevice->mWidth, mDevice->mHeight, pixels.data());
    ConvertLinearToPng(buffer, pixels.data(), mDevice->mWidth, mDevice->mHeight);
    mDevice->Invalidate();
}

void Renderer::BeginConditionalSurvey(int survey)
{
}

void Renderer::EndConditionalSurvey()
{
}

void Renderer::BeginConditionalRendering(int survey)
{
}

void Renderer::EndConditionalRendering()
{
}

void Renderer::BeginOrderIndependentAlpha()
{
    Context& context = getContext();
    GpuState& gpu = context.mGpu;

    mDevice->mTarget = RenderTarget_OrderIndependentAlpha;

    if (IsRenderThread() && mScreen == 0)
    {
        mDevice->FlushPending(*this);
        mDevice->BindFramebuffer(mDevice->mAlphaFramebuffer);
        mDevice->ScreenState(gpu.mViewport, gpu.mScissor);
        mDevice->mAlphaInitialise = true;
        float quad[4] = {0.0f, 0.0f, 1.0f, 1.0f};
        mDevice->DrawScreenQuad(RenderVertexShader_Clear, RenderPixelShader_AlphaAccumulate, quad, 1.0f);
        mDevice->mAlphaInitialise = false;
    }

    SetGpuShaders(RenderVertexShader_Clear, RenderPixelShader_AlphaAccumulate);
    mCurrentVertexShader = -1;
    mCurrentPixelShader = -1;
    gpu.mColourMask = CHANNEL_MASK_RGBA;
    gpu.mBlendEnable = true;
    gpu.mBlendSource = BLEND_ONE;
    gpu.mBlendDestination = BLEND_ONE;
    gpu.mDepthTest = true;
    gpu.mDepthWrite = false;
    gpu.mDepthFunc = COMPARE_LEQUAL;
    gpu.mStencilFunc = COMPARE_ALWAYS;
    gpu.mFrontFaceCCW = true;
    gpu.mCullBack = true;
    gpu.mAlphaTestOn = false;
    gpu.mAlphaReference = 0.0f;
    context.mOrderIndependentAlpha = true;
}

void Renderer::EndOrderIndependentAlpha()
{
    Context& context = getContext();
    GpuState& gpu = context.mGpu;

    context.mOrderIndependentAlpha = false;
    mDevice->mTarget = RenderTarget_Main;

    if (IsRenderThread() && mScreen == 0)
    {
        mDevice->FlushPending(*this);
        mDevice->BindFramebuffer(mDevice->mMainFramebuffer);
        mDevice->ScreenState(gpu.mViewport, gpu.mScissor);
        mDevice->mDrawState.mBlendEnable = true;
        mDevice->mDrawState.mBlendSource = 7;
        mDevice->mDrawState.mBlendDestination = 6;
        mDevice->mDrawState.mColourMask = gpu.mColourMask;
        SDL_GPUSampler* pointSampler = mDevice->GetSampler(MakeSampler(true, false, MIP_FILTER_NONE));
        mDevice->BindTextureUnit(0, mDevice->mAlphaAccumulateTexture, pointSampler);
        mDevice->BindTextureUnit(1, mDevice->mAlphaWeightTexture, pointSampler);

        float quad[4];
        quad[0] = context.mViewportScale[2];
        quad[1] = context.mViewportScale[3];
        quad[2] = context.mViewportScale[0];
        quad[3] = context.mViewportScale[1];
        mDevice->DrawScreenQuad(RenderVertexShader_ScreenQuad, RenderPixelShader_AlphaResolve, quad, 1.0f);
    }

    SetGpuShaders(RenderVertexShader_ScreenQuad, RenderPixelShader_AlphaResolve);
    RestoreGpuStateFromContext();
    mCurrentPixelShader = -1;
    mCurrentVertexShader = -1;
}

void Renderer::ReleaseForeground()
{
    if (IsRenderThread())
    {
        PcGraphics().Submit(true);
    }

    if (mReservedMEM1 != NULL)
    {
        AlignedFree(mReservedMEM1);
        mReservedMEM1 = NULL;
    }

    mDisplayEnabled = false;
    mForegroundAcquired = false;
}

void Renderer::AcquiredForeground()
{
    mForegroundAcquired = true;
    Initialise(false);
}

void Renderer::WiiUWaitDrawDone()
{
    if (!mForegroundAcquired)
    {
        return;
    }

    if (IsRenderThread())
    {
        PcGraphics().Submit(true);
    }
}

int Renderer::GetFramesSinceBoot()
{
    return mFrameCount;
}

void Renderer::BeginEvent(const wchar_t* name)
{
}

void Renderer::EndEvent()
{
}

int Renderer::GetNumCommandBufferVertsDrawnLastFrame()
{
    return mCommandBufferVertsDrawnLastFrame;
}

int Renderer::GetNumVertsInCommandBuffer(int id)
{
    EnterCriticalSection(&mCommandBufferMutex);

    int slot = mCommandBufferSlots[id];
    int numVertices = 0;
    if (slot >= 0)
    {
        numVertices = mCommandBuffers[slot]->getNumVertices();
    }

    LeaveCriticalSection(&mCommandBufferMutex);
    return numVertices;
}

void Renderer::MatrixMode(int mode)
{
    Context& context = getContext();
    context.mMatrixMode = mode;
}

void Renderer::MatrixSetIdentity()
{
    Context& context = getContext();
    MatrixIdentity(context.mMatrixStack[context.mMatrixMode][context.mMatrixStackDepth[context.mMatrixMode]]);
    context.mMatrixDirty[context.mMatrixMode] = true;
}

void Renderer::MultWithStack(const float* matrix)
{
    Context& context = getContext();
    float* top = context.mMatrixStack[context.mMatrixMode][context.mMatrixStackDepth[context.mMatrixMode]];
    MatrixConcat(matrix, top, top);
    context.mMatrixDirty[context.mMatrixMode] = true;
}

void Renderer::MatrixTranslate(float x, float y, float z)
{
    float matrix[16];
    MatrixIdentity(matrix);
    matrix[12] = x;
    matrix[13] = y;
    matrix[14] = z;
    MultWithStack(matrix);
}

void Renderer::MatrixRotate(float angle, float x, float y, float z)
{
    float axis[3] = {x, y, z};
    NormalizeVector(axis);

    float s = sinf(angle);
    float c = cosf(angle);
    float t = 1.0f - c;
    float ax = axis[0];
    float ay = axis[1];
    float az = axis[2];

    float rotation[16];
    rotation[0] = c + t * ax * ax;
    rotation[1] = ay * (t * ax) - s * az;
    rotation[2] = az * (t * ax) + s * ay;
    rotation[3] = 0.0f;
    rotation[4] = ay * (t * ax) + s * az;
    rotation[5] = c + t * ay * ay;
    rotation[6] = az * (t * ay) - s * ax;
    rotation[7] = 0.0f;
    rotation[8] = az * (t * ax) - s * ay;
    rotation[9] = az * (t * ay) + s * ax;
    rotation[10] = c + t * az * az;
    rotation[11] = 0.0f;
    rotation[12] = 0.0f;
    rotation[13] = 0.0f;
    rotation[14] = 0.0f;
    rotation[15] = 1.0f;

    MatrixTranspose(rotation, rotation);
    MultWithStack(rotation);
}

void Renderer::MatrixScale(float x, float y, float z)
{
    float matrix[16];
    MatrixIdentity(matrix);
    matrix[0] = x;
    matrix[5] = y;
    matrix[10] = z;
    MultWithStack(matrix);
}

void Renderer::MatrixPerspective(float fieldOfView, float aspect, float nearDistance, float farDistance)
{
    float angle = fieldOfView * 0.5f * 0.01745329252f;
    float cotangent = 1.0f / tanf(angle);
    float depth = 1.0f / (farDistance - nearDistance);

    float matrix[16];
    memset(matrix, 0, sizeof(matrix));
    matrix[0] = cotangent / aspect;
    matrix[5] = cotangent;
    matrix[10] = -(farDistance + nearDistance) * depth;
    matrix[11] = -(2.0f * farDistance * nearDistance) * depth;
    matrix[14] = -1.0f;

    MatrixTranspose(matrix, matrix);
    MultWithStack(matrix);
}

void Renderer::MatrixOrthogonal(float left, float right, float bottom, float top, float nearDistance, float farDistance)
{
    float matrix[16];
    memset(matrix, 0, sizeof(matrix));

    float horizontal = 1.0f / (right - left);
    matrix[0] = 2.0f * horizontal;
    matrix[3] = -(right + left) * horizontal;
    float vertical = 1.0f / (top - bottom);
    matrix[5] = 2.0f * vertical;
    matrix[7] = -(top + bottom) * vertical;
    float depth = 1.0f / (farDistance - nearDistance);
    matrix[10] = -2.0f * depth;
    matrix[11] = -(farDistance + nearDistance) * depth;
    matrix[15] = 1.0f;

    MatrixTranspose(matrix, matrix);
    MultWithStack(matrix);
}

void Renderer::MatrixPop()
{
    Context& context = getContext();
    --context.mMatrixStackDepth[context.mMatrixMode];
    context.mMatrixDirty[context.mMatrixMode] = true;
}

void Renderer::MatrixPush()
{
    Context& context = getContext();
    int mode = context.mMatrixMode;
    memcpy(context.mMatrixStack[mode][context.mMatrixStackDepth[mode] + 1],
           context.mMatrixStack[mode][context.mMatrixStackDepth[mode]], sizeof(float) * 16);
    ++context.mMatrixStackDepth[mode];
}

void Renderer::MatrixMult(float* matrix)
{
    float local[16];
    memcpy(local, matrix, sizeof(local));
    MultWithStack(local);
}

float* Renderer::MatrixGet(int mode)
{
    Context& context = getContext();
    return context.mMatrixStack[mode][context.mMatrixStackDepth[mode]];
}

void Renderer::Set_matrixDirty()
{
    Context& context = getContext();

    MatrixIdentity34(context.mMatrixStack[0][0]);
    MatrixIdentity34(context.mMatrixStack[1][0]);
    MatrixIdentity34(context.mMatrixStack[3][0]);
    MatrixIdentity34(context.mMatrixStack[2][0]);

    context.mMatrixDirty[2] = true;
    mCurrentVertexShader = -1;
    context.mMatrixDirty[1] = true;
    mCurrentPixelShader = -1;
    context.mMatrixDirty[3] = true;
    context.mMatrixDirty[0] = true;
}

void* Renderer::MemoryAllocMEM1(unsigned int size, unsigned int alignment, bool bReserved)
{
    if (alignment < 4)
    {
        alignment = 4;
    }

    if (!bReserved)
    {
        return AlignedAllocate(size, alignment);
    }

    if (mReservedMEM1 == NULL)
    {
        mReservedMEM1 = AlignedAllocate(size, alignment);
    }

    return mReservedMEM1;
}

void Renderer::MemoryFreeMEM1(void* memory)
{
    if (memory != NULL && memory == mReservedMEM1)
    {
        mReservedMEM1 = NULL;
    }

    AlignedFree(memory);
}

void* Renderer::MemoryAllocMEM2(unsigned int size, unsigned int alignment)
{
    if (alignment < 4)
    {
        alignment = 4;
    }

    return AlignedAllocate(size, alignment);
}

void Renderer::MemoryFreeMEM2(void* memory)
{
    AlignedFree(memory);
}

void* Renderer::MemoryAllocBucket(unsigned int size, unsigned int alignment)
{
    return AlignedAllocate(size, alignment);
}

void Renderer::MemoryFreeBucket(void* memory)
{
    AlignedFree(memory);
}

void Renderer::StateSetColour(float red, float green, float blue, float alpha)
{
    Context& context = getContext();

    if (context.mRecordingBuffer != NULL)
    {
        if (context.mRecordingBuffer->mRecordState)
        {
            context.mRecordingBuffer->SetColor(red, green, blue, alpha);
        }
    }
    else
    {
        mPixelUniformsDirty = true;
        mPixelColours[0][2] = blue;
        mPixelColours[0][3] = alpha;
        mPixelColours[0][1] = green;
        mPixelColours[0][0] = red;
    }
}

void Renderer::StateSetColourAdd(float red, float green, float blue, float alpha)
{
    Context& context = getContext();

    if (context.mRecordingBuffer == NULL)
    {
        mPixelColours[3][0] = red;
        mPixelColours[3][1] = green;
        mPixelColours[3][2] = blue;
        mPixelColours[3][3] = alpha;
        mPixelUniformsDirty = true;
    }
}

void Renderer::StateSetTexColour(int stage, float red, float green, float blue, float alpha)
{
    Context& context = getContext();

    if (context.mRecordingBuffer == NULL && (unsigned int)stage < 4)
    {
        mPixelColours[4 + stage][0] = red;
        mPixelColours[4 + stage][1] = green;
        mPixelColours[4 + stage][2] = blue;
        mPixelColours[4 + stage][3] = alpha;
        mPixelUniformsDirty = true;
    }
}

void Renderer::StateSetDepthMask(bool bDepthMask)
{
    Context& context = getContext();

    if (context.mRecordingBuffer != NULL)
    {
        if (context.mRecordingBuffer->mRecordState)
        {
            context.mRecordingBuffer->SetDepthMask(bDepthMask);
        }
    }
    else
    {
        context.mDepthMask = bDepthMask;
        ApplyGpuDepthStencil();
    }
}

void Renderer::StateSetBlendEnable(bool bBlend)
{
    Context& context = getContext();

    if (context.mRecordingBuffer != NULL)
    {
        if (context.mRecordingBuffer->mRecordState)
        {
            context.mRecordingBuffer->SetBlendEnable(bBlend);
        }
    }
    else
    {
        context.mBlendEnable = bBlend;
        context.mGpu.mBlendEnable = bBlend;
    }
}

void Renderer::StateSetBlendFunc(int source, int destination)
{
    Context& context = getContext();

    if (context.mRecordingBuffer != NULL)
    {
        if (context.mRecordingBuffer->mRecordState)
        {
            context.mRecordingBuffer->SetBlendFunc(source, destination);
        }
    }
    else
    {
        context.mBlendSource = source;
        context.mBlendDestination = destination;
        context.mGpu.mBlendSource = source;
        context.mGpu.mBlendDestination = destination;
    }
}

void Renderer::StateSetBlendFactor(unsigned int factor)
{
    Context& context = getContext();

    if (context.mRecordingBuffer != NULL)
    {
        if (context.mRecordingBuffer->mRecordState)
        {
            context.mRecordingBuffer->SetBlendFactor(factor);
        }
    }
    else
    {
        context.mGpu.mBlendColour[0] = (float)(factor & 0xFF) / 255.0f;
        context.mGpu.mBlendColour[1] = (float)((factor >> 8) & 0xFF) / 255.0f;
        context.mGpu.mBlendColour[2] = (float)((factor >> 16) & 0xFF) / 255.0f;
        context.mGpu.mBlendColour[3] = (float)(factor >> 24) / 255.0f;
    }
}

void Renderer::StateSetAlphaFunc(int alphaFunc, float reference)
{
    Context& context = getContext();
    context.mAlphaTestRef = reference;
    ApplyGpuAlphaTest();
}

void Renderer::StateSetDepthFunc(int depthFunc)
{
    Context& context = getContext();

    if (context.mRecordingBuffer != NULL)
    {
        if (context.mRecordingBuffer->mRecordState)
        {
            context.mRecordingBuffer->SetDepthFunc(depthFunc);
        }
    }
    else
    {
        context.mDepthFunc = depthFunc;
        ApplyGpuDepthStencil();
    }
}

void Renderer::StateSetFaceCull(bool bFaceCull)
{
    Context& context = getContext();

    if (context.mRecordingBuffer != NULL)
    {
        if (context.mRecordingBuffer->mRecordState)
        {
            context.mRecordingBuffer->SetFaceCull(bFaceCull);
        }
    }
    else
    {
        context.mFaceCull = bFaceCull;
        ApplyGpuPolygonControl(true);
    }
}

void Renderer::StateSetFaceCullCW(bool bClockwise)
{
    Context& context = getContext();

    if (context.mRecordingBuffer == NULL)
    {
        ApplyGpuPolygonControl(bClockwise);
    }
}

void Renderer::StateSetLineWidth(float width)
{
}

void Renderer::StateSetWriteEnable(bool bRed, bool bGreen, bool bBlue, bool bAlpha)
{
    Context& context = getContext();
    context.mGpu.mColourMask = (unsigned char)((bRed ? 1 : 0) | (bGreen ? 2 : 0) | (bBlue ? 4 : 0) | (bAlpha ? 8 : 0));
}

void Renderer::StateSetDepthTestEnable(bool bDepthTest)
{
    Context& context = getContext();

    if (context.mRecordingBuffer != NULL)
    {
        if (context.mRecordingBuffer->mRecordState)
        {
            context.mRecordingBuffer->SetDepthTestEnable(bDepthTest);
        }
    }
    else
    {
        context.mDepthTestEnable = bDepthTest;
        ApplyGpuDepthStencil();
    }
}

void Renderer::StateSetAlphaTestEnable(bool bAlphaTest)
{
    Context& context = getContext();

    if (context.mRecordingBuffer == NULL)
    {
        context.mAlphaTestEnable = bAlphaTest;
        ApplyGpuAlphaTest();
    }
}

void Renderer::StateSetDepthSlopeAndBias(float slope, float bias)
{
    Context& context = getContext();

    if (context.mRecordingBuffer == NULL)
    {
        ApplyGpuPolygonControl(true);
        context.mGpu.mPolygonOffsetScale = slope;
        context.mGpu.mPolygonOffsetBias = bias;
    }
}

void Renderer::StateSetFogEnable(bool bFog)
{
    Context& context = getContext();
    context.mFogEnable = bFog;
}

void Renderer::StateSetFogMode(int mode)
{
    Context& context = getContext();
    context.mFogMode = mode;
}

void Renderer::StateSetFogNearDistance(float nearDistance)
{
    Context& context = getContext();
    context.mFogNearDistance = nearDistance;
}

void Renderer::StateSetFogFarDistance(float farDistance)
{
    Context& context = getContext();
    context.mFogFarDistance = farDistance;
}

void Renderer::StateSetFogDensity(float density)
{
    Context& context = getContext();
    context.mFogDensity = density;
}

void Renderer::StateSetFogColour(float red, float green, float blue)
{
    mPixelColours[1][0] = red;
    mPixelColours[1][1] = green;
    mPixelColours[1][2] = blue;
    mPixelColours[1][3] = 1.0f;
    mPixelUniformsDirty = true;
}

void Renderer::StateSetLightingEnable(bool bLighting)
{
    Context& context = getContext();

    if (context.mRecordingBuffer != NULL && context.mRecordingBuffer->mRecordState)
    {
        context.mRecordingBuffer->SetLightingEnable(bLighting);
        return;
    }

    context.mLightingEnable = bLighting;
}

void Renderer::StateSetVertexTextureUV(float u, float v)
{
    float uniform[4];
    uniform[0] = u - 1.0f;
    uniform[1] = v - 1.0f;
    uniform[2] = 0.0f;
    uniform[3] = 0.0f;
    SetVertexUniformReg(VERTEX_REG_VERTEX_TEXTURE_UV, 4, uniform);
}

void Renderer::StateSetTextureScaleUI(float scaleU, float scaleV, float offsetU, float offsetV)
{
    float uniform[4];
    uniform[0] = scaleU;
    uniform[1] = scaleV;
    uniform[2] = offsetU;
    uniform[3] = offsetV;
    SetVertexUniformReg(VERTEX_REG_UI_TEXTURE_SCALE, 4, uniform);
}

void Renderer::StateSetScalingGridUI(float left, float right, float top, float bottom, float innerLeft,
                                     float innerRight, float innerTop, float innerBottom, float scaleU, float scaleV)
{
    float edges[4];
    float inner[4];
    float scale[4];

    edges[0] = left;
    edges[1] = right;
    edges[2] = top;
    edges[3] = bottom;
    inner[0] = innerLeft;
    inner[1] = innerRight;
    inner[2] = innerTop;
    inner[3] = innerBottom;
    scale[0] = scaleU;
    scale[1] = scaleV;
    scale[2] = 0.0f;
    scale[3] = 0.0f;

    SetVertexUniformReg(VERTEX_REG_SCALING_GRID_X, 4, edges);
    SetVertexUniformReg(VERTEX_REG_SCALING_GRID_Y, 4, inner);
    SetVertexUniformReg(VERTEX_REG_SCALING_GRID_SCALE, 4, scale);
}

void Renderer::StateSetLightColour(int light, float red, float green, float blue)
{
    if (light >= 2)
    {
        return;
    }

    Context& context = getContext();

    if (context.mRecordingBuffer != NULL)
    {
        if (context.mRecordingBuffer->mRecordState)
        {
            context.mRecordingBuffer->SetLightColour(light, red, green, blue);
        }
    }
    else
    {
        context.mLightState[2 + light][0] = red;
        context.mLightState[2 + light][1] = green;
        context.mLightingDirty = true;
        context.mLightState[2 + light][3] = 1.0f;
        context.mLightState[2 + light][2] = blue;
    }
}

void Renderer::StateSetLightAmbientColour(float red, float green, float blue)
{
    Context& context = getContext();

    if (context.mRecordingBuffer != NULL)
    {
        if (context.mRecordingBuffer->mRecordState)
        {
            context.mRecordingBuffer->SetLightAmbientColour(red, green, blue);
        }
    }
    else
    {
        context.mLightState[4][0] = red;
        context.mLightState[4][2] = blue;
        context.mLightState[4][3] = 1.0f;
        context.mLightingDirty = true;
        context.mLightState[4][1] = green;
    }
}

void Renderer::StateSetLightDirection(int light, float x, float y, float z)
{
    if (light >= 2)
    {
        return;
    }

    Context& context = getContext();

    if (context.mRecordingBuffer != NULL)
    {
        if (context.mRecordingBuffer->mRecordState)
        {
            context.mRecordingBuffer->SetLightDirection(light, x, y, z);
        }
    }
    else
    {
        const float* modelView = context.mMatrixStack[0][context.mMatrixStackDepth[0]];
        float direction[3];
        for (int axis = 0; axis < 3; ++axis)
        {
            direction[axis] = modelView[0 * 4 + axis] * x + modelView[1 * 4 + axis] * y + modelView[2 * 4 + axis] * z;
        }
        NormalizeVector(direction);

        context.mLightState[light][0] = direction[0];
        context.mLightState[light][1] = direction[1];
        context.mLightState[light][2] = direction[2];
        context.mLightState[light][3] = 0.0f;
        context.mLightingDirty = true;
    }
}

void Renderer::StateSetLightEnable(int light, bool bEnable)
{
    if (light >= 2)
    {
        return;
    }

    Context& context = getContext();

    if (context.mRecordingBuffer != NULL)
    {
        if (context.mRecordingBuffer->mRecordState)
        {
            context.mRecordingBuffer->SetLightEnable(light, bEnable);
        }
    }
    else
    {
        context.mLightEnable[light] = bEnable;
        context.mLightingDirty = true;
    }
}

void Renderer::StateSetViewport(C4JRender::eViewportType viewport)
{
    Context& context = getContext();

    if (context.mRecordingBuffer != NULL)
    {
        return;
    }

    mCurrentViewport = viewport;

    float screenWidth = (float)mScreenWidth[mScreen];
    float screenHeight = (float)mScreenHeight[mScreen];
    float x = 0.0f;
    float y = 0.0f;
    float width = screenWidth;
    float height = screenHeight;

    switch (viewport)
    {
    case C4JRender::eViewportType_TopHalf:
        height = screenHeight * 0.5f;
        break;
    case C4JRender::eViewportType_BottomHalf:
        height = screenHeight * 0.5f;
        y = height;
        break;
    case C4JRender::eViewportType_LeftHalf:
        width = screenWidth * 0.5f;
        break;
    case C4JRender::eViewportType_RightHalf:
        width = screenWidth * 0.5f;
        x = width;
        break;
    case C4JRender::eViewportType_TopLeftQuarter:
        width = screenWidth * 0.5f;
        height = screenHeight * 0.5f;
        break;
    case C4JRender::eViewportType_TopRightQuarter:
        width = screenWidth * 0.5f;
        height = screenHeight * 0.5f;
        x = width;
        break;
    case C4JRender::eViewportType_BottomLeftQuarter:
        width = screenWidth * 0.5f;
        height = screenHeight * 0.5f;
        y = height;
        break;
    case C4JRender::eViewportType_BottomRightQuarter:
        width = screenWidth * 0.5f;
        height = screenHeight * 0.5f;
        x = width;
        y = height;
        break;
    default:
        break;
    }

    float layoutX;
    float layoutY;
    float layoutWidth;
    float layoutHeight;
    GetLayoutRect(mLayout, &layoutX, &layoutY, &layoutWidth, &layoutHeight);
    x = layoutX + x * layoutWidth / screenWidth;
    y = layoutY + y * layoutHeight / screenHeight;
    width = width * layoutWidth / screenWidth;
    height = height * layoutHeight / screenHeight;

    context.mViewportScale[0] = width / screenWidth;
    context.mViewportScale[1] = height / screenHeight;
    context.mViewportScale[2] = x / screenWidth;
    context.mViewportScale[3] = y / screenHeight;

    ApplyGpuViewportAndScissor(x, y, width, height);
    float scissorX0 = x < 0.0f ? 0.0f : x;
    float scissorY0 = y < 0.0f ? 0.0f : y;
    float scissorX1 = x + width > screenWidth ? screenWidth : x + width;
    float scissorY1 = y + height > screenHeight ? screenHeight : y + height;
    context.mGpu.mScissor[0] = (int)scissorX0;
    context.mGpu.mScissor[1] = (int)scissorY0;
    context.mGpu.mScissor[2] = (int)scissorX1 - (int)scissorX0;
    context.mGpu.mScissor[3] = (int)scissorY1 - (int)scissorY0;
}

void Renderer::StateGetScissorRect(unsigned int& left, unsigned int& top, unsigned int& right, unsigned int& bottom)
{
    Context& context = getContext();
    left = context.mScissorRect[0];
    top = context.mScissorRect[1];
    right = context.mScissorRect[2];
    bottom = context.mScissorRect[3];
}

void Renderer::StateSetScissorRect(unsigned int left, unsigned int top, unsigned int right, unsigned int bottom)
{
    Context& context = getContext();
    unsigned int width = right - left;
    unsigned int height = bottom - top;
    context.mScissorRect[0] = left;
    context.mScissorRect[1] = top;
    context.mScissorRect[2] = right;
    context.mScissorRect[3] = bottom;
    SetScreenScissor(left, top, width, height);
}

void Renderer::StateSetEnableViewportClipPlanes(bool bEnable)
{
}

void Renderer::StateSetTexGenCol(int coordinate, float x, float y, float z, float w, bool bEyeLinear)
{
    Context& context = getContext();

    if (context.mRecordingBuffer != NULL)
    {
        return;
    }

    float plane[4] = {x, y, z, w};

    if (bEyeLinear)
    {
        float modelView[16];
        memcpy(modelView, MatrixGet(0), sizeof(modelView));
        MatrixTranspose(modelView, modelView);

        float inverse[16];
        MatrixInverse(modelView, inverse);

        float transposed[16];
        MatrixTranspose(inverse, transposed);

        float transformed[4];
        for (int row = 0; row < 4; ++row)
        {
            transformed[row] = transposed[row * 4 + 0] * plane[0] + transposed[row * 4 + 1] * plane[1]
                               + transposed[row * 4 + 2] * plane[2] + transposed[row * 4 + 3] * plane[3];
        }

        memcpy(plane, transformed, sizeof(plane));
    }

    int matrix = bEyeLinear ? 0 : 1;
    context.mTexGenMatrix[matrix][0][coordinate] = plane[0];
    context.mTexGenMatrix[matrix][1][coordinate] = plane[1];
    context.mTexGenMatrix[matrix][2][coordinate] = plane[2];
    context.mTexGenMatrix[matrix][3][coordinate] = plane[3];

    int other = bEyeLinear ? 1 : 0;
    context.mTexGenMatrix[other][0][coordinate] = 0.0f;
    context.mTexGenMatrix[other][1][coordinate] = 0.0f;
    context.mTexGenMatrix[other][2][coordinate] = 0.0f;
    context.mTexGenMatrix[other][3][coordinate] = 0.0f;
}

void Renderer::StateSetStencil(int stencilFunc, unsigned char reference, unsigned char compareMask,
                               unsigned char writeMask)
{
    Context& context = getContext();
    context.mStencilFunc = stencilFunc;

    if (context.mRecordingBuffer == NULL)
    {
        ApplyGpuDepthStencil();
        context.mGpu.mStencilCompareMask = compareMask;
        context.mGpu.mStencilWriteMask = writeMask;
        context.mGpu.mStencilReference = reference;
    }
}

void Renderer::StateSetForceLOD(int lod)
{
    Context& context = getContext();

    if (context.mRecordingBuffer == NULL)
    {
        context.mForceLOD = lod;
    }
}

void Renderer::StateSetLerpColour(float red, float green, float blue, float alpha)
{
    Context& context = getContext();

    if (context.mRecordingBuffer == NULL)
    {
        mPixelColours[2][0] = red;
        mPixelColours[2][1] = green;
        mPixelColours[2][2] = blue;
        mPixelColours[2][3] = alpha;
        mPixelUniformsDirty = true;
    }
}

void Renderer::SetLayeredColour(int layer, float red, float green, float blue)
{
    Context& context = getContext();
    context.mLayeredColour[layer][0] = red;
    context.mLayeredColour[layer][1] = green;
    context.mLayeredColour[layer][2] = blue;
    context.mLayeredColour[layer][3] = 1.0f;
}

void Renderer::SetLayeredParam(int layer, int slot, float offset, float scale)
{
    Context& context = getContext();
    mLayeredParams[1 + layer][0] = context.mLayeredColour[slot][0];
    mLayeredParams[1 + layer][1] = context.mLayeredColour[slot][1];
    mLayeredParams[1 + layer][2] = context.mLayeredColour[slot][2];
    mLayeredParams[1 + layer][3] = offset + scale;
    mLayeredParamsDirty = true;
}

void Renderer::SetLayeredTextureSize(int width, int height)
{
    mLayeredParams[0][0] = (float)width;
    mLayeredParams[0][1] = (float)height;
    mLayeredParamsDirty = true;
}

void Renderer::StateSetBoneMatrixData(int bone)
{
    Context& context = getContext();
    memcpy(context.mBoneMatrix[bone], MatrixGet(0), sizeof(float) * 16);

    int count = bone + 1;
    if (count > context.mNumBoneMatrices)
    {
        context.mNumBoneMatrices = count;
    }
}

void Renderer::StateSetBoneMatrixNull(int bone)
{
    Context& context = getContext();
    memset(context.mBoneMatrix[bone], 0, sizeof(float) * 16);

    int count = bone + 1;
    if (count > context.mNumBoneMatrices)
    {
        context.mNumBoneMatrices = count;
    }
}

void Renderer::StateSetVertexAlphaDistance(float distance)
{
    Context& context = getContext();
    context.mVertexAlphaDistance = distance;
}

void Renderer::StateSetVertexAlpha(float alpha)
{
    Context& context = getContext();

    if (context.mVertexAlpha != alpha)
    {
        context.mVertexAlpha = alpha;
        SetVertexUniformReg(VERTEX_REG_VERTEX_ALPHA, 4, &context.mVertexAlpha);
    }
}

void Renderer::UpdateFogState()
{
    Context& context = getContext();

    float uniform[4];
    uniform[0] = 0.0f;
    uniform[1] = 0.0f;
    uniform[2] = 0.0f;
    uniform[3] = 0.0f;

    if (!context.mFogEnable)
    {
        SetVertexUniformReg(VERTEX_REG_FOG, 4, uniform);
        return;
    }

    if (context.mFogMode == 1)
    {
        uniform[0] = context.mFogFarDistance;
        uniform[1] = context.mFogNearDistance;
        uniform[2] = 1.0f;
        uniform[3] = context.mVertexAlphaDistance;
    }
    else
    {
        uniform[0] = context.mFogDensity;
        uniform[2] = context.mFogMode == 2 ? 2.0f : 3.0f;
        uniform[3] = context.mVertexAlphaDistance;
    }

    SetVertexUniformReg(VERTEX_REG_FOG, 4, uniform);
}

void Renderer::UpdateLightingState()
{
    Context& context = getContext();

    if (!context.mLightingDirty || !context.mLightingEnable)
    {
        return;
    }

    if (!context.mLightEnable[0])
    {
        memset(context.mLightState[0], 0, sizeof(context.mLightState[0]));
        memset(context.mLightState[2], 0, sizeof(context.mLightState[2]));
    }

    if (!context.mLightEnable[1])
    {
        memset(context.mLightState[1], 0, sizeof(context.mLightState[1]));
        memset(context.mLightState[3], 0, sizeof(context.mLightState[3]));
    }

    SetVertexUniformReg(VERTEX_REG_LIGHTING, 0x14, &context.mLightState[0][0]);
    context.mLightingDirty = false;
}

void Renderer::UpdateViewportState()
{
}

void Renderer::UpdateTexGenState()
{
    Context& context = getContext();
    SetVertexUniformReg(VERTEX_REG_TEXGEN_EYE, 0x10, &context.mTexGenMatrix[0][0][0]);
    SetVertexUniformReg(VERTEX_REG_TEXGEN_OBJECT, 0x10, &context.mTexGenMatrix[1][0][0]);
}

void Renderer::StateUpdate()
{
    Context& context = getContext();
    StateSetFaceCull(context.mFaceCull);
    StateSetDepthMask(context.mDepthMask);
    StateSetDepthTestEnable(context.mDepthTestEnable);
    StateSetAlphaTestEnable(context.mAlphaTestEnable);
}

int Renderer::GetMaxTextures()
{
    return TEXTURE_COUNT;
}

int Renderer::TextureCreate()
{
    EnterCriticalSection(&mTextureMutex);

    for (int texture = 0; texture < TEXTURE_COUNT; ++texture)
    {
        Texture& slot = mTextures[texture];

        if (!slot.mUsed)
        {
            slot.mHasStorage = false;
            slot.mWidth = 0;
            slot.mHeight = 0;
            slot.mStorageLevels = 1;
            slot.mFlags = 0;
            slot.mUsed = true;
            slot.mLevels = 1;
            slot.mFormat = C4JRender::eTextureFormat_RGBA8;
            slot.mSampler = MakeSampler(false, false, MIP_FILTER_POINT);
            ++slot.mGeneration;
            LeaveCriticalSection(&mTextureMutex);
            return texture;
        }
    }

    LeaveCriticalSection(&mTextureMutex);
    return -1;
}

void Renderer::TextureFree(int texture)
{
    bool bRenderThread = IsRenderThread();

    if (bRenderThread)
    {
        mDevice->FlushPending(*this);
    }

    EnterCriticalSection(&mTextureMutex);
    Texture& slot = mTextures[texture];

    if (slot.mName != 0)
    {
        if (bRenderThread)
        {
            PcGraphics().EndPass();
            SDL_ReleaseGPUTexture(PcGraphics().device, slot.mName);
        }
        else
        {
            Device::TextureOperation operation;
            operation.mType = Device::TextureOperation::eType_Delete;
            operation.mSlot = texture;
            operation.mGeneration = slot.mGeneration;
            operation.mLevel = 0;
            operation.mWidth = 0;
            operation.mHeight = 0;
            operation.mName = slot.mName;
            operation.mHasPixels = false;
            mDevice->mTextureOperations.push_back(operation);
        }
    }

    slot.mName = 0;
    slot.mHasStorage = false;
    slot.mUsed = false;
    ++slot.mGeneration;
    LeaveCriticalSection(&mTextureMutex);
}

void Renderer::TextureBind(int texture)
{
    TextureBind(0, texture);
}

void Renderer::UpdateTextureState(int stage)
{
    Context& context = getContext();
    int texture = context.mBoundTexture[stage];

    if (!mTextures[texture].mHasStorage)
    {
        return;
    }

    if (context.mTextureIsVertex[stage])
    {
        context.mGpu.mVertexTexture = texture;
        context.mGpu.mVertexSampler = mTextures[texture].mSampler;
    }
    else
    {
        context.mGpu.mPixelTexture[stage] = texture;
        context.mGpu.mPixelSampler[stage] = mTextures[texture].mSampler;
    }
}

void Renderer::TextureBind(int stage, int texture)
{
    if (texture == -1)
    {
        texture = mDefaultTexture;
    }

    Context& context = getContext();
    CommandBuffer* buffer = context.mRecordingBuffer;

    if (buffer != NULL && buffer->mRecordState)
    {
        buffer->BindTexture(texture);
    }

    context.mBoundTexture[stage] = texture;
    context.mTextureIsMask[stage] = false;
    context.mTextureIsVertex[stage] = false;
    UpdateTextureState(stage);
}

void Renderer::TextureBindMask(int stage)
{
    Context& context = getContext();
    context.mTextureIsMask[stage] = true;
}

void Renderer::TextureBindVertex(int texture)
{
    if (texture == -1)
    {
        texture = mDefaultTexture;
    }

    Context& context = getContext();
    context.mBoundTexture[0] = texture;
    context.mTextureIsMask[0] = false;
    context.mTextureIsVertex[0] = true;
    UpdateTextureState(0);
}

void Renderer::TextureSetTextureLevels(int levels)
{
    Context& context = getContext();
    mTextures[context.mBoundTexture[0]].mLevels = levels;
}

int Renderer::TextureGetTextureLevels()
{
    Context& context = getContext();
    return mTextures[context.mBoundTexture[0]].mLevels;
}

void Renderer::SubmitTextureStorage(int slot, const void* data)
{
    bool bRenderThread = IsRenderThread();

    if (bRenderThread)
    {
        mDevice->FlushPending(*this);
    }

    EnterCriticalSection(&mTextureMutex);
    Texture& texture = mTextures[slot];

    if (bRenderThread)
    {
        mDevice->ExecuteTextureStorage(texture, data);
    }
    else
    {
        Device::TextureOperation operation;
        operation.mType = Device::TextureOperation::eType_Storage;
        operation.mSlot = slot;
        operation.mGeneration = texture.mGeneration;
        operation.mLevel = 0;
        operation.mWidth = texture.mWidth;
        operation.mHeight = texture.mHeight;
        operation.mName = 0;
        operation.mHasPixels = data != NULL;
        if (data != NULL)
        {
            const unsigned char* bytes = static_cast<const unsigned char*>(data);
            operation.mPixels.assign(bytes, bytes + (size_t)texture.mWidth * texture.mHeight * 4);
        }
        mDevice->mTextureOperations.push_back(operation);
    }

    LeaveCriticalSection(&mTextureMutex);
}

void Renderer::SubmitTextureLevel(int slot, int level, int width, int height, const void* data)
{
    bool bRenderThread = IsRenderThread();

    if (bRenderThread)
    {
        mDevice->FlushPending(*this);
    }

    EnterCriticalSection(&mTextureMutex);
    Texture& texture = mTextures[slot];

    if (bRenderThread)
    {
        mDevice->ExecuteTextureLevel(texture, level, width, height, data);
    }
    else
    {
        Device::TextureOperation operation;
        operation.mType = Device::TextureOperation::eType_Level;
        operation.mSlot = slot;
        operation.mGeneration = texture.mGeneration;
        operation.mLevel = level;
        operation.mWidth = width;
        operation.mHeight = height;
        operation.mName = 0;
        operation.mHasPixels = true;
        const unsigned char* bytes = static_cast<const unsigned char*>(data);
        operation.mPixels.assign(bytes, bytes + (size_t)width * height * 4);
        mDevice->mTextureOperations.push_back(operation);
    }

    LeaveCriticalSection(&mTextureMutex);
}

void Renderer::TextureData(int width, int height, void* data, int level, C4JRender::eTextureFormat format)
{
    Context& context = getContext();
    int slot = context.mBoundTexture[0];
    Texture& texture = mTextures[slot];
    texture.mFormat = format;

    if (level == 0)
    {
        texture.mWidth = width;
        texture.mHeight = height;
        texture.mStorageLevels = ClampTextureLevels(texture.mLevels, width, height);
        texture.mHasStorage = width > 0 && height > 0;

        if (!texture.mHasStorage)
        {
            return;
        }

        SubmitTextureStorage(slot, data);
        return;
    }

    if (!texture.mHasStorage || texture.mStorageLevels <= 1 || data == NULL || level >= texture.mStorageLevels)
    {
        return;
    }

    int levelWidth = texture.mWidth >> level;
    int levelHeight = texture.mHeight >> level;

    if (levelWidth <= 0 || levelHeight <= 0)
    {
        return;
    }

    SubmitTextureLevel(slot, level, levelWidth, levelHeight, data);
}

void Renderer::TextureDataUpdate(int x, int y, int width, int height, void* data, int level)
{
    Context& context = getContext();
    int slot = context.mBoundTexture[0];
    Texture& texture = mTextures[slot];

    if (x != 0 || y != 0 || data == NULL)
    {
        return;
    }

    if (width != (texture.mWidth >> level) || height != (texture.mHeight >> level))
    {
        return;
    }

    if (!texture.mHasStorage)
    {
        return;
    }

    if (level != 0 && (texture.mStorageLevels <= 1 || level >= texture.mStorageLevels))
    {
        return;
    }

    if (width <= 0 || height <= 0)
    {
        return;
    }

    SubmitTextureLevel(slot, level, width, height, data);
}

void Renderer::TextureSetParam(int parameter, int value)
{
    Context& context = getContext();

    if (parameter == 0)
    {
        return;
    }

    Texture& texture = mTextures[context.mBoundTexture[0]];
    unsigned int flags = texture.mFlags;

    if (parameter == 1)
    {
        flags &= ~TEXTURE_FLAG_MIN_LINEAR;
        if (value == 1)
        {
            flags |= TEXTURE_FLAG_MIN_LINEAR;
        }
    }
    else if (parameter == 2)
    {
        flags &= ~TEXTURE_FLAG_MAG_LINEAR;
        if (value == 1)
        {
            flags |= TEXTURE_FLAG_MAG_LINEAR;
        }
    }
    else if (parameter == 3)
    {
        flags &= ~TEXTURE_FLAG_CLAMP_U;
        if (value == 0)
        {
            flags |= TEXTURE_FLAG_CLAMP_U;
        }
    }
    else if (parameter == 4)
    {
        flags &= ~TEXTURE_FLAG_CLAMP_V;
        if (value == 0)
        {
            flags |= TEXTURE_FLAG_CLAMP_V;
        }
    }

    texture.mFlags = flags;
    texture.mSampler.mClampU = (flags & TEXTURE_FLAG_CLAMP_U) != 0 ? 1 : 0;
    texture.mSampler.mClampV = (flags & TEXTURE_FLAG_CLAMP_V) != 0 ? 1 : 0;
    texture.mSampler.mMagLinear = (flags & TEXTURE_FLAG_MAG_LINEAR) != 0 ? 1 : 0;
    texture.mSampler.mMinLinear = (flags & TEXTURE_FLAG_MIN_LINEAR) != 0 ? 1 : 0;
    texture.mSampler.mMipFilter = MIP_FILTER_LINEAR;
    UpdateTextureState(0);
}

void Renderer::TextureDynamicUpdateStart()
{
}

void Renderer::TextureDynamicUpdateEnd()
{
}

int Renderer::LoadTextureData(const char* fileName, D3DXIMAGE_INFO* info, int** data)
{
    int file = CreateFileA(fileName, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    if (file == INVALID_HANDLE_VALUE)
    {
        return -1;
    }

    unsigned int fileSize = GetFileSize(file, NULL);
    unsigned char* fileData = static_cast<unsigned char*>(malloc(fileSize));
    unsigned int bytesRead = 0;
    ReadFile(file, fileData, fileSize, &bytesRead, NULL);
    CloseHandle(file);

    int result = LoadTextureData(fileData, bytesRead, info, data);
    free(fileData);
    return result;
}

int Renderer::LoadTextureData(unsigned char* fileData, unsigned int fileSize, D3DXIMAGE_INFO* info, int** data)
{
    png_image image;
    memset(&image, 0, sizeof(image));
    image.version = PNG_IMAGE_VERSION;

    png_image_begin_read_from_memory(&image, fileData, fileSize);

    if ((image.warning_or_error & 3) > 1)
    {
        png_image_free(&image);
        return -1;
    }

    image.format = PNG_FORMAT_ARGB;
    *data = (int*)new unsigned char[image.height * image.width * 4];

    if (*data != NULL && png_image_finish_read(&image, NULL, *data, 0, NULL))
    {
        info->Width = image.width;
        info->Height = image.height;
        return 0;
    }

    return -1;
}

int Renderer::SaveTextureData(const char* fileName, D3DXIMAGE_INFO* info, int* data)
{
    png_image image;
    memset(&image, 0, sizeof(image));
    image.version = PNG_IMAGE_VERSION;
    image.width = info->Width;
    image.height = info->Height;
    image.format = PNG_FORMAT_RGBA;
    png_image_write_to_file(&image, fileName, 0, data, 0, NULL);
    return 0;
}

int Renderer::SaveTextureDataToMemory(void* destination, int destinationSize, int* bytesWritten, int width, int height,
                                      int* data)
{
    png_image image;
    memset(&image, 0, sizeof(image));
    image.version = PNG_IMAGE_VERSION;
    image.width = width;
    image.height = height;
    image.format = PNG_FORMAT_RGBA;

    PngMemoryWriter writer;
    writer.mStart = static_cast<unsigned char*>(destination);
    writer.mCurrent = writer.mStart;
    writer.mEnd = writer.mStart + destinationSize;

    png_rw_ptr writeFunction = PngWriteToMemory;
    png_flush_ptr flushFunction = PngFlushMemory;
    png_image_write_to_stdio(&image, &writer, 0, data, 0, NULL, reinterpret_cast<void*>(writeFunction),
                             reinterpret_cast<void*>(flushFunction));
    *bytesWritten = (int)(writer.mCurrent - writer.mStart);
    return 0;
}

void Renderer::TextureGetStats()
{
}

C4JRender::ePixelShaderType Renderer::FinalisePixelShaderType(C4JRender::ePixelShaderType pixelShaderType)
{
    Context& context = getContext();

    if (pixelShaderType != C4JRender::ePixelShaderType_Default)
    {
        return pixelShaderType;
    }

    if (context.mOrderIndependentAlpha)
    {
        return C4JRender::ePixelShaderType_OrderIndependentAlpha;
    }

    if (context.mBoundTexture[1] == mDefaultTexture)
    {
        return C4JRender::ePixelShaderType_Default;
    }

    if (context.mBoundTexture[2] == mDefaultTexture)
    {
        if (context.mTextureIsMask[1])
        {
            return C4JRender::ePixelShaderType_LayeredColourMask;
        }

        return C4JRender::ePixelShaderType_DefaultTwoLayers;
    }

    if (context.mBoundTexture[3] == mDefaultTexture)
    {
        return C4JRender::ePixelShaderType_DefaultThreeLayers;
    }

    return C4JRender::ePixelShaderType_DefaultFourLayers;
}

bool Renderer::GetDynamicVertexMemory(C4JRender::eVertexType vertexType, int numVertices)
{
    Context& context = getContext();

    unsigned int used = context.mDynamicVertexOffset + ((VERTEX_STRIDES[vertexType] * numVertices + 0x1F) & ~0x1Fu);
    context.mDynamicVertexOffset = used;
    return used < DYNAMIC_VERTEX_BUDGET;
}

void Renderer::DrawVertexSetup(C4JRender::eVertexType vertexType, C4JRender::ePixelShaderType pixelShaderType,
                               C4JRender::ePrimitiveType primitiveType)
{
    Context& context = getContext();

    int vertexShader = vertexType;
    if (vertexType == C4JRender::eVertexType_Default && context.mLightingEnable)
    {
        vertexShader = C4JRender::eVertexType_DefaultLighting;
    }

    if (vertexShader != mCurrentVertexShader)
    {
        context.mGpu.mVertexShader = vertexShader;
        mCurrentVertexShader = vertexShader;
    }

    C4JRender::ePixelShaderType finalPixelShader = FinalisePixelShaderType(pixelShaderType);

    if (finalPixelShader == C4JRender::ePixelShaderType_LayeredColourMask
        && (mCurrentPixelShader != C4JRender::ePixelShaderType_LayeredColourMask || mLayeredParamsDirty))
    {
        SetPixelUniformReg(PIXEL_REG_LAYERED, 0x20, &mLayeredParams[0][0]);
        mLayeredParamsDirty = false;
    }

    if (finalPixelShader != mCurrentPixelShader || mPixelUniformsDirty)
    {
        SetPixelUniformReg(PIXEL_REG_COLOURS, 0x20, &mPixelColours[0][0]);
        mPixelUniformsDirty = false;
    }

    if (finalPixelShader != mCurrentPixelShader)
    {
        context.mGpu.mPixelShader = finalPixelShader;
        mCurrentPixelShader = finalPixelShader;
    }

    if (context.mMatrixDirty[0])
    {
        SetVertexUniformReg(VERTEX_REG_WORLD_VIEW, 0x10, MatrixGet(0));
        context.mMatrixDirty[0] = false;
    }

    if (context.mMatrixDirty[1])
    {
        SetVertexUniformReg(VERTEX_REG_PROJECTION, 0x10, MatrixGet(1));
        context.mMatrixDirty[1] = false;
    }

    if (context.mMatrixDirty[2])
    {
        SetVertexUniformReg(VERTEX_REG_TEXTURE, 0x10, MatrixGet(2));
        context.mMatrixDirty[2] = false;
    }

    float identity[16];
    MatrixIdentity(identity);
    SetVertexUniformReg(VERTEX_REG_WORLD_VIEW2, 0x10, identity);

    UpdateFogState();
    UpdateViewportState();
    UpdateLightingState();
    UpdateTexGenState();
}

void Renderer::DrawVertices(C4JRender::ePrimitiveType primitiveType, int numVertices, void* vertices,
                            C4JRender::eVertexType vertexType, C4JRender::ePixelShaderType pixelShaderType,
                            unsigned char* layers)
{
    Context& context = getContext();

    if (context.mRecordingBuffer != NULL)
    {
        int recordedVertexType = vertexType;
        if (vertexType == C4JRender::eVertexType_Default && context.mLightingEnable)
        {
            recordedVertexType = C4JRender::eVertexType_DefaultLighting;
        }

        context.mRecordingVertexType = (C4JRender::eVertexType)recordedVertexType;
        context.mRecordingPrimitiveType = primitiveType;
        context.mRecordingPixelShaderType = pixelShaderType;
        context.mRecordingBuffer->AddVertices(VERTEX_STRIDES[recordedVertexType], numVertices, vertices, context,
                                              layers);
        return;
    }

    if (!GetDynamicVertexMemory(vertexType, numVertices))
    {
        return;
    }

    DrawVertexSetup(vertexType, pixelShaderType, primitiveType);

    unsigned int size = numVertices * VERTEX_STRIDES[vertexType];
    StateUpdate();

    if (mDevice != NULL && numVertices > 0)
    {
        mDevice->DrawStream(*this, context, vertices, size, VERTEX_STRIDES[vertexType], LayoutForVertexType(vertexType),
                            primitiveType, (unsigned int)numVertices);
    }
}

void Renderer::CBuffLockStaticCreations()
{
    mAllowStaticCommandBufferIds = false;
}

int Renderer::CBuffCreate(int count)
{
    EnterCriticalSection(&mCommandBufferMutex);

    int first = mNextCommandBufferId;
    for (;;)
    {
        if (first + count > COMMAND_BUFFER_ID_COUNT)
        {
            LeaveCriticalSection(&mCommandBufferMutex);
            return -1;
        }

        bool bFree = true;
        for (int index = 0; index < count; ++index)
        {
            if (mCommandBufferSlots[first + index] != -1)
            {
                bFree = false;
                break;
            }
        }

        if (bFree)
        {
            break;
        }

        ++first;
    }

    for (int index = 0; index < count; ++index)
    {
        mCommandBufferSlots[first + index] = -2;
    }

    if (mAllowStaticCommandBufferIds)
    {
        mNextCommandBufferId = first + count;
    }

    LeaveCriticalSection(&mCommandBufferMutex);
    return first;
}

void Renderer::DeleteInternalBuffer(int slot)
{
    EnterCriticalSection(&mCommandBufferMutex);

    int retired = mNumRetiredCommandBuffers + 1;
    int retiredSlot = COMMAND_BUFFER_COUNT - retired;
    mCommandBuffers[retiredSlot] = mCommandBuffers[slot];
    mNumRetiredCommandBuffers = retired;
    memcpy(&mCommandBufferMatrices[retiredSlot * 16], &mCommandBufferMatrices[slot * 16], sizeof(float) * 16);

    int last = mNumCommandBuffers - 1;
    mNumCommandBuffers = last;

    if (last != 0)
    {
        mCommandBuffers[slot] = mCommandBuffers[last];
        memcpy(&mCommandBufferMatrices[slot * 16], &mCommandBufferMatrices[last * 16], sizeof(float) * 16);
        mCommandBufferVertexTypes[slot] = mCommandBufferVertexTypes[mNumCommandBuffers];
        mCommandBufferPixelShaderTypes[slot] = mCommandBufferPixelShaderTypes[mNumCommandBuffers];
        mCommandBufferPrimitiveTypes[slot] = mCommandBufferPrimitiveTypes[mNumCommandBuffers];
        int id = mCommandBufferIds[mNumCommandBuffers];
        mCommandBufferIds[slot] = id;
        mCommandBufferSlots[id] = slot;
    }

    LeaveCriticalSection(&mCommandBufferMutex);
}

void Renderer::NoteCommandBufferUse()
{
    const int used = mNumCommandBuffers + mNumRetiredCommandBuffers;
    if (used > mPeakCommandBuffers)
    {
        mPeakCommandBuffers = used;
        mPeakCommandBufferFrame = mFrameCount;
    }
    if (!mCommandBufferWarned && used * 4 > COMMAND_BUFFER_COUNT * 3)
    {
        mCommandBufferWarned = true;
        LOG_WARN(sLog, "Command buffers are over 75%% full: %d of %d (%d in use, %d retired) at render distance %d",
                 used, COMMAND_BUFFER_COUNT, mNumCommandBuffers, mNumRetiredCommandBuffers,
                 VideoSettings::GetRenderDistance());
    }
}

void Renderer::WriteCommandBufferReport(std::string& out)
{
    Renderer* renderer = gRendererInstance;
    if (renderer == NULL || renderer->mCommandBuffers == NULL)
    {
        out += "The renderer was not initialised.\n";
        return;
    }

    const int live = renderer->mNumCommandBuffers;
    const int retired = renderer->mNumRetiredCommandBuffers;
    int busy = 0;
    for (int index = 0; index < retired && index < COMMAND_BUFFER_COUNT; ++index)
    {
        CommandBuffer* buffer = renderer->mCommandBuffers[COMMAND_BUFFER_COUNT - 1 - index];
        if (buffer != NULL && buffer->IsBusy())
            ++busy;
    }

    char line[256];
    snprintf(line, sizeof(line), "Capacity: %d (sized for render distance %d)\n", COMMAND_BUFFER_COUNT,
             (int)VideoSettings::RENDER_DISTANCE_MAX);
    out += line;
    snprintf(line, sizeof(line), "Render distance: %d\n", VideoSettings::GetRenderDistance());
    out += line;
    snprintf(line, sizeof(line), "In use: %d, retired: %d (%d still busy), total %d\n", live, retired, busy,
             live + retired);
    out += line;
    snprintf(line, sizeof(line), "Peak: %d at frame %d; this is frame %d\n", renderer->mPeakCommandBuffers,
             renderer->mPeakCommandBufferFrame, renderer->mFrameCount);
    out += line;

    std::map<int, int> ranges;
    for (int index = 0; index < live && index < COMMAND_BUFFER_COUNT; ++index)
        ++ranges[renderer->mCommandBufferIds[index] >> 14];
    out += "In use by id range:";
    for (std::map<int, int>::iterator it = ranges.begin(); it != ranges.end(); ++it)
    {
        snprintf(line, sizeof(line), " %d:%d", it->first << 14, it->second);
        out += line;
    }
    out += "\n";
}

void Renderer::CBuffDelete(int firstId, int count)
{
    EnterCriticalSection(&mCommandBufferMutex);

    for (int index = 0; index < count; ++index)
    {
        if (mCommandBufferSlots[firstId + index] >= 0)
        {
            DeleteInternalBuffer(mCommandBufferSlots[firstId + index]);
        }

        mCommandBufferSlots[firstId + index] = -1;
    }

    LeaveCriticalSection(&mCommandBufferMutex);
}

void Renderer::CBuffStart(int id, bool bRecordState)
{
    Context& context = getContext();

    CommandBuffer* buffer = new CommandBuffer(bRecordState);
    context.mMatrixMode = 3;
    context.mRecordingBuffer = buffer;
    context.mMatrixStackDepth[3] = 0;
    context.mRecordingId = id;
    MatrixIdentity(context.mMatrixStack[3][0]);
    context.mMatrixDirty[3] = true;
}

void Renderer::CBuffClear(int id)
{
    EnterCriticalSection(&mCommandBufferMutex);

    if (mCommandBufferSlots[id] >= 0)
    {
        DeleteInternalBuffer(mCommandBufferSlots[id]);
        mCommandBufferSlots[id] = -2;
    }

    LeaveCriticalSection(&mCommandBufferMutex);
}

int Renderer::CBuffSize(int id)
{
    if (id == -1)
    {
        return sTotalCommandBufferBytes < 0 ? 0 : sTotalCommandBufferBytes;
    }

    EnterCriticalSection(&mCommandBufferMutex);

    int size = 0;
    int slot = mCommandBufferSlots[id];
    if (slot >= 0)
    {
        size = (int)mCommandBuffers[slot]->mVerticesCapacity;
    }

    LeaveCriticalSection(&mCommandBufferMutex);
    return size;
}

void Renderer::CBuffEnd()
{
    Context& context = getContext();

    EnterCriticalSection(&mCommandBufferMutex);

    if (context.mDeferredMode)
    {
        context.mDeferredBuffers.push_back(DeferredCBuff(
            context.mRecordingId, context.mRecordingBuffer, context.mRecordingVertexType,
            context.mRecordingPixelShaderType, context.mRecordingPrimitiveType, context.mMatrixStack[3][0]));
        context.mMatrixMode = 0;
        context.mRecordingBuffer->EndRecording(VERTEX_STRIDES[context.mRecordingVertexType]);
        context.mRecordingBuffer = NULL;
        LeaveCriticalSection(&mCommandBufferMutex);
        return;
    }

    if (mCommandBufferSlots[context.mRecordingId] >= 0)
    {
        DeleteInternalBuffer(mCommandBufferSlots[context.mRecordingId]);
    }

    if (mNumCommandBuffers + mNumRetiredCommandBuffers + 10 > COMMAND_BUFFER_COUNT)
    {
        FatalRendererError("Out of command buffers");
    }

    int slot = mNumCommandBuffers;
    mNumCommandBuffers = slot + 1;
    NoteCommandBufferUse();
    mCommandBufferIds[slot] = context.mRecordingId;
    mCommandBufferSlots[context.mRecordingId] = slot;
    mCommandBufferVertexTypes[slot] = (unsigned char)context.mRecordingVertexType;
    mCommandBufferPixelShaderTypes[slot] = context.mRecordingPixelShaderType;
    mCommandBufferPrimitiveTypes[slot] = (unsigned char)context.mRecordingPrimitiveType;
    mCommandBuffers[slot] = context.mRecordingBuffer;
    memcpy(&mCommandBufferMatrices[slot * 16], context.mMatrixStack[3][0], sizeof(float) * 16);

    context.mMatrixMode = 0;
    context.mRecordingBuffer->EndRecording(VERTEX_STRIDES[context.mRecordingVertexType]);
    context.mRecordingBuffer = NULL;
    LeaveCriticalSection(&mCommandBufferMutex);
}

bool Renderer::CBuffCall(int id, bool bApplyMatrix, unsigned char layerMask)
{
    EnterCriticalSection(&mCommandBufferMutex);

    int slot = mCommandBufferSlots[id];
    if (slot < 0)
    {
        LeaveCriticalSection(&mCommandBufferMutex);
        return false;
    }

    Context& context = getContext();
    int vertexType = mCommandBufferVertexTypes[slot];
    unsigned char primitiveType = mCommandBufferPrimitiveTypes[slot];

    if (!bApplyMatrix)
    {
        mCommandBuffers[slot]->Render((C4JRender::eVertexType)vertexType, context, primitiveType, false, layerMask);
        LeaveCriticalSection(&mCommandBufferMutex);
        return true;
    }

    if (context.mMatrixDirty[0])
    {
        SetVertexUniformReg(VERTEX_REG_WORLD_VIEW, 0x10, MatrixGet(0));
        context.mMatrixDirty[0] = false;
    }

    if (context.mMatrixDirty[1])
    {
        SetVertexUniformReg(VERTEX_REG_PROJECTION, 0x10, MatrixGet(1));
        context.mMatrixDirty[1] = false;
    }

    if (context.mMatrixDirty[2])
    {
        SetVertexUniformReg(VERTEX_REG_TEXTURE, 0x10, MatrixGet(2));
        context.mMatrixDirty[2] = false;
    }

    if (context.mNumBoneMatrices > 0)
    {
        SetVertexUniformReg(VERTEX_REG_BONES, (unsigned int)context.mNumBoneMatrices << 4, &context.mBoneMatrix[0][0]);
        context.mNumBoneMatrices = 0;
    }

    UpdateFogState();
    UpdateLightingState();

    if (vertexType == C4JRender::eVertexType_Default && context.mLightingEnable)
    {
        vertexType = C4JRender::eVertexType_DefaultLighting;
    }
    else if (vertexType == C4JRender::eVertexType_DefaultLighting && !context.mLightingEnable)
    {
        vertexType = C4JRender::eVertexType_Default;
    }
    else if (vertexType == C4JRender::eVertexType_Entity && context.mLightingEnable)
    {
        vertexType = C4JRender::eVertexType_EntityLighting;
    }
    else if (vertexType == C4JRender::eVertexType_EntityLighting && !context.mLightingEnable)
    {
        vertexType = C4JRender::eVertexType_Entity;
    }

    UpdateViewportState();
    UpdateTexGenState();

    if (vertexType != mCurrentVertexShader)
    {
        context.mGpu.mVertexShader = vertexType;
        mCurrentVertexShader = vertexType;
    }

    C4JRender::ePixelShaderType pixelShaderType = (C4JRender::ePixelShaderType)mCommandBufferPixelShaderTypes[slot];

    if (context.mForceLOD >= 0)
    {
        float forceLOD[4];
        forceLOD[0] = (float)context.mForceLOD;
        forceLOD[1] = 0.0f;
        forceLOD[2] = 0.0f;
        forceLOD[3] = 0.0f;
        pixelShaderType = C4JRender::ePixelShaderType_TerrainForceLod;
        SetPixelUniformReg(PIXEL_REG_FORCE_LOD, 4, forceLOD);
    }

    C4JRender::ePixelShaderType finalPixelShader = FinalisePixelShaderType(pixelShaderType);

    if (finalPixelShader != mCurrentPixelShader || mPixelUniformsDirty)
    {
        SetPixelUniformReg(PIXEL_REG_COLOURS, 0x20, &mPixelColours[0][0]);
        mPixelUniformsDirty = false;
    }

    if (finalPixelShader == C4JRender::ePixelShaderType_LayeredColourMask
        && (mCurrentPixelShader != C4JRender::ePixelShaderType_LayeredColourMask || mLayeredParamsDirty))
    {
        SetPixelUniformReg(PIXEL_REG_LAYERED, 0x20, &mLayeredParams[0][0]);
        mLayeredParamsDirty = false;
    }

    if (finalPixelShader != mCurrentPixelShader)
    {
        context.mGpu.mPixelShader = finalPixelShader;
        mCurrentPixelShader = finalPixelShader;
    }

    mCommandBuffers[slot]->Render((C4JRender::eVertexType)vertexType, context, primitiveType, bApplyMatrix, layerMask);

    MultWithStack(&mCommandBufferMatrices[slot * 16]);
    MatrixIdentity(context.mMatrixStack[3][0]);
    context.mMatrixDirty[3] = true;

    LeaveCriticalSection(&mCommandBufferMutex);
    return true;
}

void Renderer::CBuffTick()
{
    EnterCriticalSection(&mCommandBufferMutex);

    int remaining = mNumRetiredCommandBuffers;
    int examined = 0;
    int slot = COMMAND_BUFFER_COUNT - 1;

    while (examined < remaining)
    {
        CommandBuffer* buffer = mCommandBuffers[slot];

        if (buffer->IsBusy())
        {
            ++examined;
            --slot;
            continue;
        }

        delete buffer;
        remaining = mNumRetiredCommandBuffers - 1;
        mNumRetiredCommandBuffers = remaining;
        mCommandBuffers[slot] = NULL;

        if (remaining == 0)
        {
            break;
        }

        mCommandBuffers[slot] = mCommandBuffers[COMMAND_BUFFER_COUNT - 1 - remaining];
    }

    LeaveCriticalSection(&mCommandBufferMutex);
}

void Renderer::CBuffDeferredModeStart()
{
    Context& context = getContext();
    context.mDeferredMode = true;
}

void Renderer::CBuffDeferredModeEnd()
{
    Context& context = getContext();

    if (!context.mDeferredMode)
    {
        return;
    }

    EnterCriticalSection(&mCommandBufferMutex);
    context.mDeferredMode = false;

    for (unsigned int index = 0; index < context.mDeferredBuffers.size(); ++index)
    {
        const DeferredCBuff& deferred = context.mDeferredBuffers[index];

        if (mCommandBufferSlots[deferred.mId] >= 0)
        {
            DeleteInternalBuffer(mCommandBufferSlots[deferred.mId]);
        }

        if (mNumCommandBuffers + mNumRetiredCommandBuffers + 10 > COMMAND_BUFFER_COUNT)
        {
            FatalRendererError("Out of command buffers");
        }

        int slot = mNumCommandBuffers;
        mNumCommandBuffers = slot + 1;
        NoteCommandBufferUse();
        mCommandBufferIds[slot] = deferred.mId;
        mCommandBufferSlots[deferred.mId] = slot;
        mCommandBufferVertexTypes[slot] = (unsigned char)deferred.mVertexType;
        mCommandBufferPixelShaderTypes[slot] = deferred.mPixelShaderType;
        mCommandBufferPrimitiveTypes[slot] = (unsigned char)deferred.mPrimitiveType;
        mCommandBuffers[slot] = deferred.mBuffer;
        memcpy(&mCommandBufferMatrices[slot * 16], deferred.mMatrix, sizeof(float) * 16);
    }

    context.mDeferredBuffers.clear();
    LeaveCriticalSection(&mCommandBufferMutex);
}

Renderer::CommandBuffer::CommandBuffer(bool bRecordState)
{
    mRecordState = bRecordState;
    mVerticesUsed = 0;
    mVertices = static_cast<unsigned char*>(malloc(COMMAND_BUFFER_INITIAL_CAPACITY));
    mVerticesCapacity = COMMAND_BUFFER_INITIAL_CAPACITY;
    mHasVertices = true;
    mBuffer = 0;

    EnterCriticalSection(&sTotalAllocMutex);
    sTotalCommandBufferBytes += mVerticesCapacity;
    LeaveCriticalSection(&sTotalAllocMutex);

    mLastRenderedFrame = 0;
}

Renderer::CommandBuffer::~CommandBuffer()
{
    free(mVertices);

    Device* device = gRendererInstance->mDevice;
    if (device != NULL && mBuffer != NULL)
    {
        if (device->IsRenderThread())
        {
            PcGraphics().EndPass();
            SDL_ReleaseGPUBuffer(PcGraphics().device, mBuffer);
        }
        else
            device->QueueBufferDelete(mBuffer);
    }

    EnterCriticalSection(&sTotalAllocMutex);
    sTotalCommandBufferBytes -= mVerticesCapacity;
    LeaveCriticalSection(&sTotalAllocMutex);
}

void Renderer::CommandBuffer::StartRecording()
{
}

void Renderer::CommandBuffer::EndRecording(unsigned int vertexStride)
{
    if (mVerticesUsed != 0)
    {
        return;
    }

    free(mVertices);
    mVertices = NULL;
    mHasVertices = false;
}

void Renderer::CommandBuffer::Render(C4JRender::eVertexType vertexType, Context& context, int primitiveType,
                                     bool bApplyMatrix, unsigned char layerMask)
{
    if (!mHasVertices)
    {
        return;
    }

    Renderer* renderer = gRendererInstance;
    Device* device = renderer->mDevice;
    static thread_local std::vector<unsigned int> batchOffset;
    static thread_local std::vector<int> batchCount;
    batchOffset.clear();
    batchCount.clear();
    bool bMatrixPushed = false;
    int shaderVertexType = vertexType;

    for (unsigned int index = 0; index < mCommands.size(); ++index)
    {
        const Command& command = mCommands[index];

        if (command.mType == eCommandType_Matrix)
        {
            if (shaderVertexType == C4JRender::eVertexType_Terrain)
            {
                if (!bApplyMatrix && command.mMatrix[13] == context.mLastQuadUniform[2]
                    && command.mMatrix[14] == context.mLastQuadUniform[3])
                {
                    continue;
                }

                renderer->SetVertexUniformReg(VERTEX_REG_LIGHTING, 4, &command.mMatrix[12]);
                context.mLastQuadUniform[2] = command.mMatrix[13];
                context.mLastQuadUniform[3] = command.mMatrix[14];
            }
            else
            {
                renderer->SetVertexUniformReg(VERTEX_REG_WORLD_VIEW2, 0x10, command.mMatrix);
                bMatrixPushed = true;
            }
        }
        else if (command.mType == eCommandType_Vertices)
        {
            if (mRecordState)
            {
                renderer->UpdateLightingState();

                if (shaderVertexType == C4JRender::eVertexType_Default && context.mLightingEnable)
                {
                    shaderVertexType = C4JRender::eVertexType_DefaultLighting;
                }
                else if (shaderVertexType == C4JRender::eVertexType_DefaultLighting && !context.mLightingEnable)
                {
                    shaderVertexType = C4JRender::eVertexType_Default;
                }
                else if (shaderVertexType == C4JRender::eVertexType_Entity && context.mLightingEnable)
                {
                    shaderVertexType = C4JRender::eVertexType_EntityLighting;
                }
                else if (shaderVertexType == C4JRender::eVertexType_EntityLighting && !context.mLightingEnable)
                {
                    shaderVertexType = C4JRender::eVertexType_Entity;
                }

                if (shaderVertexType != renderer->mCurrentVertexShader)
                {
                    context.mGpu.mVertexShader = shaderVertexType;
                    renderer->mCurrentVertexShader = shaderVertexType;
                }
            }

            unsigned int stride = VERTEX_STRIDES[shaderVertexType];

            if (shaderVertexType == C4JRender::eVertexType_Terrain)
            {
                if (!batchOffset.empty()
                    && command.mVertices.mOffset
                           == batchOffset.back()
                                  + (unsigned int)batchCount.back() * VERTEX_STRIDES[C4JRender::eVertexType_Terrain])
                {
                    batchCount.back() += (int)command.mVertices.mCount;
                }
                else
                {
                    batchOffset.push_back(command.mVertices.mOffset);
                    batchCount.push_back((int)command.mVertices.mCount);
                }
            }
            else
            {
                if (device != NULL && device->PrepareDraw(*renderer, context))
                {
                    int layout = LayoutForVertexType(shaderVertexType);
                    if (EnsureUploaded(device, layout))
                    {
                        device->BindVertexBuffer(mBuffer, LayoutForVertexType(shaderVertexType));
                        device->DrawRange(primitiveType, command.mVertices.mOffset / stride, command.mVertices.mCount);
                    }
                }

                renderer->mCommandBufferVertsDrawn += command.mVertices.mCount;
            }
        }
    }

    if (!batchOffset.empty())
    {
        unsigned int stride = VERTEX_STRIDES[shaderVertexType];
        bool bDraw = device != NULL && device->PrepareDraw(*renderer, context)
                     && EnsureUploaded(device, LayoutForVertexType(shaderVertexType));

        if (bDraw)
        {
            device->BindVertexBuffer(mBuffer, LayoutForVertexType(shaderVertexType));
        }

        for (size_t batch = 0; batch < batchOffset.size(); ++batch)
        {
            if (bDraw)
            {
                device->DrawRange(primitiveType, batchOffset[batch] / stride, (unsigned int)batchCount[batch]);
            }

            renderer->mCommandBufferVertsDrawn += batchCount[batch];
        }
    }

    if (bMatrixPushed)
    {
        float identity[16];
        MatrixIdentity(identity);
        renderer->SetVertexUniformReg(VERTEX_REG_WORLD_VIEW2, 0x10, identity);
    }

    mLastRenderedFrame = renderer->mFrameCount;
}

bool Renderer::CommandBuffer::EnsureUploaded(Device* device, int layout)
{
    if (mBuffer == 0)
    {
        if (mVertices == NULL || mVerticesUsed == 0)
        {
            return false;
        }

        mBuffer = PcGraphics().Buffer(mVerticesUsed, SDL_GPU_BUFFERUSAGE_VERTEX);
        PcGraphics().Upload(mBuffer, 0, mVertices, mVerticesUsed);
        free(mVertices);
        mVertices = NULL;
    }

    return true;
}

bool Renderer::CommandBuffer::IsBusy()
{
    return mLastRenderedFrame >= gRendererInstance->mFrameCount - 3;
}

void Renderer::CommandBuffer::AddMatrix(const float* matrix)
{
    Command command;
    command.mType = eCommandType_Matrix;
    memcpy(command.mMatrix, matrix, sizeof(command.mMatrix));
    mCommands.push_back(command);
}

void Renderer::CommandBuffer::AddVertices(unsigned int vertexStride, unsigned int numVertices, void* vertices,
                                          Context& context, unsigned char* layers)
{
    if (context.mMatrixDirty[3])
    {
        AddMatrix(gRendererInstance->MatrixGet(3));
        context.mMatrixDirty[3] = false;
    }

    Command command;
    unsigned int offset = mVerticesUsed;
    command.mType = eCommandType_Vertices;
    mVerticesUsed = offset + numVertices * vertexStride;
    command.mVertices.mOffset = offset;
    command.mVertices.mCount = numVertices;

    if (mVerticesCapacity < mVerticesUsed)
    {
        EnterCriticalSection(&sTotalAllocMutex);
        sTotalCommandBufferBytes -= mVerticesCapacity;
        LeaveCriticalSection(&sTotalAllocMutex);

        mVerticesCapacity = (mVerticesUsed + 0xFFF) & ~0xFFFu;
        mVertices = static_cast<unsigned char*>(realloc(mVertices, mVerticesCapacity));

        EnterCriticalSection(&sTotalAllocMutex);
        sTotalCommandBufferBytes += mVerticesCapacity;
        LeaveCriticalSection(&sTotalAllocMutex);
    }

    if (layers == NULL)
    {
        memcpy(mVertices + offset, vertices, numVertices * vertexStride);
        command.mVertices.mLayer = 0;
        mCommands.push_back(command);
        return;
    }

    unsigned int quadSize = vertexStride * 4;
    unsigned int destination = offset;

    for (unsigned char layer = 0; layer < 7; ++layer)
    {
        unsigned int count = 0;

        for (unsigned int quad = 0; quad < numVertices / 4; ++quad)
        {
            if (layers[quad] == layer)
            {
                memcpy(mVertices + destination, static_cast<unsigned char*>(vertices) + quad * quadSize, quadSize);
                count += 4;
                destination += quadSize;
            }
        }

        if (count > 0)
        {
            command.mVertices.mLayer = layer;
            command.mVertices.mOffset = offset;
            command.mVertices.mCount = count;
            mCommands.push_back(command);
        }

        offset = destination;
    }
}

void Renderer::CommandBuffer::BindTexture(int texture)
{
    Command command;
    command.mType = eCommandType_BindTexture;
    command.mTexture = texture;
    mCommands.push_back(command);
}

void Renderer::CommandBuffer::SetColor(float red, float green, float blue, float alpha)
{
    Command command;
    command.mType = eCommandType_Colour;
    command.mColour[0] = red;
    command.mColour[1] = green;
    command.mColour[2] = blue;
    command.mColour[3] = alpha;
    mCommands.push_back(command);
}

void Renderer::CommandBuffer::SetDepthFunc(int depthFunc)
{
    Command command;
    command.mType = eCommandType_DepthFunc;
    command.mDepthFunc = depthFunc;
    mCommands.push_back(command);
}

void Renderer::CommandBuffer::SetDepthMask(bool bDepthMask)
{
    Command command;
    command.mType = eCommandType_DepthMask;
    command.mEnable = bDepthMask;
    mCommands.push_back(command);
}

void Renderer::CommandBuffer::SetDepthTestEnable(bool bDepthTest)
{
    Command command;
    command.mType = eCommandType_DepthTestEnable;
    command.mEnable = bDepthTest;
    mCommands.push_back(command);
}

void Renderer::CommandBuffer::SetLightingEnable(bool bLighting)
{
    Command command;
    command.mType = eCommandType_LightingEnable;
    command.mEnable = bLighting;
    mCommands.push_back(command);
}

void Renderer::CommandBuffer::SetLightEnable(int light, bool bEnable)
{
    Command command;
    command.mType = eCommandType_LightEnable;
    command.mLightEnable.mIndex = light;
    command.mLightEnable.mEnable = bEnable;
    mCommands.push_back(command);
}

void Renderer::CommandBuffer::SetLightDirection(int light, float x, float y, float z)
{
    Command command;
    command.mType = eCommandType_LightDirection;
    command.mLight.mIndex = light;

    Context& context = gRendererInstance->getContext();
    const float* matrix = context.mMatrixStack[3][context.mMatrixStackDepth[3]];
    float direction[3];
    for (int axis = 0; axis < 3; ++axis)
    {
        direction[axis] = matrix[0 * 4 + axis] * x + matrix[1 * 4 + axis] * y + matrix[2 * 4 + axis] * z;
    }
    NormalizeVector(direction);

    command.mLight.mValue[0] = direction[0];
    command.mLight.mValue[1] = direction[1];
    command.mLight.mValue[2] = direction[2];
    command.mLight.mValue[3] = 0.0f;
    mCommands.push_back(command);
}

void Renderer::CommandBuffer::SetLightColour(int light, float red, float green, float blue)
{
    Command command;
    command.mType = eCommandType_LightColour;
    command.mLight.mIndex = light;
    command.mLight.mValue[0] = red;
    command.mLight.mValue[1] = green;
    command.mLight.mValue[2] = blue;
    command.mLight.mValue[3] = 0.0f;
    mCommands.push_back(command);
}

void Renderer::CommandBuffer::SetLightAmbientColour(float red, float green, float blue)
{
    Command command;
    command.mType = eCommandType_LightAmbientColour;
    command.mLight.mIndex = 0;
    command.mLight.mValue[0] = red;
    command.mLight.mValue[1] = green;
    command.mLight.mValue[2] = blue;
    command.mLight.mValue[3] = 0.0f;
    mCommands.push_back(command);
}

void Renderer::CommandBuffer::SetBlendEnable(bool bBlend)
{
    Command command;
    command.mType = eCommandType_BlendEnable;
    command.mEnable = bBlend;
    mCommands.push_back(command);
}

void Renderer::CommandBuffer::SetBlendFunc(int source, int destination)
{
    Command command;
    command.mType = eCommandType_BlendFunc;
    command.mBlendFunc.mSource = source;
    command.mBlendFunc.mDestination = destination;
    mCommands.push_back(command);
}

void Renderer::CommandBuffer::SetBlendFactor(unsigned int factor)
{
    Command command;
    command.mType = eCommandType_BlendFactor;
    command.mBlendFactor = factor;
    mCommands.push_back(command);
}

void Renderer::CommandBuffer::SetFaceCull(bool bFaceCull)
{
    Command command;
    command.mType = eCommandType_FaceCull;
    command.mEnable = bFaceCull;
    mCommands.push_back(command);
}

int Renderer::CommandBuffer::getNumVertices()
{
    if (!mHasVertices)
    {
        return 0;
    }

    int numVertices = 0;

    for (unsigned int index = 0; index < mCommands.size(); ++index)
    {
        if (mCommands[index].mType == eCommandType_Vertices)
        {
            numVertices += (int)mCommands[index].mVertices.mCount;
        }
    }

    return numVertices;
}

const char* PcGpuName()
{
    return sGpuName.c_str();
}
