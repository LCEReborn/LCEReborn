#ifndef GAME_CORE_VIDEOSETTINGS_HPP
#define GAME_CORE_VIDEOSETTINGS_HPP

#include <string>

class VideoSettings
{
public:
    enum
    {
        FOV_MIN = 30,
        FOV_NORMAL = 70,
        FOV_MAX = 110,
        RENDER_DISTANCE_MIN = 2,
        RENDER_DISTANCE_DEFAULT = 10,
        RENDER_DISTANCE_MAX = 32,
        BIOME_BLEND_DEFAULT = 1,
        BIOME_BLEND_MAX = 7
    };

    enum EParticles
    {
        EParticles_All,
        EParticles_Decreased,
        EParticles_Minimal,
        EParticles_Count
    };

    static int GetFov();
    static void SetFov(int fov);
    static int GetRenderDistance();
    static void SetRenderDistance(int chunks);
    static bool GetFancyGraphics();
    static void SetFancyGraphics(bool fancy);
    static bool GetSmoothLighting();
    static void SetSmoothLighting(bool smooth);
    static int GetParticles();
    static void SetParticles(int particles);
    static int GetBiomeBlend();
    static void SetBiomeBlend(int radius);

    static void Load(const std::string& path);
    static void Apply();

private:
    static void Save();
    static void RebuildWorld();

    static int sFov;
    static int sRenderDistance;
    static bool sFancyGraphics;
    static bool sSmoothLighting;
    static int sParticles;
    static int sBiomeBlend;
    static std::string sPath;
};

#endif
