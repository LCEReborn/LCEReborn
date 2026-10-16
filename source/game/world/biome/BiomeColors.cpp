#include "game/world/biome/BiomeColors.hpp"
#include "game/core/VideoSettings.hpp"

#include "game/world/LevelSource.hpp"
#include "game/world/biome/Biome.hpp"
#include "game/world/block/BlockPos.hpp"
#include "game/world/block/MutableBlockPos.hpp"

static BiomeColors::ColorResolver* GRASS_COLOR_RESOLVER = new BiomeColors::GrassColorResolver();
static BiomeColors::ColorResolver* FOLIAGE_COLOR_RESOLVER = new BiomeColors::FoliageColorResolver();
static BiomeColors::ColorResolver* WATER_COLOR_RESOLVER = new BiomeColors::WaterColorResolver();
static BiomeColors::ColorResolver* SKY_COLOR_RESOLVER = new BiomeColors::SkyColorResolver();

unsigned int BiomeColors::GrassColorResolver::getColor(Biome* biome, const BlockPos& pos)
{
    return biome->getGrassColor(pos);
}

unsigned int BiomeColors::FoliageColorResolver::getColor(Biome* biome, const BlockPos& pos)
{
    return biome->getFoliageColor(pos);
}

unsigned int BiomeColors::WaterColorResolver::getColor(Biome* biome, const BlockPos& pos)
{
    return biome->getWaterColor();
}

unsigned int BiomeColors::SkyColorResolver::getColor(Biome* biome, const BlockPos& pos)
{
    float temperature = biome->getTemperature(pos);
    return biome->getSkyColor(temperature);
}

unsigned int BiomeColors::getAverageColor(LevelSource* level, const BlockPos& pos, BiomeColors::ColorResolver* resolver)
{
    int alpha = 0;
    int red = 0;
    int green = 0;
    int blue = 0;

    MutableBlockPos samplePos;
    const int radius = VideoSettings::GetBiomeBlend();
    const float samples = static_cast<float>((radius * 2 + 1) * (radius * 2 + 1));

    for (int offsetZ = -radius; offsetZ <= radius; ++offsetZ)
    {
        for (int offsetX = -radius; offsetX <= radius; ++offsetX)
        {
            samplePos.setX(pos.x + offsetX);
            samplePos.setZ(pos.z + offsetZ);

            unsigned int colour = resolver->getColor(level->getBiome(samplePos), samplePos);

            alpha += (colour >> 24) & 0xFF;
            red += (colour >> 16) & 0xFF;
            green += (colour >> 8) & 0xFF;
            blue += colour & 0xFF;
        }
    }

    int averageAlpha = (int)(alpha / samples);
    int averageRed = (int)(red / samples);
    int averageGreen = (int)(green / samples);
    int averageBlue = (int)(blue / samples);

    return (unsigned int)((averageAlpha << 24) | ((averageRed & 0xFF) << 16) | ((averageGreen & 0xFF) << 8)
                          | (averageBlue & 0xFF));
}

unsigned int BiomeColors::getAverageGrassColor(LevelSource* level, const BlockPos& pos)
{
    return getAverageColor(level, pos, GRASS_COLOR_RESOLVER);
}

unsigned int BiomeColors::getAverageFoliageColor(LevelSource* level, const BlockPos& pos)
{
    return getAverageColor(level, pos, FOLIAGE_COLOR_RESOLVER);
}

unsigned int BiomeColors::getAverageWaterColor(LevelSource* level, const BlockPos& pos)
{
    return getAverageColor(level, pos, WATER_COLOR_RESOLVER);
}

unsigned int BiomeColors::getAverageSkyColor(LevelSource* level, const BlockPos& pos)
{
    return getAverageColor(level, pos, SKY_COLOR_RESOLVER);
}
