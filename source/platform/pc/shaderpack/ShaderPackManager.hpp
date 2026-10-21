#ifndef PLATFORM_PC_SHADERPACK_SHADERPACKMANAGER_HPP
#define PLATFORM_PC_SHADERPACK_SHADERPACKMANAGER_HPP

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "platform/pc/shaderpack/ShaderPack.hpp"
#include "platform/pc/shaderpack/ShaderPackLang.hpp"
#include "platform/pc/shaderpack/ShaderPackOptions.hpp"
#include "platform/pc/shaderpack/ShaderPackProperties.hpp"
#include "platform/pc/shaderpack/ShaderSettings.hpp"

class ShaderPackManager
{
public:
    static const int kMinecraftVersion = ShaderSettings::kMinecraftVersion;

    static ShaderPackManager& Get();

    void Initialise();
    std::filesystem::path PacksFolder() const;
    std::vector<std::string> ListPacks() const;

    const ShaderSettings& Settings() const { return mSettings; }
    ShaderSettings& EditSettings() { return mSettings; }
    bool SaveSettings() const;

    bool SelectPack(const std::string& name);
    ShaderPack* ActivePack() { return mPack.get(); }
    ShaderPackOptions& Options() { return mOptions; }
    const ShaderPackProperties& Properties() const { return mProperties; }
    const ShaderPackLang& Lang() const { return mLang; }
    const std::vector<std::string>& Errors() const { return mErrors; }

    bool SetOption(const std::string& name, const std::string& value);
    void ApplyProfile(const std::string& profile);
    void ResetOptions();
    bool SaveOptions() const;

    ShaderMacros StandardMacros() const;
    int GlVersion() const { return mGlVersion; }
    int GlslVersion() const { return mGlslVersion; }

private:
    ShaderPackManager();

    bool LoadPack(const std::string& name);
    void UnloadPack();
    void RefreshProperties();
    void LogSummary() const;
    std::filesystem::path SettingsPath() const;
    std::filesystem::path OptionsPath(const std::string& pack) const;
    static std::string LocaleCode();

    ShaderSettings mSettings;
    std::unique_ptr<ShaderPack> mPack;
    ShaderPackOptions mOptions;
    ShaderPackProperties mProperties;
    ShaderPackLang mLang;
    std::vector<std::string> mErrors;
    std::string mGlVendor;
    std::string mGlRenderer;
    std::vector<std::string> mGlExtensions;
    int mGlVersion;
    int mGlslVersion;
};

#endif
