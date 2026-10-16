#include "platform/pc/PcPaths.hpp"

#include <SDL3/SDL.h>

#include <cstdlib>
#include <filesystem>

static std::string RootFromEnvironment(const char* variable, const char* fallback)
{
    const char* value = std::getenv(variable);
    if (value != nullptr && value[0] != '\0')
        return std::filesystem::absolute(value).generic_string();
    return (std::filesystem::current_path() / fallback).generic_string();
}

static std::string& ContentRoot()
{
    static std::string root = RootFromEnvironment("LCE_CONTENT_DIR", "assets");
    return root;
}

static std::string& SaveRoot()
{
    static std::string root = RootFromEnvironment("LCE_SAVE_DIR", "saves");
    return root;
}

void PcSetContentRoot(const std::string& path)
{
    ContentRoot() = std::filesystem::absolute(path).generic_string();
}

void PcSetSaveRoot(const std::string& path)
{
    SaveRoot() = std::filesystem::absolute(path).generic_string();
}

const std::string& PcContentRoot()
{
    return ContentRoot();
}

const std::string& PcSaveRoot()
{
    return SaveRoot();
}

const std::string& PcResourceRoot()
{
    static std::string root;
    if (root.empty())
    {
        const char* value = std::getenv("LCE_RESOURCE_DIR");
        const char* base = SDL_GetBasePath();
        if (value != nullptr && value[0] != '\0')
            root = std::filesystem::absolute(value).generic_string();
        else if (base != nullptr)
            root = (std::filesystem::path(base) / "resources").generic_string();
        else
            root = (std::filesystem::current_path() / "resources").generic_string();
    }
    return root;
}
