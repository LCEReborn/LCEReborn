#include "game/core/VideoSettings.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "game/client/render/LevelRenderer.hpp"
#include "game/core/Minecraft.hpp"
#include "game/core/Options.hpp"
#if LCE_PLATFORM_PC
#include "platform/pc/Log.hpp"
#endif

int VideoSettings::sFov = VideoSettings::FOV_NORMAL;
int VideoSettings::sRenderDistance = VideoSettings::RENDER_DISTANCE_DEFAULT;
int VideoSettings::sSimulationDistance = VideoSettings::SIMULATION_DISTANCE_DEFAULT;
bool VideoSettings::sFancyGraphics = true;
bool VideoSettings::sSmoothLighting = true;
int VideoSettings::sParticles = VideoSettings::EParticles_All;
int VideoSettings::sBiomeBlend = VideoSettings::BIOME_BLEND_DEFAULT;
int VideoSettings::sHostPort = VideoSettings::HOST_PORT_DEFAULT;
std::wstring VideoSettings::sHostMessage;
bool VideoSettings::sHostVisible = true;
std::string VideoSettings::sPath;

namespace
{
int Clamp(int value, int minimum, int maximum)
{
    return value < minimum ? minimum : (value > maximum ? maximum : value);
}

std::wstring CleanMessage(const std::wstring& message)
{
    std::wstring clean;
    for (size_t index = 0; index < message.size() && clean.size() < VideoSettings::HOST_MESSAGE_MAX; ++index)
        if (message[index] >= 0x20 && message[index] != 0x7F)
            clean.push_back(message[index]);
    const size_t first = clean.find_first_not_of(L' ');
    if (first == std::wstring::npos)
        return std::wstring();
    return clean.substr(first, clean.find_last_not_of(L' ') + 1 - first);
}

std::string EncodeUtf8(const std::wstring& text)
{
    std::string out;
    for (size_t index = 0; index < text.size(); ++index)
    {
        unsigned int code = static_cast<unsigned int>(text[index]) & 0xFFFF;
        if (code < 0x80)
        {
            out.push_back(static_cast<char>(code));
        }
        else if (code < 0x800)
        {
            out.push_back(static_cast<char>(0xC0 | (code >> 6)));
            out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
        }
        else
        {
            out.push_back(static_cast<char>(0xE0 | (code >> 12)));
            out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
        }
    }
    return out;
}

std::wstring DecodeUtf8(const char* text)
{
    std::wstring out;
    const unsigned char* bytes = reinterpret_cast<const unsigned char*>(text);
    while (*bytes != 0)
    {
        unsigned int code = *bytes++;
        int extra = 0;
        if (code >= 0xE0 && code < 0xF0)
        {
            code &= 0x0F;
            extra = 2;
        }
        else if (code >= 0xC0 && code < 0xE0)
        {
            code &= 0x1F;
            extra = 1;
        }
        else if (code >= 0x80)
        {
            code = L'?';
        }
        for (; extra > 0 && (*bytes & 0xC0) == 0x80; --extra)
            code = (code << 6) | (*bytes++ & 0x3F);
        out.push_back(extra == 0 ? static_cast<wchar_t>(code) : L'?');
    }
    return out;
}
}

int VideoSettings::GetFov()
{
    return sFov;
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

int VideoSettings::GetViewDistanceForPlayers(int players)
{
    if (players < 1)
        players = 1;
    const int grid = (int)sqrtf(4.0f * (float)(sRenderDistance * sRenderDistance) / (float)players);
    return Clamp(grid / 2, RENDER_DISTANCE_MIN, RENDER_DISTANCE_MAX);
}

int VideoSettings::GetSimulationDistance()
{
    return sSimulationDistance;
}

void VideoSettings::SetSimulationDistance(int chunks)
{
    sSimulationDistance = Clamp(chunks, SIMULATION_DISTANCE_MIN, SIMULATION_DISTANCE_MAX);
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

int VideoSettings::GetHostPort()
{
    return sHostPort;
}

void VideoSettings::SetHostPort(int port)
{
    sHostPort = Clamp(port, HOST_PORT_MIN, HOST_PORT_MAX);
    Save();
}

std::wstring VideoSettings::GetHostMessage()
{
    return sHostMessage;
}

void VideoSettings::SetHostMessage(const std::wstring& message)
{
    sHostMessage = CleanMessage(message);
    Save();
}

bool VideoSettings::GetHostVisible()
{
    return sHostVisible;
}

void VideoSettings::SetHostVisible(bool visible)
{
    sHostVisible = visible;
    Save();
}

void VideoSettings::Load(const std::string& path)
{
    sPath = path;
    FILE* file = std::fopen(path.c_str(), "r");
    if (file == NULL)
        return;

    char line[512];
    while (std::fgets(line, sizeof(line), file) != NULL)
    {
        char key[64];
        int value = 0;
        int textStart = 0;
        if (std::sscanf(line, " HostMessage =%n", &textStart) == 0 && textStart > 0)
        {
            char* text = line + textStart;
            while (*text == ' ')
                ++text;
            text[std::strcspn(text, "\r\n")] = 0;
            sHostMessage = CleanMessage(DecodeUtf8(text));
            continue;
        }
        if (std::sscanf(line, " %63[A-Za-z] = %d", key, &value) != 2)
            continue;
        if (std::strcmp(key, "Fov") == 0)
            sFov = Clamp(value, FOV_MIN, FOV_MAX);
        else if (std::strcmp(key, "RenderDistance") == 0)
            sRenderDistance = Clamp(value, RENDER_DISTANCE_MIN, RENDER_DISTANCE_MAX);
        else if (std::strcmp(key, "SimulationDistance") == 0)
            sSimulationDistance = Clamp(value, SIMULATION_DISTANCE_MIN, SIMULATION_DISTANCE_MAX);
        else if (std::strcmp(key, "FancyGraphics") == 0)
            sFancyGraphics = value != 0;
        else if (std::strcmp(key, "SmoothLighting") == 0)
            sSmoothLighting = value != 0;
        else if (std::strcmp(key, "Particles") == 0)
            sParticles = Clamp(value, EParticles_All, EParticles_Count - 1);
        else if (std::strcmp(key, "BiomeBlend") == 0)
            sBiomeBlend = Clamp(value, 0, BIOME_BLEND_MAX);
        else if (std::strcmp(key, "HostPort") == 0)
            sHostPort = Clamp(value, HOST_PORT_MIN, HOST_PORT_MAX);
        else if (std::strcmp(key, "HostVisible") == 0)
            sHostVisible = value != 0;
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
#if LCE_PLATFORM_PC
        static const Logger log("VideoSettings");
        LOG_WARN(log, "Video settings could not be written to %s", sPath.c_str());
#else
        std::fprintf(stderr, "video settings could not be written to %s\n", sPath.c_str());
#endif
        return;
    }
    std::fprintf(file, "Fov = %d\n", sFov);
    std::fprintf(file, "RenderDistance = %d\n", sRenderDistance);
    std::fprintf(file, "SimulationDistance = %d\n", sSimulationDistance);
    std::fprintf(file, "FancyGraphics = %d\n", sFancyGraphics ? 1 : 0);
    std::fprintf(file, "SmoothLighting = %d\n", sSmoothLighting ? 1 : 0);
    std::fprintf(file, "Particles = %d\n", sParticles);
    std::fprintf(file, "BiomeBlend = %d\n", sBiomeBlend);
    std::fprintf(file, "HostPort = %d\n", sHostPort);
    std::fprintf(file, "HostVisible = %d\n", sHostVisible ? 1 : 0);
    std::fprintf(file, "HostMessage = %s\n", EncodeUtf8(sHostMessage).c_str());
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
