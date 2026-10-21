#ifndef PLATFORM_PC_SHADERPACK_SHADERADAPTERS_HPP
#define PLATFORM_PC_SHADERPACK_SHADERADAPTERS_HPP

#include <string>

#include "platform/pc/shaderpack/GlslTranslator.hpp"

enum ShaderAdapter
{
    ShaderAdapter_Terrain,
    ShaderAdapter_Standard,
    ShaderAdapter_TexGen,
    ShaderAdapter_Entity,
    ShaderAdapter_Quad,
    ShaderAdapter_Count
};

enum ShaderTextureUnit
{
    ShaderTextureUnit_Albedo = 0,
    ShaderTextureUnit_Lightmap = 4,
    ShaderTextureUnit_Normals = 5,
    ShaderTextureUnit_Specular = 6,
    ShaderTextureUnit_TerrainVertices = 7,
    ShaderTextureUnit_QuadData = 8,
    ShaderTextureUnit_BlockMap = 9,
    ShaderTextureUnit_FirstPack = 10
};

const char* ShaderAdapterName(ShaderAdapter adapter);
std::string ShaderAdapterHeader(ShaderAdapter adapter, ShaderStage stage, const GlslUsage& usage);
std::string ShaderAdapterFooter(ShaderAdapter adapter, ShaderStage stage, const std::string& fragmentOutput0);

#endif
