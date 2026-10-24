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

    enum Choice
    {
        Choice_Antialiasing,
        Choice_NormalMap,
        Choice_SpecularMap,
        Choice_RenderQuality,
        Choice_ShadowQuality,
        Choice_HandDepth,
        Choice_OldHandLight,
        Choice_OldLighting,
        Choice_Count
    };

    struct ChoiceInfo
    {
        const char* name;
        const char* macro;
        bool slider;
        int count;
        const char* const* labels;
    };

    static const ChoiceInfo& Describe(int choice);

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
    int GetChoice(int choice) const;
    void SetChoice(int choice, int index);
    bool operator==(const ShaderSettings& other) const;
    bool operator!=(const ShaderSettings& other) const { return !(*this == other); }
};

ShaderMacros ShaderStandardMacros(const ShaderSettings& settings, int glVersion, int glslVersion,
                                  const std::string& glVendor, const std::string& glRenderer,
                                  const std::vector<std::string>& glExtensions = std::vector<std::string>());

#endif
