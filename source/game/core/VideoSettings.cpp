#include "game/core/VideoSettings.hpp"

#include <cstdio>
#include <cstring>

#include "game/client/render/LevelRenderer.hpp"
#include "game/core/Minecraft.hpp"
#include "game/core/Options.hpp"

int VideoSettings::sFov = VideoSettings::FOV_NORMAL;
int VideoSettings::sRenderDistance = VideoSettings::RENDER_DISTANCE_DEFAULT;
bool VideoSettings::sFancyGraphics = true;
bool VideoSettings::sSmoothLighting = true;
int VideoSettings::sParticles = VideoSettings::EParticles_All;
int VideoSettings::sBiomeBlend = VideoSettings::BIOME_BLEND_DEFAULT;
std::string VideoSettings::sPath;

namespace
{
int Clamp(int value, int minimum, int maximum)
{
    return value < minimum ? minimum : (value > maximum ? maximum : value);
}
}

int VideoSettings::GetFov()
{
    return sFov;
}

void VideoSettings::SetFov(int fov)
{
    sFov = Clamp(fov, FOV_MIN, FOV_MAX);
    Save();
}

int VideoSettings::GetRenderDistance()
{
    return sRenderDistance;
}

void VideoSettings::SetRenderDistance(int chunks)
{
    sRenderDistance = Clamp(chunks, RENDER_DISTANCE_MIN, RENDER_DISTANCE_MAX);
    Save();
}

bool VideoSettings::GetFancyGraphics()
{
    return sFancyGraphics;
}

void VideoSettings::SetFancyGraphics(bool fancy)
{
    sFancyGraphics = fancy;
    Save();
    Apply();
    RebuildWorld();
}

bool VideoSettings::GetSmoothLighting()
{
    return sSmoothLighting;
}

void VideoSettings::SetSmoothLighting(bool smooth)
{
    sSmoothLighting = smooth;
    Save();
    Apply();
    RebuildWorld();
}

int VideoSettings::GetParticles()
{
    return sParticles;
}

void VideoSettings::SetParticles(int particles)
{
    sParticles = Clamp(particles, EParticles_All, EParticles_Count - 1);
    Save();
    Apply();
}

int VideoSettings::GetBiomeBlend()
{
    return sBiomeBlend;
}

void VideoSettings::SetBiomeBlend(int radius)
{
    sBiomeBlend = Clamp(radius, 0, BIOME_BLEND_MAX);
    Save();
    RebuildWorld();
}

void VideoSettings::Load(const std::string& path)
{
    sPath = path;
    FILE* file = std::fopen(path.c_str(), "r");
    if (file == NULL)
        return;

    char line[128];
    while (std::fgets(line, sizeof(line), file) != NULL)
    {
        char key[64];
        int value = 0;
        if (std::sscanf(line, " %63[A-Za-z] = %d", key, &value) != 2)
            continue;
        if (std::strcmp(key, "Fov") == 0)
            sFov = Clamp(value, FOV_MIN, FOV_MAX);
        else if (std::strcmp(key, "RenderDistance") == 0)
            sRenderDistance = Clamp(value, RENDER_DISTANCE_MIN, RENDER_DISTANCE_MAX);
        else if (std::strcmp(key, "FancyGraphics") == 0)
            sFancyGraphics = value != 0;
        else if (std::strcmp(key, "SmoothLighting") == 0)
            sSmoothLighting = value != 0;
        else if (std::strcmp(key, "Particles") == 0)
            sParticles = Clamp(value, EParticles_All, EParticles_Count - 1);
        else if (std::strcmp(key, "BiomeBlend") == 0)
            sBiomeBlend = Clamp(value, 0, BIOME_BLEND_MAX);
    }
    std::fclose(file);
}

void VideoSettings::Save()
{
    if (sPath.empty())
        return;
    FILE* file = std::fopen(sPath.c_str(), "w");
    if (file == NULL)
    {
        std::fprintf(stderr, "video settings could not be written to %s\n", sPath.c_str());
        return;
    }
    std::fprintf(file, "Fov = %d\n", sFov);
    std::fprintf(file, "RenderDistance = %d\n", sRenderDistance);
    std::fprintf(file, "FancyGraphics = %d\n", sFancyGraphics ? 1 : 0);
    std::fprintf(file, "SmoothLighting = %d\n", sSmoothLighting ? 1 : 0);
    std::fprintf(file, "Particles = %d\n", sParticles);
    std::fprintf(file, "BiomeBlend = %d\n", sBiomeBlend);
    std::fclose(file);
}

void VideoSettings::Apply()
{
    Minecraft* minecraft = Minecraft::GetInstance();
    if (minecraft == NULL || minecraft->mOptions == NULL)
        return;
    minecraft->mOptions->mFancyGraphics = sFancyGraphics;
    minecraft->mOptions->mAmbientOcclusion = sSmoothLighting;
    minecraft->mOptions->mParticles = sParticles;
}

void VideoSettings::RebuildWorld()
{
    Minecraft* minecraft = Minecraft::GetInstance();
    if (minecraft == NULL || minecraft->mLevelRenderer == NULL || minecraft->mLevel == NULL)
        return;
    for (int player = 0; player < 4; ++player)
        minecraft->mLevelRenderer->allChanged(player);
}
