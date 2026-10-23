#ifndef PLATFORM_PC_SHADERPACK_SHADERSETTINGS_HPP
#define PLATFORM_PC_SHADERPACK_SHADERSETTINGS_HPP

#include <filesystem>
#include <string>
#include <vector>

#include "platform/pc/shaderpack/ShaderPreprocessor.hpp"

struct ShaderSettings
{
    enum Tristate
    {
        Default,
        On,
        Off
    };

    static const int kIrisVersion = 11103;
    static const int kMinecraftVersion = 11202;
    static const char* const kPackOff;
    static const char* const kPackInternal;
    static const float kQualityValues[5];
    static const float kHandDepthValues[3];

    std::string shaderPack = kPackOff;
    bool enableShaders = true;
    int antialiasingLevel = 0;
    bool normalMapEnabled = true;
    bool specularMapEnabled = true;
    float renderResMul = 1.0f;
    float shadowResMul = 1.0f;
    float handDepthMul = 0.125f;
    Tristate oldHandLight = Default;
    Tristate oldLighting = Default;

    void Load(const std::filesystem::path& path);
    bool Save(const std::filesystem::path& path) const;
    bool IsPackSelected() const { return shaderPack != kPackOff && shaderPack != kPackInternal; }
    bool UsesPack() const { return enableShaders && IsPackSelected(); }
    bool UsesInternal() const { return enableShaders && shaderPack == kPackInternal; }
};

ShaderMacros ShaderStandardMacros(const ShaderSettings& settings, int glVersion, int glslVersion,
                                  const std::string& glVendor, const std::string& glRenderer,
                                  const std::vector<std::string>& glExtensions = std::vector<std::string>());

#endif
