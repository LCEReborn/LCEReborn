#include "platform/pc/shaderpack/ShaderSettings.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <sstream>

#include "platform/pc/shaderpack/ShaderPackProperties.hpp"
#include "platform/pc/shaderpack/ShaderUniforms.hpp"

const char* const ShaderSettings::kPackOff = "OFF";
const char* const ShaderSettings::kPackInternal = "(internal)";
const float ShaderSettings::kQualityValues[5] = {0.5f, 0.70710677f, 1.0f, 1.4142135f, 2.0f};
const float ShaderSettings::kHandDepthValues[3] = {0.0625f, 0.125f, 0.25f};

namespace
{
std::string Lower(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

std::string FloatMacro(float value)
{
    std::ostringstream stream;
    stream.precision(8);
    stream << value;
    std::string text = stream.str();
    if (text.find('.') == std::string::npos)
        text += ".0";
    return text;
}

template <size_t Count> float Snap(float value, const float (&allowed)[Count], float fallback)
{
    if (!std::isfinite(value))
        return fallback;
    float best = allowed[0];
    for (float candidate : allowed)
    {
        if (std::fabs(candidate - value) < std::fabs(best - value))
            best = candidate;
    }
    return best;
}

ShaderSettings::Tristate ParseTristate(const std::string& text)
{
    if (text == "true")
        return ShaderSettings::On;
    if (text == "false")
        return ShaderSettings::Off;
    return ShaderSettings::Default;
}

const char* TristateText(ShaderSettings::Tristate value)
{
    return value == ShaderSettings::On ? "true" : value == ShaderSettings::Off ? "false" : "default";
}

std::string FloatText(float value)
{
    std::ostringstream stream;
    stream << value;
    std::string text = stream.str();
    if (text.find('.') == std::string::npos && text.find('e') == std::string::npos)
        text += ".0";
    return text;
}
}

void ShaderSettings::Load(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
        return;
    std::string text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    for (const auto& entry : ShaderParseProperties(text))
    {
        const std::string& key = entry.first;
        const std::string& value = entry.second;
        if (key == "shaderPack")
            shaderPack = value.empty() ? std::string(kPackOff) : value;
        else if (key == "antialiasingLevel")
        {
            int level = std::atoi(value.c_str());
            antialiasingLevel = level >= 4 ? 4 : level >= 2 ? 2 : 0;
        }
        else if (key == "normalMapEnabled")
            normalMapEnabled = value != "false";
        else if (key == "specularMapEnabled")
            specularMapEnabled = value != "false";
        else if (key == "renderResMul")
            renderResMul = Snap(static_cast<float>(std::atof(value.c_str())), kQualityValues, 1.0f);
        else if (key == "shadowResMul")
            shadowResMul = Snap(static_cast<float>(std::atof(value.c_str())), kQualityValues, 1.0f);
        else if (key == "handDepthMul")
            handDepthMul = Snap(static_cast<float>(std::atof(value.c_str())), kHandDepthValues, 0.125f);
        else if (key == "oldHandLight")
            oldHandLight = ParseTristate(value);
        else if (key == "oldLighting")
            oldLighting = ParseTristate(value);
    }
}

bool ShaderSettings::Save(const std::filesystem::path& path) const
{
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    std::filesystem::path temporary = path;
    temporary += ".tmp";
    {
        std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
        if (!file)
            return false;
        file << "shaderPack=" << shaderPack << "\n";
        file << "antialiasingLevel=" << antialiasingLevel << "\n";
        file << "normalMapEnabled=" << (normalMapEnabled ? "true" : "false") << "\n";
        file << "specularMapEnabled=" << (specularMapEnabled ? "true" : "false") << "\n";
        file << "renderResMul=" << FloatText(renderResMul) << "\n";
        file << "shadowResMul=" << FloatText(shadowResMul) << "\n";
        file << "handDepthMul=" << FloatText(handDepthMul) << "\n";
        file << "oldHandLight=" << TristateText(oldHandLight) << "\n";
        file << "oldLighting=" << TristateText(oldLighting) << "\n";
        if (!file)
            return false;
    }
    std::filesystem::rename(temporary, path, error);
    return !error;
}

ShaderMacros ShaderStandardMacros(const ShaderSettings& settings, int glVersion, int glslVersion,
                                  const std::string& glVendor, const std::string& glRenderer,
                                  const std::vector<std::string>& glExtensions)
{
    ShaderMacros macros;
    for (const std::string& extension : glExtensions)
    {
        if (extension.compare(0, 3, "GL_") == 0)
            macros["MC_" + extension] = std::string();
    }
    macros["MC_VERSION"] = std::to_string(ShaderSettings::kMinecraftVersion);
    macros["IS_IRIS"] = std::string();
    macros["IRIS_VERSION"] = std::to_string(ShaderSettings::kIrisVersion);
    macros["IRIS_FEATURE_CUSTOM_TEXTURE_NAME"] = std::string();
    macros["MAX_COLOR_BUFFERS"] = "16";
    macros["MC_GL_VERSION"] = std::to_string(glVersion);
    macros["MC_GLSL_VERSION"] = std::to_string(glslVersion);
#if defined(_WIN32)
    macros["MC_OS_WINDOWS"] = std::string();
#elif defined(__APPLE__)
    macros["MC_OS_MAC"] = std::string();
#elif defined(__linux__)
    macros["MC_OS_LINUX"] = std::string();
#else
    macros["MC_OS_OTHER"] = std::string();
#endif

    std::string vendor = Lower(glVendor);
    const char* vendorMacro = "MC_GL_VENDOR_OTHER";
    if (vendor.find("nvidia") != std::string::npos)
        vendorMacro = "MC_GL_VENDOR_NVIDIA";
    else if (vendor.find("ati") == 0)
        vendorMacro = "MC_GL_VENDOR_ATI";
    else if (vendor.find("amd") != std::string::npos)
        vendorMacro = "MC_GL_VENDOR_AMD";
    else if (vendor.find("intel") != std::string::npos)
        vendorMacro = "MC_GL_VENDOR_INTEL";
    else if (vendor.find("mesa") != std::string::npos)
        vendorMacro = "MC_GL_VENDOR_MESA";
    else if (vendor.find("x.org") != std::string::npos)
        vendorMacro = "MC_GL_VENDOR_XORG";
    macros[vendorMacro] = std::string();
    if (std::string(vendorMacro) == "MC_GL_VENDOR_NVIDIA")
    {
        static const char* const kSmallIntegers[][2] = {
            {"int8_t", "int"},     {"i8vec2", "ivec2"},   {"i8vec3", "ivec3"},   {"i8vec4", "ivec4"},
            {"uint8_t", "uint"},   {"u8vec2", "uvec2"},   {"u8vec3", "uvec3"},   {"u8vec4", "uvec4"},
            {"int16_t", "int"},    {"i16vec2", "ivec2"},  {"i16vec3", "ivec3"},  {"i16vec4", "ivec4"},
            {"uint16_t", "uint"},  {"u16vec2", "uvec2"},  {"u16vec3", "uvec3"},  {"u16vec4", "uvec4"}};
        for (const auto& type : kSmallIntegers)
            macros[type[0]] = type[1];
    }

    std::string renderer = Lower(glRenderer);
    const char* rendererMacro = "MC_GL_RENDERER_OTHER";
    if (renderer.find("radeon") != std::string::npos)
        rendererMacro = "MC_GL_RENDERER_RADEON";
    else if (renderer.find("geforce") != std::string::npos)
        rendererMacro = "MC_GL_RENDERER_GEFORCE";
    else if (renderer.find("quadro") != std::string::npos)
        rendererMacro = "MC_GL_RENDERER_QUADRO";
    else if (renderer.find("intel") != std::string::npos)
        rendererMacro = "MC_GL_RENDERER_INTEL";
    else if (renderer.find("gallium") != std::string::npos)
        rendererMacro = "MC_GL_RENDERER_GALLIUM";
    else if (renderer.find("mesa") != std::string::npos)
        rendererMacro = "MC_GL_RENDERER_MESA";
    macros[rendererMacro] = std::string();

    if (settings.antialiasingLevel > 0)
        macros["MC_FXAA_LEVEL"] = std::to_string(settings.antialiasingLevel);
    if (settings.normalMapEnabled)
        macros["MC_NORMAL_MAP"] = std::string();
    if (settings.specularMapEnabled)
        macros["MC_SPECULAR_MAP"] = std::string();
    macros["MC_RENDER_QUALITY"] = FloatMacro(settings.renderResMul);
    macros["MC_SHADOW_QUALITY"] = FloatMacro(settings.shadowResMul);
    macros["MC_HAND_DEPTH"] = FloatMacro(settings.handDepthMul);

    for (int stage = 0; stage < ShaderRenderStage_Count; ++stage)
        macros[std::string("MC_RENDER_STAGE_") + ShaderRenderStageName(stage)] = std::to_string(stage);
    return macros;
}
