#include "platform/pc/shaderpack/ShaderAdapters.hpp"

namespace
{
int VertexConstantCount(ShaderAdapter adapter)
{
    switch (adapter)
    {
    case ShaderAdapter_Terrain:
        return 41;
    case ShaderAdapter_Standard:
        return 18;
    case ShaderAdapter_TexGen:
        return 31;
    case ShaderAdapter_Entity:
        return 41 + 48 * 4;
    default:
        return 0;
    }
}

const char* const kCommonUniforms = R"(uniform mat4 lce_ProjectionFix;
uniform mat4 lce_CameraView;
uniform mat4 lce_CameraViewInverse;
uniform vec2 lce_AlphaTest;
uniform float lce_FlipY;
uniform vec4 lce_FogColorValue;
uniform vec4 lce_FogValues;
uniform vec4 lce_ColourScale;
struct lce_FogParameters
{
    vec4 color;
    float density;
    float start;
    float end;
    float scale;
};
const mat4 lce_LightmapMatrix = mat4(0.00390625, 0.0, 0.0, 0.0, 0.0, 0.00390625, 0.0, 0.0, 0.0, 0.0, 0.00390625, 0.0,
                                     0.03125, 0.03125, 0.03125, 1.0);
const int lce_MaxTextureCoords = 8;
)";

const char* const kGameConstants = R"(#define lce_MAT_WV mat4(uVC[0], uVC[1], uVC[2], uVC[3])
#define lce_MAT_WV2 mat4(uVC[4], uVC[5], uVC[6], uVC[7])
#define lce_MAT_P mat4(uVC[8], uVC[9], uVC[10], uVC[11])
#define lce_MAT_TEX mat4(uVC[12], uVC[13], uVC[14], uVC[15])
lce_FogParameters lce_FogValue()
{
    vec4 fog = uVC[17];
    float start = abs(fog.y);
    float end = abs(fog.x);
    float range = end - start;
    return lce_FogParameters(vec4(uPC[1].rgb, 1.0), abs(fog.x), start, end, range == 0.0 ? 0.0 : 1.0 / range);
}
#define lce_Fog lce_FogValue()
#define lce_ProjectionMatrix (lce_ProjectionFix * lce_MAT_P)
)";

const char* const kQuadConstants = R"(lce_FogParameters lce_FogValue()
{
    return lce_FogParameters(lce_FogColorValue, lce_FogValues.x, lce_FogValues.y, lce_FogValues.z, lce_FogValues.w);
}
#define lce_Fog lce_FogValue()
#define lce_ModelViewMatrix mat4(1.0)
#define lce_ProjectionMatrix mat4(2.0, 0.0, 0.0, 0.0, 0.0, 2.0, 0.0, 0.0, 0.0, 0.0, -2.0, 0.0, -1.0, -1.0, -1.0, 1.0)
)";

const char* const kDerivedMatrices = R"(#define lce_ModelViewProjectionMatrix (lce_ProjectionMatrix * lce_ModelViewMatrix)
#define lce_ModelViewMatrixTranspose transpose(lce_ModelViewMatrix)
#define lce_ModelViewMatrixInverseTranspose transpose(lce_ModelViewMatrixInverse)
#define lce_ProjectionMatrixInverse inverse(lce_ProjectionMatrix)
#define lce_ProjectionMatrixTranspose transpose(lce_ProjectionMatrix)
#define lce_ProjectionMatrixInverseTranspose transpose(lce_ProjectionMatrixInverse)
#define lce_ModelViewProjectionMatrixInverse inverse(lce_ModelViewProjectionMatrix)
#define lce_ModelViewProjectionMatrixTranspose transpose(lce_ModelViewProjectionMatrix)
#define lce_ModelViewProjectionMatrixInverseTranspose transpose(lce_ModelViewProjectionMatrixInverse)
#define lce_TextureMatrix lce_TextureMatrixArray()
#define lce_TextureMatrixInverse lce_TextureMatrixInverseArray()
#define lce_TextureMatrixTranspose lce_TextureMatrixTransposeArray()
#define lce_TextureMatrixInverseTranspose lce_TextureMatrixInverseTransposeArray()
mat4[8] lce_TextureMatrixInverseArray()
{
    mat4 matrices[8] = lce_TextureMatrixArray();
    for (int index = 0; index < 8; ++index)
        matrices[index] = inverse(matrices[index]);
    return matrices;
}
mat4[8] lce_TextureMatrixTransposeArray()
{
    mat4 matrices[8] = lce_TextureMatrixArray();
    for (int index = 0; index < 8; ++index)
        matrices[index] = transpose(matrices[index]);
    return matrices;
}
mat4[8] lce_TextureMatrixInverseTransposeArray()
{
    mat4 matrices[8] = lce_TextureMatrixArray();
    for (int index = 0; index < 8; ++index)
        matrices[index] = transpose(inverse(matrices[index]));
    return matrices;
}
)";

const char* const kLightUniforms = R"(struct lce_LightSourceParameters
{
    vec4 ambient;
    vec4 diffuse;
    vec4 specular;
    vec4 position;
    vec4 halfVector;
    vec3 spotDirection;
    float spotExponent;
    float spotCutoff;
    float spotCosCutoff;
    float constantAttenuation;
    float linearAttenuation;
    float quadraticAttenuation;
};
uniform lce_LightSourceParameters lce_LightSource[2];
struct lce_LightModelParameters
{
    vec4 ambient;
};
uniform lce_LightModelParameters lce_LightModel;
)";

const char* const kMaterial = R"(struct lce_MaterialParameters
{
    vec4 emission;
    vec4 ambient;
    vec4 diffuse;
    vec4 specular;
    float shininess;
};
const lce_MaterialParameters lce_FrontMaterial = lce_MaterialParameters(vec4(0.0, 0.0, 0.0, 1.0),
    vec4(0.2, 0.2, 0.2, 1.0), vec4(0.8, 0.8, 0.8, 1.0), vec4(0.0, 0.0, 0.0, 1.0), 0.0);
#define lce_BackMaterial lce_FrontMaterial
)";

const char* const kVertexGlobals = R"(vec4 lce_Vertex;
vec4 lce_Color;
vec4 lce_SecondaryColor;
vec3 lce_Normal;
float lce_FogCoord;
vec4 lce_MultiTexCoord0;
vec4 lce_MultiTexCoord1;
vec4 lce_MultiTexCoord2;
vec4 lce_MultiTexCoord3;
vec4 lce_MultiTexCoord4;
vec4 lce_MultiTexCoord5;
vec4 lce_MultiTexCoord6;
vec4 lce_MultiTexCoord7;
vec4 lce_EntityData;
vec4 lce_MidTexCoordData;
vec4 lce_TangentData;
vec4 lce_VelocityData;
vec4 lce_MidBlockData;
vec4 lce_ClipVertex;
uniform int lce_FirstVertex;
uniform int lce_QuadVertexMode;
int lce_QuadBase()
{
    return lce_FirstVertex + ((gl_VertexID - lce_FirstVertex) & ~3);
}
vec4 lce_ftransform()
{
    return lce_ProjectionMatrix * (lce_ModelViewMatrix * lce_Vertex);
}
void lce_SetupDefaults()
{
    lce_SecondaryColor = vec4(0.0);
    lce_FogCoord = 0.0;
    lce_MultiTexCoord2 = vec4(0.0, 0.0, 0.0, 1.0);
    lce_MultiTexCoord3 = vec4(0.0, 0.0, 0.0, 1.0);
    lce_MultiTexCoord4 = vec4(0.0, 0.0, 0.0, 1.0);
    lce_MultiTexCoord5 = vec4(0.0, 0.0, 0.0, 1.0);
    lce_MultiTexCoord6 = vec4(0.0, 0.0, 0.0, 1.0);
    lce_MultiTexCoord7 = vec4(0.0, 0.0, 0.0, 1.0);
    lce_EntityData = vec4(-1.0, 0.0, 0.0, 1.0);
    lce_VelocityData = vec4(0.0);
    lce_MidBlockData = vec4(0.0);
    lce_ClipVertex = vec4(0.0);
}
vec4 lce_FallbackTangent(vec3 normal)
{
    vec3 axis = abs(normal.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
    return vec4(normalize(cross(axis, normal)), 1.0);
}
)";

const char* const kTerrainVertex = R"(layout(location = 0) in ivec4 lce_aPosition;
layout(location = 1) in ivec4 lce_aTexture;
uniform isamplerBuffer lce_TerrainVertices;
uniform usamplerBuffer lce_QuadData;
uniform isamplerBuffer lce_BlockMap;
uniform int lce_QuadDataMode;
vec3 lce_TerrainCorner(int vertex)
{
    ivec4 position = texelFetch(lce_TerrainVertices, vertex * 2);
    ivec4 data = texelFetch(lce_TerrainVertices, vertex * 2 + 1);
    return vec3(position.xyz) * (1.0 / 1024.0) + vec3(float(data.w), 0.0, 0.0);
}
vec2 lce_TerrainCornerUV(int vertex)
{
    return vec2(texelFetch(lce_TerrainVertices, vertex * 2 + 1).xy) * (1.0 / 8192.0);
}
void lce_SetupVertex()
{
    lce_SetupDefaults();
    vec3 position = vec3(lce_aPosition.xyz) * (1.0 / 1024.0) + vec3(float(lce_aTexture.w), uVC[18].y, uVC[18].z);
    lce_Vertex = vec4(position, 1.0);

    int packedColour = lce_aPosition.w & 0xFFFF;
    vec3 colour = vec3(float((packedColour >> 10) & 31), float((packedColour >> 5) & 31), float(packedColour & 31))
                  * (1.0 / 31.0);
    float alpha = 1.0;
    if ((packedColour & 0x8000) == 0)
    {
        vec4 eye = lce_MAT_WV * lce_Vertex;
        float fadeDistance = abs(uVC[17].w);
        float fadeScale = fadeDistance == 0.0 ? 0.0 : 1.0 / fadeDistance;
        float fade = clamp((fadeDistance - length(eye.xyz)) * fadeScale, 0.0, 1.0);
        alpha = (1.0 - uVC[40].x) * (1.0 - fade) + uVC[40].x;
    }
    lce_Color = vec4(colour * lce_ColourScale.rgb, alpha * lce_ColourScale.a);

    vec2 uv = vec2(lce_aTexture.xy) * (1.0 / 8192.0);
    lce_MultiTexCoord0 = vec4(uv, 0.0, 1.0);
    int light = lce_aTexture.z & 0xFFFF;
#ifdef LCE_SEPARATE_AO
    lce_Color.a = float(((light >> 6) & 12) | (light & 3)) * (1.0 / 15.0) * lce_ColourScale.a;
    light &= 0xFCFC;
#endif
    lce_MultiTexCoord1 = vec4(float(light >> 8), float(light & 0xFF), 0.0, 1.0);

    int base = lce_QuadBase();
    vec3 p0 = lce_TerrainCorner(base);
    vec3 p1 = lce_TerrainCorner(base + 1);
    vec3 p2 = lce_TerrainCorner(base + 2);
    vec3 p3 = lce_TerrainCorner(base + 3);
    vec3 normal = cross(p2 - p0, p3 - p1);
    float normalLength = length(normal);
    normal = normalLength > 0.0 ? normal / normalLength : vec3(0.0, 1.0, 0.0);
    lce_Normal = normal;
    vec3 local = vec3(lce_aPosition.xyz) * (1.0 / 1024.0) + vec3(float(lce_aTexture.w), 0.0, 0.0);
    vec3 blockCenter = floor((p0 + p1 + p2 + p3) * 0.25 - normal * 0.01) + 0.5;
    lce_MidBlockData = vec4((blockCenter - local) * 64.0, 0.0);

    vec2 uv0 = lce_TerrainCornerUV(base);
    vec2 uv1 = lce_TerrainCornerUV(base + 1);
    vec2 uv2 = lce_TerrainCornerUV(base + 2);
    vec2 uv3 = lce_TerrainCornerUV(base + 3);
    lce_MidTexCoordData = vec4((uv0 + uv1 + uv2 + uv3) * 0.25, 0.0, 1.0);

    vec3 edge1 = p1 - p0;
    vec3 edge2 = p2 - p0;
    vec2 delta1 = uv1 - uv0;
    vec2 delta2 = uv2 - uv0;
    float determinant = delta1.x * delta2.y - delta2.x * delta1.y;
    if (abs(determinant) > 1.0e-12)
    {
        vec3 tangent = (edge1 * delta2.y - edge2 * delta1.y) / determinant;
        vec3 bitangent = (edge2 * delta1.x - edge1 * delta2.x) / determinant;
        tangent = tangent - normal * dot(normal, tangent);
        float tangentLength = length(tangent);
        if (tangentLength > 0.0)
        {
            tangent /= tangentLength;
            float handedness = dot(cross(normal, tangent), bitangent) < 0.0 ? -1.0 : 1.0;
            lce_TangentData = vec4(tangent, handedness);
        }
        else
        {
            lce_TangentData = lce_FallbackTangent(normal);
        }
    }
    else
    {
        lce_TangentData = lce_FallbackTangent(normal);
    }

    if (lce_QuadDataMode != 0)
    {
        uint element = texelFetch(lce_QuadData, gl_VertexID >> 2).r;
        int id = texelFetch(lce_BlockMap, int(element)).r;
        lce_EntityData = vec4(float(id), 0.0, 0.0, 1.0);
    }
}
)";

const char* const kStandardAttributes = R"(layout(location = 0) in vec3 lce_aPosition;
layout(location = 1) in vec4 lce_aColour;
layout(location = 2) in ivec4 lce_aNormal;
layout(location = 3) in vec2 lce_aTexCoord;
layout(location = 4) in ivec2 lce_aLight;
uniform isamplerBuffer lce_TerrainVertices;
bool lce_HasQuadTangent;
vec3 lce_QuadTangent;
float lce_QuadHandedness;
bool lce_HasQuadNormal;
vec3 lce_QuadNormal;
float lce_PackedFloat(int low, int high)
{
    return intBitsToFloat((low & 0xFFFF) | (high << 16));
}
vec3 lce_StandardCorner(int vertex, out vec2 uv)
{
    ivec4 first = texelFetch(lce_TerrainVertices, vertex * 4);
    ivec4 second = texelFetch(lce_TerrainVertices, vertex * 4 + 1);
    ivec4 third = texelFetch(lce_TerrainVertices, vertex * 4 + 2);
    uv = vec2(lce_PackedFloat(second.z, second.w), lce_PackedFloat(third.x, third.y));
    return vec3(lce_PackedFloat(first.x, first.y), lce_PackedFloat(first.z, first.w),
                lce_PackedFloat(second.x, second.y));
}
void lce_SetupQuad()
{
    lce_HasQuadTangent = false;
    lce_HasQuadNormal = false;
    if (lce_QuadVertexMode == 0)
        return;
    int base = lce_QuadBase();
    vec2 uv0;
    vec2 uv1;
    vec2 uv2;
    vec2 uv3;
    vec3 p0 = lce_StandardCorner(base, uv0);
    vec3 p1 = lce_StandardCorner(base + 1, uv1);
    vec3 p2 = lce_StandardCorner(base + 2, uv2);
    vec3 p3 = lce_StandardCorner(base + 3, uv3);
    vec3 faceNormal = cross(p2 - p0, p3 - p1);
    if (dot(faceNormal, faceNormal) > 0.0)
    {
        lce_QuadNormal = normalize(faceNormal);
        lce_HasQuadNormal = true;
    }
    vec4 mid = vec4((uv0 + uv1 + uv2 + uv3) * 0.25, 0.0, 1.0);
    if ((lce_MAT_TEX * mid).x > 1.0)
        mid -= inverse(lce_MAT_TEX) * vec4(1.0, 0.0, 0.0, 0.0);
    lce_MidTexCoordData = mid;

    vec3 edge1 = p1 - p0;
    vec3 edge2 = p2 - p0;
    vec2 delta1 = uv1 - uv0;
    vec2 delta2 = uv2 - uv0;
    float determinant = delta1.x * delta2.y - delta2.x * delta1.y;
    if (abs(determinant) <= 1.0e-12)
        return;
    vec3 tangent = (edge1 * delta2.y - edge2 * delta1.y) / determinant;
    vec3 bitangent = (edge2 * delta1.x - edge1 * delta2.x) / determinant;
    if (dot(tangent, tangent) <= 0.0)
        return;
    lce_QuadTangent = normalize(tangent);
    lce_QuadHandedness = dot(cross(faceNormal, tangent), bitangent) < 0.0 ? -1.0 : 1.0;
    lce_HasQuadTangent = true;
}
vec4 lce_QuadTangentData(vec3 normal)
{
    if (lce_HasQuadTangent)
    {
        vec3 tangent = lce_QuadTangent - normal * dot(normal, lce_QuadTangent);
        if (dot(tangent, tangent) > 0.0)
            return vec4(normalize(tangent), lce_QuadHandedness);
    }
    return lce_FallbackTangent(normal);
}
void lce_SetupStandard()
{
    lce_SetupDefaults();
    lce_Color = lce_aColour.wzyx * lce_ColourScale;
    lce_MultiTexCoord0 = vec4(lce_aTexCoord, 0.0, 1.0);
    if ((lce_MAT_TEX * lce_MultiTexCoord0).x > 1.0)
        lce_MultiTexCoord0 -= inverse(lce_MAT_TEX) * vec4(1.0, 0.0, 0.0, 0.0);
    lce_MultiTexCoord1 = vec4(fract(max(vec2(lce_aLight.yx) * (1.0 / 256.0), uVC[16].xy)) * 256.0, 0.0, 1.0);
    lce_MidTexCoordData = lce_MultiTexCoord0;
    lce_SetupQuad();
}
vec3 lce_AttributeNormal()
{
    vec3 normal = max(vec3(lce_aNormal.xyz) * (1.0 / 127.0), vec3(-1.0));
    if (dot(normal, normal) > 0.0)
        return normal;
    return lce_HasQuadNormal ? lce_QuadNormal : vec3(0.0, 0.0, 1.0);
}
)";

const char* const kStandardSetup = R"(void lce_SetupVertex()
{
    lce_SetupStandard();
    lce_Vertex = vec4(lce_aPosition, 1.0);
    lce_Normal = lce_AttributeNormal();
    lce_TangentData = lce_QuadTangentData(normalize(lce_Normal));
}
)";

const char* const kTexGenSetup = R"(void lce_SetupVertex()
{
    lce_SetupStandard();
    lce_Vertex = vec4(lce_aPosition, 1.0);
    lce_Normal = lce_AttributeNormal();
    lce_TangentData = lce_QuadTangentData(normalize(lce_Normal));
    vec4 eye = lce_MAT_WV * (lce_MAT_WV2 * lce_Vertex);
    lce_MultiTexCoord0 = mat4(uVC[23], uVC[24], uVC[25], uVC[26]) * eye
                         + mat4(uVC[27], uVC[28], uVC[29], uVC[30]) * lce_Vertex;
}
)";

const char* const kEntitySetup = R"(void lce_SetupVertex()
{
    lce_SetupStandard();
    int base = 41 + lce_aNormal.w * 4;
    mat4 bone = mat4(uVC[base], uVC[base + 1], uVC[base + 2], uVC[base + 3]);
    vec4 eye = bone * (lce_MAT_WV2 * vec4(lce_aPosition, 1.0));
    lce_Vertex = lce_CameraViewInverse * eye;
    mat3 toWorld = mat3(lce_CameraViewInverse) * mat3(bone) * mat3(lce_MAT_WV2);
    vec3 objectNormal = normalize(lce_AttributeNormal());
    lce_Normal = toWorld * objectNormal;
    float normalLength = length(lce_Normal);
    lce_Normal = normalLength > 0.0 ? lce_Normal / normalLength : vec3(0.0, 1.0, 0.0);
    vec4 objectTangent = lce_QuadTangentData(objectNormal);
    vec3 tangent = toWorld * objectTangent.xyz;
    lce_TangentData = dot(tangent, tangent) > 0.0 ? vec4(normalize(tangent), objectTangent.w)
                                                  : lce_FallbackTangent(lce_Normal);
}
)";

const char* const kQuadSetup = R"(void lce_SetupVertex()
{
    lce_SetupDefaults();
    vec2 corner = vec2(float(gl_VertexID & 1), float(gl_VertexID >> 1));
    lce_Vertex = vec4(corner, 0.0, 1.0);
    lce_Color = vec4(1.0);
    lce_Normal = vec3(0.0, 0.0, 1.0);
    lce_MultiTexCoord0 = vec4(corner, 0.0, 1.0);
    lce_MultiTexCoord1 = vec4(240.0, 240.0, 0.0, 1.0);
    lce_MidTexCoordData = vec4(0.5, 0.5, 0.0, 1.0);
    lce_TangentData = vec4(1.0, 0.0, 0.0, 1.0);
}
)";
}

const char* ShaderAdapterName(ShaderAdapter adapter)
{
    switch (adapter)
    {
    case ShaderAdapter_Terrain:
        return "terrain";
    case ShaderAdapter_Standard:
        return "standard";
    case ShaderAdapter_TexGen:
        return "texgen";
    case ShaderAdapter_Entity:
        return "entity";
    case ShaderAdapter_Quad:
        return "quad";
    default:
        return "unknown";
    }
}

std::string ShaderAdapterHeader(ShaderAdapter adapter, ShaderStage stage, const GlslUsage& usage)
{
    std::string header;
    int constants = VertexConstantCount(adapter);
    if (constants > 0)
    {
        header += "layout(std140) uniform VertexConstants\n{\n    vec4 uVC[" + std::to_string(constants) + "];\n};\n";
        header += "layout(std140) uniform PixelConstants\n{\n    vec4 uPC[32];\n};\n";
    }
    header += kCommonUniforms;

    if (adapter == ShaderAdapter_Quad)
    {
        header += kQuadConstants;
        header += "mat4[8] lce_TextureMatrixArray()\n{\n    mat4 matrices[8];\n"
                  "    for (int index = 0; index < 8; ++index)\n        matrices[index] = mat4(1.0);\n"
                  "    return matrices;\n}\n";
        header += "#define lce_ModelViewMatrixInverse mat4(1.0)\n#define lce_NormalMatrix mat3(1.0)\n";
    }
    else
    {
        header += kGameConstants;
        if (adapter == ShaderAdapter_Terrain)
        {
            header += "#define lce_ModelViewMatrix lce_MAT_WV\n";
            header += "#define lce_ModelViewMatrixInverse inverse(lce_ModelViewMatrix)\n";
            header += "#define lce_NormalMatrix transpose(inverse(mat3(lce_ModelViewMatrix)))\n";
        }
        else if (adapter == ShaderAdapter_Entity)
        {
            header += "#define lce_ModelViewMatrix lce_CameraView\n";
            header += "#define lce_ModelViewMatrixInverse lce_CameraViewInverse\n";
            header += "#define lce_NormalMatrix mat3(lce_CameraView)\n";
        }
        else
        {
            header += "#define lce_ModelViewMatrix (lce_MAT_WV * lce_MAT_WV2)\n";
            header += "#define lce_ModelViewMatrixInverse inverse(lce_ModelViewMatrix)\n";
            header += "#define lce_NormalMatrix transpose(inverse(mat3(lce_ModelViewMatrix)))\n";
        }
        header += "mat4[8] lce_TextureMatrixArray()\n{\n    mat4 matrices[8];\n"
                  "    for (int index = 0; index < 8; ++index)\n        matrices[index] = mat4(1.0);\n";
        if (adapter != ShaderAdapter_Terrain)
            header += "    matrices[0] = lce_MAT_TEX;\n";
        header += "    matrices[1] = lce_LightmapMatrix;\n    return matrices;\n}\n";
    }
    header += kDerivedMatrices;

    if (usage.builtins.count("lce_LightSource") || usage.builtins.count("lce_LightModel"))
        header += kLightUniforms;
    if (usage.builtins.count("lce_FrontMaterial") || usage.builtins.count("lce_BackMaterial"))
        header += kMaterial;

    if (stage == ShaderStage_Vertex)
    {
        header += kVertexGlobals;
        switch (adapter)
        {
        case ShaderAdapter_Terrain:
            header += kTerrainVertex;
            break;
        case ShaderAdapter_Standard:
            header += kStandardAttributes;
            header += kStandardSetup;
            break;
        case ShaderAdapter_TexGen:
            header += kStandardAttributes;
            header += kTexGenSetup;
            break;
        case ShaderAdapter_Entity:
            header += kStandardAttributes;
            header += kEntitySetup;
            break;
        default:
            header += kQuadSetup;
            break;
        }
    }
    return header;
}

std::string ShaderAdapterFooter(ShaderAdapter adapter, ShaderStage stage, const std::string& fragmentOutput0)
{
    if (stage == ShaderStage_Vertex)
    {
        std::string footer = "void main()\n{\n    lce_SetupVertex();\n    lce_AssignAttributes();\n"
                             "    lce_pack_main();\n";
        if (adapter == ShaderAdapter_Quad)
            footer += "    gl_Position.y *= lce_FlipY;\n";
        footer += "}\n";
        return footer;
    }

    if (stage == ShaderStage_Fragment && !fragmentOutput0.empty() && adapter != ShaderAdapter_Quad)
    {
        return "float lce_OutputAlpha(vec4 value)\n{\n    return value.a;\n}\n"
               "float lce_OutputAlpha(vec3 value)\n{\n    return 1.0;\n}\n"
               "float lce_OutputAlpha(vec2 value)\n{\n    return 1.0;\n}\n"
               "float lce_OutputAlpha(float value)\n{\n    return 1.0;\n}\n"
               "float lce_OutputAlpha(ivec4 value)\n{\n    return 1.0;\n}\n"
               "float lce_OutputAlpha(uvec4 value)\n{\n    return 1.0;\n}\n"
               "void main()\n{\n    lce_pack_main();\n"
               "    float alpha = lce_OutputAlpha(" + fragmentOutput0 + ");\n"
               "    int function = int(lce_AlphaTest.x);\n"
               "    float reference = lce_AlphaTest.y;\n"
               "    bool pass = true;\n"
               "    if (function == 0)\n        pass = alpha >= uPC[31].x;\n"
               "    else if (function == 1)\n        pass = false;\n"
               "    else if (function == 2)\n        pass = alpha < reference;\n"
               "    else if (function == 3)\n        pass = alpha == reference;\n"
               "    else if (function == 4)\n        pass = alpha <= reference;\n"
               "    else if (function == 5)\n        pass = alpha > reference;\n"
               "    else if (function == 6)\n        pass = alpha != reference;\n"
               "    else if (function == 7)\n        pass = alpha >= reference;\n"
               "    if (!pass)\n        discard;\n}\n";
    }

    return "void main()\n{\n    lce_pack_main();\n}\n";
}
