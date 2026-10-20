#include "platform/pc/RenderShaders.hpp"

#include <stdio.h>

namespace
{
const char* const kVertexHeader = R"(#version 450
layout(std140, set=1, binding=0) uniform VertexConstants
{
    vec4 uVC[VC_COUNT];
};
#define MAT_WV mat4(uVC[0], uVC[1], uVC[2], uVC[3])
#define MAT_WV2 mat4(uVC[4], uVC[5], uVC[6], uVC[7])
#define MAT_P mat4(uVC[8], uVC[9], uVC[10], uVC[11])
layout(location=0) out vec4 vColour;
layout(location=1) out vec4 vTexCoord;
void EmitPosition(vec4 clip)
{
    gl_Position = vec4(clip.xy, (clip.z + clip.w) * 0.5, clip.w);
}
)";

const char* const kFog = R"(
float FogFactor(vec3 eye)
{
    vec4 fog = uVC[17];
    if (fog.z == 1.0)
    {
        float nearDistance = sqrt(fog.y * fog.y);
        float farDistance = sqrt(fog.x * fog.x);
        float range = farDistance - nearDistance;
        float scale = range == 0.0 ? 0.0 : 1.0 / range;
        return clamp((farDistance - length(eye)) * scale, 0.0, 1.0);
    }
    if (fog.z == 2.0)
    {
        return clamp(exp2(1.442695 * eye.z * fog.x), 0.0, 1.0);
    }
    if (fog.z == 3.0)
    {
        return clamp(exp2(1.442695 * eye.z * (eye.z * -fog.x * fog.x)), 0.0, 1.0);
    }
    return 1.0;
}
)";

const char* const kUVMatrix = R"(
vec4 TransformUV(vec4 coordinate)
{
    return mat4(uVC[12], uVC[13], uVC[14], uVC[15]) * coordinate;
}
)";

const char* const kLightMap = R"(
layout(set=0, binding=0) uniform sampler2D uLightTexture;
vec3 LightMap(ivec2 light)
{
    vec2 coordinate = fract(max(vec2(light) * (1.0 / 256.0), uVC[16].xy));
    return textureLod(uLightTexture, coordinate, 0.0).rgb;
}
)";

const char* const kLighting = R"(
vec4 Lighting(vec4 normal)
{
    float lengthSquared = dot(normal, normal);
    vec4 unit = normal * (lengthSquared <= 0.0 ? 0.0 : inversesqrt(lengthSquared));
    float diffuse0 = max(dot(unit, uVC[18]), 0.0);
    float diffuse1 = max(dot(unit, uVC[19]), 0.0);
    return clamp(diffuse0 * uVC[20] + uVC[22] + diffuse1 * uVC[21], 0.0, 1.0);
}
)";

const char* const kStandardAttributes = R"(
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec4 aColour;
layout(location = 2) in ivec4 aNormal;
layout(location = 3) in vec2 aTexCoord;
layout(location = 4) in ivec2 aLight;
vec4 NormalVector()
{
    return max(vec4(aNormal) * (1.0 / 127.0), vec4(-1.0));
}
)";

const char* const kStandardMain = R"(
void main()
{
    vec4 eye = MAT_WV * (MAT_WV2 * vec4(aPosition, 1.0));
    vec4 colour = aColour.wzyx;
    vColour = vec4(colour.rgb * LightMap(aLight.yx), colour.a);
    vTexCoord = vec4(TransformUV(vec4(aTexCoord, 0.0, 1.0)).xy, FogFactor(eye.xyz), 1.0);
    EmitPosition(MAT_P * eye);
}
)";

const char* const kStandardLitMain = R"(
void main()
{
    vec4 eye = MAT_WV * (MAT_WV2 * vec4(aPosition, 1.0));
    vec4 normal = MAT_WV * vec4(mat3(MAT_WV2) * NormalVector().xyz, 0.0);
    vec4 lit = Lighting(normal);
    vec4 colour = aColour.wzyx;
    vColour = vec4(lit.rgb * colour.rgb * LightMap(aLight.yx), lit.a);
    vTexCoord = vec4(TransformUV(vec4(aTexCoord, 0.0, 1.0)).xy, FogFactor(eye.xyz), 1.0);
    EmitPosition(MAT_P * eye);
}
)";

const char* const kTexGenMain = R"(
void main()
{
    vec4 local = vec4(aPosition, 1.0);
    vec4 eye = MAT_WV * (MAT_WV2 * local);
    vec4 generated = mat4(uVC[23], uVC[24], uVC[25], uVC[26]) * eye
                     + mat4(uVC[27], uVC[28], uVC[29], uVC[30]) * local;
    vec4 coordinate = TransformUV(generated);
    vec4 colour = aColour.wzyx;
    vColour = vec4(colour.rgb * LightMap(aLight.yx), colour.a);
    vTexCoord = vec4(coordinate.x, coordinate.y, FogFactor(eye.xyz), coordinate.w);
    EmitPosition(MAT_P * eye);
}
)";

const char* const kTerrainMain = R"(
layout(set=0, binding=0) uniform sampler2D uLightTexture;
layout(location = 0) in ivec4 aPosition;
layout(location = 1) in ivec4 aTexture;
void main()
{
    vec3 position = vec3(aPosition.xyz) * (1.0 / 1024.0) + vec3(float(aTexture.w), uVC[18].y, uVC[18].z);
    vec4 eye = MAT_WV * vec4(position, 1.0);
    float packedColour = float(aPosition.w) + 32768.0;
    vec3 colour = vec3(fract(packedColour * (1.0 / 32768.0)), fract(packedColour * (1.0 / 1024.0)),
                       fract(packedColour * (1.0 / 32.0)));
    float alpha = 1.0;
    if (fract(packedColour * (1.0 / 65536.0)) >= 0.5)
    {
        float fadeDistance = sqrt(uVC[17].w * uVC[17].w);
        float fadeScale = fadeDistance == 0.0 ? 0.0 : 1.0 / fadeDistance;
        float fade = clamp((fadeDistance - length(eye.xyz)) * fadeScale, 0.0, 1.0);
        alpha = (1.0 - uVC[40].x) * (1.0 - fade) + uVC[40].x;
    }
    float light = float(aTexture.z);
    vec2 lightCoordinate = vec2(fract(light * (1.0 / 65536.0)), fract(light * (1.0 / 256.0)));
    vec3 lightColour = textureLod(uLightTexture, lightCoordinate, 0.0).rgb;
    vColour = vec4(colour * lightColour, alpha);
    vTexCoord = vec4(vec2(aTexture.xy) * (1.0 / 8192.0), FogFactor(eye.xyz), 1.0);
    EmitPosition(MAT_P * eye);
}
)";

const char* const kUIMain = R"(
layout(location = 0) in vec2 aPosition;
void main()
{
    vec4 clip = MAT_P * (MAT_WV * (MAT_WV2 * vec4(aPosition, 0.0, 1.0)));
    vColour = vec4(1.0);
    vTexCoord = vec4(aPosition * uVC[33].xy + uVC[33].zw, 0.0, 0.0);
    EmitPosition(clip);
}
)";

const char* const kGridFunctions = R"(
layout(location = 0) in vec2 aPosition;
float Reciprocal(float value)
{
    return value == 0.0 ? 0.0 : 1.0 / value;
}
float GridAxis(float value, vec4 edges, float scale, float middle)
{
    if (edges.y >= value)
    {
        float result = (value - edges.x) * Reciprocal(scale) + edges.x;
        return result > middle ? middle : result;
    }
    if (value >= edges.z)
    {
        float result = (value - edges.w) * Reciprocal(scale) + edges.w;
        return middle > result ? middle : result;
    }
    return value;
}
)";

const char* const kScalingGridMain = R"(
void main()
{
    vec2 position = vec2(GridAxis(aPosition.x, uVC[34], uVC[36].x, (uVC[34].y + uVC[34].z) * 0.5),
                         GridAxis(aPosition.y, uVC[35], uVC[36].y, (uVC[35].y + uVC[35].z) * 0.5));
    vec4 clip = MAT_P * (MAT_WV * (MAT_WV2 * vec4(position, 0.0, 1.0)));
    vColour = vec4(1.0);
    vTexCoord = vec4(aPosition * uVC[33].xy + uVC[33].zw, 0.0, 0.0);
    EmitPosition(clip);
}
)";

const char* const kTiledMain = R"(
void main()
{
    vec2 position = vec2(GridAxis(aPosition.x, uVC[34], uVC[36].x, (uVC[34].w - uVC[34].x) * 0.5),
                         GridAxis(aPosition.y, uVC[35], uVC[36].y, (uVC[35].w - uVC[35].x) * 0.5));
    vec4 clip = MAT_P * (MAT_WV * (MAT_WV2 * vec4(position, 0.0, 1.0)));
    vColour = vec4(1.0);
    vTexCoord = vec4(position * uVC[33].xy + uVC[33].zw, 0.0, 0.0);
    EmitPosition(clip);
}
)";

const char* const kColouredUIMain = R"(
layout(location = 0) in vec2 aPosition;
layout(location = 1) in vec2 aTexCoord;
layout(location = 2) in vec4 aColour;
void main()
{
    vec4 clip = MAT_P * (MAT_WV * (MAT_WV2 * vec4(aPosition, 0.0, 1.0)));
    vColour = aColour.wzyx;
    vTexCoord = vec4(aTexCoord, 0.0, 0.0);
    EmitPosition(clip);
}
)";

const char* const kEntityMain = R"(
mat4 BoneMatrix()
{
    int base = 41 + aNormal.w * 4;
    return mat4(uVC[base], uVC[base + 1], uVC[base + 2], uVC[base + 3]);
}
void main()
{
    vec4 eye = BoneMatrix() * (MAT_WV2 * vec4(aPosition, 1.0));
    vec4 colour = aColour.wzyx;
    vColour = vec4(colour.rgb * LightMap(aLight.yx), colour.a);
    vTexCoord = vec4(TransformUV(vec4(aTexCoord, 0.0, 1.0)).xy, FogFactor(eye.xyz), 1.0);
    EmitPosition(MAT_P * eye);
}
)";

const char* const kEntityLitMain = R"(
mat4 BoneMatrix()
{
    int base = 41 + aNormal.w * 4;
    return mat4(uVC[base], uVC[base + 1], uVC[base + 2], uVC[base + 3]);
}
void main()
{
    mat4 bone = BoneMatrix();
    vec4 eye = bone * (MAT_WV2 * vec4(aPosition, 1.0));
    vec4 normal = bone * vec4(mat3(MAT_WV2) * NormalVector().xyz, 0.0);
    vec4 lit = Lighting(normal);
    vec4 colour = aColour.wzyx;
    vColour = vec4(lit.rgb * colour.rgb * LightMap(aLight.yx), lit.a);
    vTexCoord = vec4(TransformUV(vec4(aTexCoord, 0.0, 1.0)).xy, FogFactor(eye.xyz), 1.0);
    EmitPosition(MAT_P * eye);
}
)";

const char* const kScreenQuadVertex = R"(#version 450
layout(std140, set=1, binding=0) uniform Quad { vec4 uQuad; };
layout(location=0) out vec4 vColour;
layout(location=1) out vec4 vTexCoord;
void main()
{
    int column = gl_VertexIndex & 1;
    int row = gl_VertexIndex >> 1;
    vec2 position = vec2(float(column * 2 - 1), float(row * 2 - 1));
    vColour = vec4(1.0);
    vTexCoord = vec4(float(column) * uQuad.z + uQuad.x, float(1 - row) * uQuad.w + uQuad.y, 0.0, 0.0);
    gl_Position = vec4(position, 1.0, 1.0);
}
)";

const char* const kClearVertex = R"(#version 450
layout(location=0) out vec4 vColour;
layout(location=1) out vec4 vTexCoord;
void main()
{
    vColour = vec4(0.0);
    vTexCoord = vec4(0.0);
    vec2 position = vec2(float((gl_VertexIndex & -2) - 1), float(((gl_VertexIndex & 1) << 1) - 1));
    gl_Position = vec4(position, 1.0, 1.0);
}
)";

const char* const kPixelHeader = R"(#version 450
layout(std140, set=3, binding=0) uniform PixelConstants
{
    vec4 uPC[32];
};
#define DIFFUSE_COLOUR uPC[0]
#define FOG_COLOUR uPC[1]
#define LERP_COLOUR uPC[2]
#define ADD_COLOUR uPC[3]
#define TEX_LAYER_COLOUR0 uPC[4]
#define TEX_LAYER_COLOUR1 uPC[5]
#define TEX_LAYER_COLOUR2 uPC[6]
#define TEX_LAYER_COLOUR3 uPC[7]
layout(location=0) in vec4 vColour;
layout(location=1) in vec4 vTexCoord;
layout(location = 0) out vec4 oColour;
layout(set=2, binding=0) uniform sampler2D uTexture0;
layout(set=2, binding=1) uniform sampler2D uTexture1;
layout(set=2, binding=2) uniform sampler2D uTexture2;
layout(set=2, binding=3) uniform sampler2D uTexture3;
void Emit(vec4 colour)
{
    if (!(colour.a >= uPC[31].x))
    {
        discard;
    }
    oColour = colour;
}
vec4 SampleBase(vec2 coordinate)
{
    vec4 filtered = texture(uTexture0, coordinate);
    vec4 baseLevel = textureLod(uTexture0, coordinate, 0.0);
    return coordinate.x > 1.0 ? baseLevel : filtered;
}
vec3 ApplyFog(vec3 colour)
{
    return (colour - FOG_COLOUR.rgb) * vTexCoord.z + FOG_COLOUR.rgb;
}
vec3 ApplyLerp(vec3 colour)
{
    return (LERP_COLOUR.rgb - colour) * LERP_COLOUR.a + colour;
}
)";

const char* const kDefaultPixel = R"(
void main()
{
    vec4 base = SampleBase(vTexCoord.xy);
    vec3 colour = base.rgb * TEX_LAYER_COLOUR0.rgb;
    LAYERS
    colour = ApplyLerp(colour) * DIFFUSE_COLOUR.rgb * vColour.rgb;
    Emit(vec4(ApplyFog(colour), base.a * DIFFUSE_COLOUR.a * vColour.a));
}
)";

const char* const kTerrainPixel = R"(
void main()
{
    float scale = vTexCoord.w == 0.0 ? 0.0 : 1.0 / vTexCoord.w;
    vec4 base = texture(uTexture0, vTexCoord.xy * scale);
    vec3 colour = base.rgb * TEX_LAYER_COLOUR0.rgb * DIFFUSE_COLOUR.rgb * vColour.rgb;
    Emit(vec4(ApplyFog(colour), base.a * DIFFUSE_COLOUR.a * vColour.a));
}
)";

const char* const kTerrainForceLodPixel = R"(
void main()
{
    vec4 base = textureLod(uTexture0, vTexCoord.xy, uPC[8].x);
    vec3 colour = base.rgb * TEX_LAYER_COLOUR0.rgb * DIFFUSE_COLOUR.rgb * vColour.rgb;
    Emit(vec4(ApplyFog(colour), base.a * DIFFUSE_COLOUR.a * vColour.a));
}
)";

const char* const kOrderIndependentAlphaPixel = R"(
layout(location = 1) out vec4 oReveal;
layout(location = 2) out vec4 oWeight;
void main()
{
    vec4 base = SampleBase(vTexCoord.xy);
    vec3 colour = ApplyFog(base.rgb * TEX_LAYER_COLOUR0.rgb * DIFFUSE_COLOUR.rgb * vColour.rgb);
    float alpha = base.a * DIFFUSE_COLOUR.a * vColour.a;
    float depthWeight = max(3000.0 - 3000.0 * gl_FragCoord.z, 0.01);
    float brightest = max(colour.b, max(colour.r, colour.g));
    float weight = depthWeight * max(alpha, min(alpha * brightest, 1.0));
    if (!(weight * alpha >= uPC[31].x))
    {
        discard;
    }
    oColour = vec4(weight * alpha * colour, 0.0);
    oReveal = vec4(0.0, 0.0, 0.0, alpha);
    oWeight = vec4(weight * alpha, 0.0, 0.0, 0.0);
}
)";

const char* const kLayeredColourMaskPixel = R"(
void main()
{
    vec2 coordinate = vTexCoord.xy;
    vec3 base = textureLod(uTexture0, coordinate, 0.0).rgb;
    vec2 inverseSize = vec2(uPC[9].x == 0.0 ? 0.0 : 1.0 / uPC[9].x, uPC[9].y == 0.0 ? 0.0 : 1.0 / uPC[9].y);
    vec2 scaled = coordinate * 64.0 * inverseSize;
    vec3 mask = vec3(1.0);
    for (int layer = 0; layer < 7; ++layer)
    {
        vec4 data = uPC[10 + layer];
        float signValue = (data.w > 0.0 ? 1.0 : 0.0) - (0.0 > data.w ? 1.0 : 0.0);
        float whole = signValue * floor(abs(data.w));
        float part = data.w - whole;
        vec2 maskCoordinate = vec2(scaled.x + whole * inverseSize.x, scaled.y + part);
        float amount = textureLod(uTexture1, maskCoordinate, 0.0).r;
        mask = (data.rgb - mask) * amount + mask;
    }
    vec3 colour = base * mask * DIFFUSE_COLOUR.rgb * vColour.rgb;
    Emit(vec4(ApplyFog(colour), vColour.a * DIFFUSE_COLOUR.a));
}
)";

const char* const kUISampling = R"(
vec2 UICoordinate(vec2 coordinate)
{
    vec2 size = vec2(textureSize(uTexture0, 0));
    vec2 texel = coordinate * size;
    vec2 seam = floor(texel + 0.5);
    vec2 footprint = max(fwidth(texel), vec2(1.0e-5));
    vec2 sharp = (seam + clamp((texel - seam) / footprint, -0.5, 0.5)) / size;
    return uPC[31].y != 0.0 ? sharp : coordinate;
}
)";

const char* const kUIColourAddPixel = R"(
void main()
{
    vec4 base = texture(uTexture0, UICoordinate(vTexCoord.xy));
    vec3 colour = base.rgb * TEX_LAYER_COLOUR0.rgb * DIFFUSE_COLOUR.rgb + ADD_COLOUR.rgb;
    float alpha = base.a * DIFFUSE_COLOUR.a + ADD_COLOUR.a;
    Emit(vec4(colour * alpha, alpha));
}
)";

const char* const kUIPixel = R"(
void main()
{
    vec4 base = texture(uTexture0, UICoordinate(vTexCoord.xy));
    float alpha = vColour.a * base.a * DIFFUSE_COLOUR.a;
    vec3 colour = base.rgb * TEX_LAYER_COLOUR0.rgb * vColour.rgb * DIFFUSE_COLOUR.rgb;
    Emit(vec4(colour * alpha, alpha));
}
)";

const char* const kUIRadialPixel = R"(
void main()
{
    float radius = length(vTexCoord.xy - vec2(0.5)) * 2.0;
    vec4 base = texture(uTexture0, vec2(radius, 0.0));
    float alpha = base.a * DIFFUSE_COLOUR.a;
    vec3 colour = base.rgb * TEX_LAYER_COLOUR0.rgb * DIFFUSE_COLOUR.rgb;
    Emit(vec4(colour * alpha, alpha));
}
)";

const char* const kEntityChangeColourPixel = R"(
void main()
{
    vec4 base = SampleBase(vTexCoord.xy);
    if (base.a == 0.0)
    {
        discard;
    }
    vec3 colour = base.rgb * TEX_LAYER_COLOUR0.rgb;
    colour = (colour * DIFFUSE_COLOUR.rgb - colour) * base.a + colour;
    colour = ApplyLerp(colour) * vColour.rgb;
    Emit(vec4(ApplyFog(colour), vColour.a * DIFFUSE_COLOUR.a));
}
)";

const char* const kEntityEmissiveAlphaPixel = R"(
void main()
{
    vec4 base = SampleBase(vTexCoord.xy);
    vec3 colour = base.rgb * TEX_LAYER_COLOUR0.rgb;
    if (((base.a + colour.r) + colour.g) + colour.b == 0.0)
    {
        discard;
    }
    colour = ApplyLerp(colour) * ((vColour.rgb - 1.0) * base.a + 1.0);
    Emit(vec4(ApplyFog(colour), base.a * ((vColour.a - 1.0) * base.a + 1.0)));
}
)";

const char* const kEntityEmissiveAlphaOnlyPixel = R"(
void main()
{
    vec4 base = SampleBase(vTexCoord.xy);
    if (base.a == 0.0 || base.a == 1.0)
    {
        discard;
    }
    vec3 colour = ApplyLerp(base.rgb * TEX_LAYER_COLOUR0.rgb) * ((vColour.rgb - 1.0) * base.a + 1.0);
    Emit(vec4(ApplyFog(colour), base.a * ((vColour.a - 1.0) * base.a + 1.0)));
}
)";

const char* const kCopyPixel = R"(#version 450
layout(location=1) in vec4 vTexCoord;
layout(location = 0) out vec4 oColour;
layout(set=2, binding=0) uniform sampler2D uTexture0;
void main()
{
    oColour = texture(uTexture0, vTexCoord.xy);
}
)";

const char* const kGammaPixel = R"(#version 450
layout(location=1) in vec4 vTexCoord;
layout(location = 0) out vec4 oColour;
layout(set=2, binding=0) uniform sampler2D uTexture0;
layout(std140, set=3, binding=0) uniform Gamma { float uGamma; };
float GammaChannel(float value)
{
    return exp2(uGamma * (value <= 0.0 ? -3.4028235e38 : log2(value)));
}
void main()
{
    vec3 colour = texture(uTexture0, vTexCoord.xy).rgb;
    oColour = vec4(GammaChannel(colour.r), GammaChannel(colour.g), GammaChannel(colour.b), 1.0);
}
)";

const char* const kAlphaResolvePixel = R"(#version 450
layout(location=1) in vec4 vTexCoord;
layout(location = 0) out vec4 oColour;
layout(set=2, binding=0) uniform sampler2D uTexture0;
layout(set=2, binding=1) uniform sampler2D uTexture1;
void main()
{
    vec3 accumulated = texture(uTexture0, vTexCoord.xy).rgb;
    float weight = min(max(texture(uTexture1, vTexCoord.xy).r, 1.0e-5), 50000.0);
    oColour = vec4(accumulated * (1.0 / weight), 1.0);
}
)";

const char* const kAlphaAccumulatePixel = R"(#version 450
layout(location = 0) out vec4 oColour;
layout(location = 1) out vec4 oReveal;
layout(location = 2) out vec4 oWeight;
void main()
{
    oColour = vec4(0.0);
    oReveal = vec4(0.0, 0.0, 0.0, 1.0);
    oWeight = vec4(0.0);
}
)";

const char* const kTwoLayers = "colour = (texture(uTexture1, vTexCoord.xy).rgb * TEX_LAYER_COLOUR1.rgb - colour)"
                               " * texture(uTexture1, vTexCoord.xy).a + colour;";
const char* const kThreeLayers = "colour = (texture(uTexture2, vTexCoord.xy).rgb * TEX_LAYER_COLOUR2.rgb - colour)"
                                 " * texture(uTexture2, vTexCoord.xy).a + colour;";
const char* const kFourLayers = "colour = (texture(uTexture3, vTexCoord.xy).rgb * TEX_LAYER_COLOUR3.rgb - colour)"
                                " * texture(uTexture3, vTexCoord.xy).a + colour;";

std::string VertexHeader(int count)
{
    char line[64];
    snprintf(line, sizeof(line), "#define VC_COUNT %d\n", count);
    std::string source(kVertexHeader);
    source.insert(source.find('\n') + 1, line);
    return source;
}

std::string DefaultPixel(int layers)
{
    std::string body(kDefaultPixel);
    std::string layerCode;

    if (layers >= 2)
    {
        layerCode += kTwoLayers;
    }

    if (layers >= 3)
    {
        layerCode += "\n    ";
        layerCode += kThreeLayers;
    }

    if (layers >= 4)
    {
        layerCode += "\n    ";
        layerCode += kFourLayers;
    }

    body.replace(body.find("LAYERS"), 6, layerCode);
    return std::string(kPixelHeader) + body;
}
}

int RenderVertexConstantCount(int vertexShader)
{
    switch (vertexShader)
    {
    case RenderVertexShader_Standard:
        return 18;
    case RenderVertexShader_Terrain:
        return 41;
    case RenderVertexShader_StandardLit:
        return 23;
    case RenderVertexShader_TexGen:
        return 31;
    case RenderVertexShader_UI:
        return 34;
    case RenderVertexShader_ScalingGridUI:
    case RenderVertexShader_TiledUI:
        return 37;
    case RenderVertexShader_ColouredUI:
        return 12;
    case RenderVertexShader_Entity:
    case RenderVertexShader_EntityLit:
        return 41 + 48 * 4;
    default:
        return 0;
    }
}

std::string RenderVertexShaderSource(int vertexShader)
{
    std::string source;

    switch (vertexShader)
    {
    case RenderVertexShader_Standard:
        source = VertexHeader(RenderVertexConstantCount(vertexShader)) + kFog + kUVMatrix + kLightMap
                 + kStandardAttributes + kStandardMain;
        break;
    case RenderVertexShader_Terrain:
        source = VertexHeader(RenderVertexConstantCount(vertexShader)) + kFog + kTerrainMain;
        break;
    case RenderVertexShader_StandardLit:
        source = VertexHeader(RenderVertexConstantCount(vertexShader)) + kFog + kUVMatrix + kLightMap + kLighting
                 + kStandardAttributes + kStandardLitMain;
        break;
    case RenderVertexShader_TexGen:
        source = VertexHeader(RenderVertexConstantCount(vertexShader)) + kFog + kUVMatrix + kLightMap
                 + kStandardAttributes + kTexGenMain;
        break;
    case RenderVertexShader_UI:
        source = VertexHeader(RenderVertexConstantCount(vertexShader)) + kUIMain;
        break;
    case RenderVertexShader_ScalingGridUI:
        source = VertexHeader(RenderVertexConstantCount(vertexShader)) + kGridFunctions + kScalingGridMain;
        break;
    case RenderVertexShader_TiledUI:
        source = VertexHeader(RenderVertexConstantCount(vertexShader)) + kGridFunctions + kTiledMain;
        break;
    case RenderVertexShader_ColouredUI:
        source = VertexHeader(RenderVertexConstantCount(vertexShader)) + kColouredUIMain;
        break;
    case RenderVertexShader_Entity:
        source = VertexHeader(RenderVertexConstantCount(vertexShader)) + kFog + kUVMatrix + kLightMap
                 + kStandardAttributes + kEntityMain;
        break;
    case RenderVertexShader_EntityLit:
        source = VertexHeader(RenderVertexConstantCount(vertexShader)) + kFog + kUVMatrix + kLightMap + kLighting
                 + kStandardAttributes + kEntityLitMain;
        break;
    case RenderVertexShader_ScreenQuad:
        source = kScreenQuadVertex;
        break;
    case RenderVertexShader_Clear:
        source = kClearVertex;
        break;
    case RenderVertexShader_Keyboard:
        source = R"(#version 450
layout(location=0) in vec2 aPosition;
layout(location=1) in vec2 aTexCoord;
layout(location=2) in vec4 aColour;
layout(std140, set=1, binding=0) uniform Screen { vec2 uScreen; };
layout(location=0) out vec4 vColour;
layout(location=1) out vec4 vTexCoord;
void main() {
    vec2 clip = aPosition / uScreen * 2.0 - 1.0;
    gl_Position = vec4(clip.x, -clip.y, 0.0, 1.0);
    vTexCoord = vec4(aTexCoord, 0.0, 0.0);
    vColour = aColour;
}
)";
        break;
    default:
        break;
    }

    if (!source.empty() && vertexShader != RenderVertexShader_Keyboard)
    {
        source.insert(source.find('\n') + 1,
                      std::string("layout(std140, set=1, binding=")
                          + (vertexShader == RenderVertexShader_Clear ? "0" : "1")
                          + ") uniform ViewportConstants { vec4 uViewport; };\n");
        source.replace(source.find("void main()"), 11, "void RenderMain()");
        source += "\nvoid main() { RenderMain(); gl_Position.xy = gl_Position.xy * uViewport.xy"
                  " + gl_Position.w * uViewport.zw; }\n";
    }
    return source;
}

std::string RenderPixelShaderSource(int pixelShader)
{
    switch (pixelShader)
    {
    case RenderPixelShader_Default:
        return DefaultPixel(1);
    case RenderPixelShader_DefaultTwoLayers:
        return DefaultPixel(2);
    case RenderPixelShader_DefaultThreeLayers:
        return DefaultPixel(3);
    case RenderPixelShader_DefaultFourLayers:
        return DefaultPixel(4);
    case RenderPixelShader_Terrain:
        return std::string(kPixelHeader) + kTerrainPixel;
    case RenderPixelShader_TerrainForceLod:
        return std::string(kPixelHeader) + kTerrainForceLodPixel;
    case RenderPixelShader_OrderIndependentAlpha:
        return std::string(kPixelHeader) + kOrderIndependentAlphaPixel;
    case RenderPixelShader_LayeredColourMask:
        return std::string(kPixelHeader) + kLayeredColourMaskPixel;
    case RenderPixelShader_UIColourAdd:
        return std::string(kPixelHeader) + kUISampling + kUIColourAddPixel;
    case RenderPixelShader_UI:
        return std::string(kPixelHeader) + kUISampling + kUIPixel;
    case RenderPixelShader_UIRadial:
        return std::string(kPixelHeader) + kUIRadialPixel;
    case RenderPixelShader_EntityChangeColour:
        return std::string(kPixelHeader) + kEntityChangeColourPixel;
    case RenderPixelShader_EntityEmissiveAlpha:
        return std::string(kPixelHeader) + kEntityEmissiveAlphaPixel;
    case RenderPixelShader_EntityEmissiveAlphaOnly:
        return std::string(kPixelHeader) + kEntityEmissiveAlphaOnlyPixel;
    case RenderPixelShader_Copy:
        return kCopyPixel;
    case RenderPixelShader_Gamma:
        return kGammaPixel;
    case RenderPixelShader_AlphaResolve:
        return kAlphaResolvePixel;
    case RenderPixelShader_AlphaAccumulate:
        return kAlphaAccumulatePixel;
    case RenderPixelShader_Clear:
        return R"(#version 450
layout(std140, set=3, binding=0) uniform ClearColour { vec4 uClear; };
layout(location=0) out vec4 oColour;
void main() { oColour = uClear; }
)";
    case RenderPixelShader_Keyboard:
        return R"(#version 450
layout(location=0) in vec4 vColour;
layout(location=1) in vec4 vTexCoord;
layout(set=2, binding=0) uniform sampler2D uGlyphs;
layout(location=0) out vec4 oColour;
void main() {
    float coverage = vTexCoord.x < 0.0 ? 1.0 : texture(uGlyphs, vTexCoord.xy).r;
    oColour = vec4(vColour.rgb, vColour.a * coverage);
}
)";
    default:
        return std::string();
    }
}
