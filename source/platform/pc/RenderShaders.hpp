#ifndef PLATFORM_PC_RENDERSHADERS_HPP
#define PLATFORM_PC_RENDERSHADERS_HPP

#include <string>

enum RenderVertexShader
{
    RenderVertexShader_Standard,
    RenderVertexShader_Terrain,
    RenderVertexShader_StandardLit,
    RenderVertexShader_TexGen,
    RenderVertexShader_UI,
    RenderVertexShader_ScalingGridUI,
    RenderVertexShader_TiledUI,
    RenderVertexShader_ColouredUI,
    RenderVertexShader_Entity,
    RenderVertexShader_EntityLit,
    RenderVertexShader_ScreenQuad,
    RenderVertexShader_Clear,
    RenderVertexShader_Count
};

enum RenderPixelShader
{
    RenderPixelShader_Default,
    RenderPixelShader_DefaultTwoLayers,
    RenderPixelShader_DefaultThreeLayers,
    RenderPixelShader_DefaultFourLayers,
    RenderPixelShader_Terrain,
    RenderPixelShader_TerrainForceLod,
    RenderPixelShader_OrderIndependentAlpha,
    RenderPixelShader_LayeredColourMask,
    RenderPixelShader_UIColourAdd,
    RenderPixelShader_UI,
    RenderPixelShader_UIRadial,
    RenderPixelShader_EntityChangeColour,
    RenderPixelShader_EntityEmissiveAlpha,
    RenderPixelShader_EntityEmissiveAlphaOnly,
    RenderPixelShader_Copy,
    RenderPixelShader_Gamma,
    RenderPixelShader_AlphaResolve,
    RenderPixelShader_AlphaAccumulate,
    RenderPixelShader_Count
};

static const int RENDER_PIXEL_CONSTANT_COUNT = 32;
static const int RENDER_PIXEL_CONSTANT_ALPHA_REFERENCE = 31;
static const int RENDER_LIGHT_TEXTURE_UNIT = 4;
static const int RENDER_VERTEX_CONSTANT_BINDING = 0;
static const int RENDER_PIXEL_CONSTANT_BINDING = 1;

int RenderVertexConstantCount(int vertexShader);
std::string RenderVertexShaderSource(int vertexShader);
std::string RenderPixelShaderSource(int pixelShader);

#endif
