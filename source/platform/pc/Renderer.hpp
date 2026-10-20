#ifndef PLATFORM_PC_RENDERER_HPP
#define PLATFORM_PC_RENDERER_HPP

#include "game/client/render/C4JRender.hpp"
#include "game/core/CriticalSection.hpp"

#include <string>

class Renderer : public C4JRender
{
public:
    Renderer();
    virtual ~Renderer();

    virtual void Tick();
    virtual void UpdateGamma(unsigned short gamma);
    virtual void MatrixMode(int mode);
    virtual void MatrixSetIdentity();
    virtual void MatrixTranslate(float x, float y, float z);
    virtual void MatrixRotate(float angle, float x, float y, float z);
    virtual void MatrixScale(float x, float y, float z);
    virtual void MatrixPerspective(float fieldOfView, float aspect, float nearDistance, float farDistance);
    virtual void MatrixOrthogonal(float left, float right, float bottom, float top, float nearDistance,
                                  float farDistance);
    virtual void MatrixPop();
    virtual void MatrixPush();
    virtual void MatrixMult(float* matrix);
    virtual float* MatrixGet(int mode);
    virtual void Set_matrixDirty();
    virtual void Initialise();
    virtual void InitialiseContext();
    virtual void StartFrame();
    virtual void TEMP_PatchStateFromIggy();
    virtual void ResetRenderState();
    virtual void StartDRCFrame(bool bCopyFrame);
    virtual void DoScreenGrabOnNextPresent();
    virtual void Present();
    virtual void Clear(int flags);
    virtual void SetClearColour(const float* colour);
    virtual bool IsWidescreen();
    virtual bool IsHiDef();
    virtual void CaptureThumbnail(ImageFileBuffer* buffer);
    virtual void CaptureScreen(ImageFileBuffer* buffer, _XSOCIAL_PREVIEWIMAGE* previewImage);
    virtual void BeginConditionalSurvey(int survey);
    virtual void EndConditionalSurvey();
    virtual void BeginConditionalRendering(int survey);
    virtual void EndConditionalRendering();
    virtual void BeginOrderIndependentAlpha();
    virtual void EndOrderIndependentAlpha();
    virtual void ReleaseForeground();
    virtual void AcquiredForeground();
    virtual void WiiUWaitDrawDone();
    virtual int GetFramesSinceBoot();
    virtual void DrawVertices(ePrimitiveType primitiveType, int numVertices, void* vertices, eVertexType vertexType,
                              ePixelShaderType pixelShaderType, unsigned char* layers);
    virtual void CBuffLockStaticCreations();
    virtual int CBuffCreate(int count);
    virtual void CBuffDelete(int firstId, int count);
    virtual void CBuffStart(int id, bool bApplyLighting);
    virtual void CBuffClear(int id);
    virtual int CBuffSize(int id);
    virtual void CBuffEnd();
    virtual bool CBuffCall(int id, bool bApplyMatrix, unsigned char layerMask);
    virtual void CBuffTick();
    virtual void CBuffDeferredModeStart();
    virtual void CBuffDeferredModeEnd();
    virtual int GetMaxTextures();
    virtual int TextureCreate();
    virtual void TextureFree(int texture);
    virtual void TextureBind(int texture);
    virtual void TextureBind(int stage, int texture);
    virtual void TextureBindMask(int stage);
    virtual void TextureBindVertex(int texture);
    virtual void TextureSetTextureLevels(int levels);
    virtual int TextureGetTextureLevels();
    virtual void TextureData(int width, int height, void* data, int level, eTextureFormat format);
    virtual void TextureDataUpdate(int x, int y, int width, int height, void* data, int level);
    virtual void TextureSetParam(int parameter, int value);
    virtual void TextureDynamicUpdateStart();
    virtual void TextureDynamicUpdateEnd();
    virtual int LoadTextureData(const char* fileName, D3DXIMAGE_INFO* info, int** data);
    virtual int LoadTextureData(unsigned char* fileData, unsigned int fileSize, D3DXIMAGE_INFO* info, int** data);
    virtual int SaveTextureData(const char* fileName, D3DXIMAGE_INFO* info, int* data);
    virtual int SaveTextureDataToMemory(void* destination, int destinationSize, int* bytesWritten, int width,
                                        int height, int* data);
    virtual void TextureGetStats();
    virtual void StateSetColour(float red, float green, float blue, float alpha);
    virtual void StateSetColourAdd(float red, float green, float blue, float alpha);
    virtual void StateSetTexColour(int stage, float red, float green, float blue, float alpha);
    virtual void StateSetDepthMask(bool bDepthMask);
    virtual void StateSetBlendEnable(bool bBlend);
    virtual void StateSetBlendFunc(int source, int destination);
    virtual void StateSetBlendFactor(unsigned int factor);
    virtual void StateSetAlphaFunc(int alphaFunc, float reference);
    virtual void StateSetDepthFunc(int depthFunc);
    virtual void StateSetFaceCull(bool bFaceCull);
    virtual void StateSetFaceCullCW(bool bClockwise);
    virtual void StateSetLineWidth(float width);
    virtual void StateSetWriteEnable(bool bRed, bool bGreen, bool bBlue, bool bAlpha);
    virtual void StateSetDepthTestEnable(bool bDepthTest);
    virtual void StateSetAlphaTestEnable(bool bAlphaTest);
    virtual void StateSetDepthSlopeAndBias(float slope, float bias);
    virtual void StateSetFogEnable(bool bFog);
    virtual void StateSetFogMode(int mode);
    virtual void StateSetFogNearDistance(float nearDistance);
    virtual void StateSetFogFarDistance(float farDistance);
    virtual void StateSetFogDensity(float density);
    virtual void StateSetFogColour(float red, float green, float blue);
    virtual void StateSetLightingEnable(bool bLighting);
    virtual void StateSetVertexTextureUV(float u, float v);
    virtual void StateSetTextureScaleUI(float scaleU, float scaleV, float offsetU, float offsetV);
    virtual void StateSetScalingGridUI(float left, float right, float top, float bottom, float innerLeft,
                                       float innerRight, float innerTop, float innerBottom, float scaleU,
                                       float scaleV);
    virtual void StateSetLightColour(int light, float red, float green, float blue);
    virtual void StateSetLightAmbientColour(float red, float green, float blue);
    virtual void StateSetLightDirection(int light, float x, float y, float z);
    virtual void StateSetLightEnable(int light, bool bEnable);
    virtual void StateSetViewport(eViewportType viewport);
    virtual void StateGetScissorRect(unsigned int& left, unsigned int& top, unsigned int& right,
                                     unsigned int& bottom);
    virtual void StateSetScissorRect(unsigned int left, unsigned int top, unsigned int right, unsigned int bottom);
    virtual void StateSetEnableViewportClipPlanes(bool bEnable);
    virtual void StateSetTexGenCol(int coordinate, float x, float y, float z, float w, bool bEyeLinear);
    virtual void StateSetStencil(int stencilFunc, unsigned char reference, unsigned char compareMask,
                                 unsigned char writeMask);
    virtual void StateSetForceLOD(int lod);
    virtual void StateSetLerpColour(float red, float green, float blue, float alpha);
    virtual void SetLayeredColour(int layer, float red, float green, float blue);
    virtual void SetLayeredParam(int layer, int slot, float offset, float scale);
    virtual void SetLayeredTextureSize(int width, int height);
    virtual void StateSetBoneMatrixData(int bone);
    virtual void StateSetBoneMatrixNull(int bone);
    virtual void StateSetVertexAlphaDistance(float distance);
    virtual void StateSetVertexAlpha(float alpha);
    virtual int GetNumCommandBufferVertsDrawnLastFrame();
    virtual int GetNumVertsInCommandBuffer(int id);
    virtual void BeginEvent(const wchar_t* name);
    virtual void EndEvent();
    virtual void* MemoryAllocMEM1(unsigned int size, unsigned int alignment, bool bReserved);
    virtual void MemoryFreeMEM1(void* memory);
    virtual void* MemoryAllocMEM2(unsigned int size, unsigned int alignment);
    virtual void MemoryFreeMEM2(void* memory);
    virtual void* MemoryAllocBucket(unsigned int size, unsigned int alignment);
    virtual void MemoryFreeBucket(void* memory);

    enum ELayout
    {
        ELayout_Screen,
        ELayout_Interface,
        ELayout_InterfaceBackground
    };
    void SetLayout(ELayout layout);
    ELayout GetLayout() const { return mLayout; }
    float GetScreenAspect();
    void GetInterfaceArea(float* left, float* top, float* width, float* height);

private:
    struct Texture;
    struct GpuState;
    struct Context;
    struct DeferredCBuff;
    class CommandBuffer;
    class Device;

    friend class CommandBuffer;

    void Initialise(bool bFirstTime);
    void InitialiseContext(bool bReset);
    Context& getContext();
    void StartFrameInternal(int screen, bool bAlphaTest);
    void GammaCorrect();
    void ConvertLinearToPng(ImageFileBuffer* buffer, unsigned char* pixels, unsigned int width, unsigned int height);
    void MultWithStack(const float* matrix);
    void UpdateFogState();
    void UpdateLightingState();
    void UpdateViewportState();
    void UpdateTexGenState();
    void StateUpdate();
    void UpdateTextureState(int stage);
    ePixelShaderType FinalisePixelShaderType(ePixelShaderType pixelShaderType);
    bool GetDynamicVertexMemory(eVertexType vertexType, int numVertices);
    void DrawVertexSetup(eVertexType vertexType, ePixelShaderType pixelShaderType, ePrimitiveType primitiveType);
    void DeleteInternalBuffer(int slot);
    void NoteCommandBufferUse();
    static void WriteCommandBufferReport(std::string& out);
    void SetVertexUniformReg(unsigned int offset, unsigned int count, const float* values);
    void SetPixelUniformReg(unsigned int offset, unsigned int count, const float* values);
    void SetGpuShaders(int vertexShader, int pixelShader);
    void RestoreGpuStateFromContext();
    void ApplyGpuDepthStencil();
    void ApplyGpuPolygonControl(bool bFrontFaceCCW);
    void ApplyGpuAlphaTest();
    void ApplyGpuViewportAndScissor(float x, float y, float width, float height);
    void SetScreenScissor(unsigned int left, unsigned int top, unsigned int width, unsigned int height);
    void SubmitTextureStorage(int slot, const void* data);
    void SubmitTextureLevel(int slot, int level, int width, int height, const void* data);
    void ResizeTargets();
    void GetLayoutRect(ELayout layout, float* x, float* y, float* width, float* height);
    bool IsGlThread();

    float mClearColour[4];
    bool mDRCFrameStarted;
    bool mDisplayEnabled;
    bool mForegroundAcquired;
    bool mInitialised;
    Device* mDevice;
    int mDefaultTexture;
    unsigned short mGamma;
    int mFrameCount;
    bool mCapturingThumbnail;
    C4JCriticalSection mCommandBufferMutex;
    C4JCriticalSection mTextureMutex;
    int mCurrentVertexShader;
    int mCurrentPixelShader;
    eViewportType mCurrentViewport;
    ELayout mLayout;
    bool mTextureStateDirty;
    Texture* mTextures;
    int mScreenWidth[2];
    int mScreenHeight[2];
    int mScreen;
    bool mHiDef;
    bool mAllowStaticCommandBufferIds;
    int mNextCommandBufferId;
    int* mCommandBufferSlots;
    CommandBuffer** mCommandBuffers;
    float* mCommandBufferMatrices;
    int* mCommandBufferIds;
    unsigned char* mCommandBufferVertexTypes;
    int* mCommandBufferPixelShaderTypes;
    unsigned char* mCommandBufferPrimitiveTypes;
    int mNumCommandBuffers;
    int mNumRetiredCommandBuffers;
    int mPeakCommandBuffers;
    int mPeakCommandBufferFrame;
    bool mCommandBufferWarned;
    int mCommandBufferVertsDrawn;
    int mCommandBufferVertsDrawnLastFrame;
    void* mReservedMEM1;
    bool mScreenGrabRequested;
    float mPixelColours[8][4];
    bool mPixelUniformsDirty;
    float mLayeredParams[8][4];
    bool mLayeredParamsDirty;
};

extern Renderer* gRenderer;
extern Renderer* gRendererInstance;

const char* PcGpuName();

#endif
