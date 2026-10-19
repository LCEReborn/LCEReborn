#include "BlockIcons.hpp"

#include <QCryptographicHash>
#include <QPainter>
#include <QTransform>

#include "GameArt.hpp"

namespace
{
const int kTile = 16;

struct Tile
{
    const char* name;
    int row;
    int column;
};

struct Block
{
    const char* name;
    Tile top;
    Tile front;
    Tile side;
};

const Block kBlocks[] = {
    {"stone", {"stone", 0, 1}, {"stone", 0, 1}, {"stone", 0, 1}},
    {"cobblestone", {"cobblestone", 1, 0}, {"cobblestone", 1, 0}, {"cobblestone", 1, 0}},
    {"dirt", {"dirt", 0, 2}, {"dirt", 0, 2}, {"dirt", 0, 2}},
    {"sand", {"sand", 1, 2}, {"sand", 1, 2}, {"sand", 1, 2}},
    {"gravel", {"gravel", 1, 3}, {"gravel", 1, 3}, {"gravel", 1, 3}},
    {"red_sand", {"red_sand", 20, 14}, {"red_sand", 20, 14}, {"red_sand", 20, 14}},
    {"coarse_dirt", {"coarse_dirt", 22, 8}, {"coarse_dirt", 22, 8}, {"coarse_dirt", 22, 8}},
    {"clay", {"clay", 4, 8}, {"clay", 4, 8}, {"clay", 4, 8}},
    {"hardened_clay", {"hardened_clay", 16, 1}, {"hardened_clay", 16, 1}, {"hardened_clay", 16, 1}},
    {"planks_oak", {"planks_oak", 0, 4}, {"planks_oak", 0, 4}, {"planks_oak", 0, 4}},
    {"planks_spruce", {"planks_spruce", 12, 6}, {"planks_spruce", 12, 6}, {"planks_spruce", 12, 6}},
    {"planks_birch", {"planks_birch", 13, 6}, {"planks_birch", 13, 6}, {"planks_birch", 13, 6}},
    {"planks_jungle", {"planks_jungle", 12, 7}, {"planks_jungle", 12, 7}, {"planks_jungle", 12, 7}},
    {"planks_acacia", {"planks_acacia", 20, 10}, {"planks_acacia", 20, 10}, {"planks_acacia", 20, 10}},
    {"planks_big_oak", {"planks_big_oak", 21, 10}, {"planks_big_oak", 21, 10}, {"planks_big_oak", 21, 10}},
    {"log_oak", {"log_oak_top", 1, 5}, {"log_oak", 1, 4}, {"log_oak", 1, 4}},
    {"log_spruce", {"log_spruce_top", 16, 13}, {"log_spruce", 7, 4}, {"log_spruce", 7, 4}},
    {"log_birch", {"log_birch_top", 16, 15}, {"log_birch", 7, 5}, {"log_birch", 7, 5}},
    {"log_jungle", {"log_jungle_top", 16, 14}, {"log_jungle", 9, 9}, {"log_jungle", 9, 9}},
    {"log_acacia", {"log_acacia_top", 20, 9}, {"log_acacia", 20, 8}, {"log_acacia", 20, 8}},
    {"log_big_oak", {"log_big_oak_top", 21, 9}, {"log_big_oak", 21, 8}, {"log_big_oak", 21, 8}},
    {"brick", {"brick", 0, 7}, {"brick", 0, 7}, {"brick", 0, 7}},
    {"bookshelf", {"planks_oak", 0, 4}, {"bookshelf", 2, 3}, {"bookshelf", 2, 3}},
    {"tnt", {"tnt_top", 0, 9}, {"tnt_side", 0, 8}, {"tnt_side", 0, 8}},
    {"crafting_table", {"workbench_top", 2, 11}, {"workbench_front", 3, 12}, {"workbench_side", 3, 11}},
    {"iron_block", {"iron_block", 1, 6}, {"iron_block", 1, 6}, {"iron_block", 1, 6}},
    {"gold_block", {"gold_block", 1, 7}, {"gold_block", 1, 7}, {"gold_block", 1, 7}},
    {"diamond_block", {"diamond_block", 1, 8}, {"diamond_block", 1, 8}, {"diamond_block", 1, 8}},
    {"emerald_block", {"emerald_block", 1, 9}, {"emerald_block", 1, 9}, {"emerald_block", 1, 9}},
    {"redstone_block", {"redstone_block", 1, 10}, {"redstone_block", 1, 10}, {"redstone_block", 1, 10}},
    {"lapis_block", {"lapis_block", 9, 0}, {"lapis_block", 9, 0}, {"lapis_block", 9, 0}},
    {"coal_block", {"coal_block", 16, 0}, {"coal_block", 16, 0}, {"coal_block", 16, 0}},
    {"gold_ore", {"gold_ore", 2, 0}, {"gold_ore", 2, 0}, {"gold_ore", 2, 0}},
    {"iron_ore", {"iron_ore", 2, 1}, {"iron_ore", 2, 1}, {"iron_ore", 2, 1}},
    {"coal_ore", {"coal_ore", 2, 2}, {"coal_ore", 2, 2}, {"coal_ore", 2, 2}},
    {"diamond_ore", {"diamond_ore", 3, 2}, {"diamond_ore", 3, 2}, {"diamond_ore", 3, 2}},
    {"redstone_ore", {"redstone_ore", 3, 3}, {"redstone_ore", 3, 3}, {"redstone_ore", 3, 3}},
    {"lapis_ore", {"lapis_ore", 10, 0}, {"lapis_ore", 10, 0}, {"lapis_ore", 10, 0}},
    {"emerald_ore", {"emerald_ore", 10, 11}, {"emerald_ore", 10, 11}, {"emerald_ore", 10, 11}},
    {"quartz_ore", {"quartz_ore", 11, 15}, {"quartz_ore", 11, 15}, {"quartz_ore", 11, 15}},
    {"obsidian", {"obsidian", 2, 5}, {"obsidian", 2, 5}, {"obsidian", 2, 5}},
    {"sponge", {"sponge", 3, 0}, {"sponge", 3, 0}, {"sponge", 3, 0}},
    {"stonebrick", {"stonebrick", 3, 6}, {"stonebrick", 3, 6}, {"stonebrick", 3, 6}},
    {"stonebrick_mossy", {"stonebrick_mossy", 6, 4}, {"stonebrick_mossy", 6, 4}, {"stonebrick_mossy", 6, 4}},
    {"cobblestone_mossy", {"cobblestone_mossy", 2, 4}, {"cobblestone_mossy", 2, 4}, {"cobblestone_mossy", 2, 4}},
    {"netherrack", {"netherrack", 6, 7}, {"netherrack", 6, 7}, {"netherrack", 6, 7}},
    {"soul_sand", {"soul_sand", 6, 8}, {"soul_sand", 6, 8}, {"soul_sand", 6, 8}},
    {"glowstone", {"glowstone", 6, 9}, {"glowstone", 6, 9}, {"glowstone", 6, 9}},
    {"end_stone", {"end_stone", 10, 15}, {"end_stone", 10, 15}, {"end_stone", 10, 15}},
    {"end_bricks", {"end_bricks", 23, 8}, {"end_bricks", 23, 8}, {"end_bricks", 23, 8}},
    {"nether_brick", {"nether_brick", 14, 0}, {"nether_brick", 14, 0}, {"nether_brick", 14, 0}},
    {"red_nether_brick", {"red_nether_brick", 24, 11}, {"red_nether_brick", 24, 11}, {"red_nether_brick", 24, 11}},
    {"nether_wart_block", {"nether_wart_block", 24, 10}, {"nether_wart_block", 24, 10}, {"nether_wart_block", 24, 10}},
    {"purpur_block", {"purpur_block", 24, 5}, {"purpur_block", 24, 5}, {"purpur_block", 24, 5}},
    {"prismarine_bricks", {"prismarine_bricks", 20, 13}, {"prismarine_bricks", 20, 13}, {"prismarine_bricks", 20, 13}},
    {"prismarine_dark", {"prismarine_dark", 21, 13}, {"prismarine_dark", 21, 13}, {"prismarine_dark", 21, 13}},
    {"quartz_block", {"quartz_block_top", 13, 11}, {"quartz_block_side", 14, 11}, {"quartz_block_side", 14, 11}},
    {"hay_block", {"hay_block_top", 15, 13}, {"hay_block_side", 15, 10}, {"hay_block_side", 15, 10}},
    {"melon", {"melon_top", 8, 9}, {"melon_side", 8, 8}, {"melon_side", 8, 8}},
    {"pumpkin", {"pumpkin_top", 6, 6}, {"pumpkin_face_off", 7, 7}, {"pumpkin_side", 7, 6}},
    {"sandstone", {"sandstone_top", 11, 0}, {"sandstone_side", 12, 0}, {"sandstone_side", 12, 0}},
    {"red_sandstone", {"red_sandstone_top", 20, 15}, {"red_sandstone_normal", 21, 15}, {"red_sandstone_normal", 21, 15}},
    {"bone_block", {"bone_block_top", 26, 1}, {"bone_block_side", 26, 0}, {"bone_block_side", 26, 0}},
    {"mycelium", {"mycel_top", 4, 14}, {"mycel_side", 4, 13}, {"mycel_side", 4, 13}},
    {"packed_ice", {"ice_packed", 23, 12}, {"ice_packed", 23, 12}, {"ice_packed", 23, 12}},
    {"snow", {"snow", 4, 2}, {"snow", 4, 2}, {"snow", 4, 2}},
    {"jukebox", {"jukebox_top", 4, 11}, {"jukebox_side", 4, 10}, {"jukebox_side", 4, 10}},
    {"noteblock", {"noteblock", 16, 2}, {"noteblock", 16, 2}, {"noteblock", 16, 2}},
    {"furnace", {"furnace_top", 3, 14}, {"furnace_front", 2, 12}, {"furnace_side", 2, 13}},
    {"wool_white", {"wool_colored_white", 4, 0}, {"wool_colored_white", 4, 0}, {"wool_colored_white", 4, 0}},
    {"wool_red", {"wool_colored_red", 8, 1}, {"wool_colored_red", 8, 1}, {"wool_colored_red", 8, 1}},
    {"wool_lime", {"wool_colored_lime", 9, 2}, {"wool_colored_lime", 9, 2}, {"wool_colored_lime", 9, 2}},
    {"wool_light_blue", {"wool_colored_light_blue", 11, 2}, {"wool_colored_light_blue", 11, 2}, {"wool_colored_light_blue", 11, 2}},
    {"wool_orange", {"wool_colored_orange", 13, 2}, {"wool_colored_orange", 13, 2}, {"wool_colored_orange", 13, 2}},
    {"wool_purple", {"wool_colored_purple", 12, 1}, {"wool_colored_purple", 12, 1}, {"wool_colored_purple", 12, 1}},
    {"glazed_terracotta_orange", {"glazed_terracotta_orange", 29, 9}, {"glazed_terracotta_orange", 29, 9}, {"glazed_terracotta_orange", 29, 9}},
    {"glazed_terracotta_magenta", {"glazed_terracotta_magenta", 29, 8}, {"glazed_terracotta_magenta", 29, 8}, {"glazed_terracotta_magenta", 29, 8}},
    {"glazed_terracotta_cyan", {"glazed_terracotta_cyan", 29, 3}, {"glazed_terracotta_cyan", 29, 3}, {"glazed_terracotta_cyan", 29, 3}},
    {"glazed_terracotta_yellow", {"glazed_terracotta_yellow", 29, 15}, {"glazed_terracotta_yellow", 29, 15}, {"glazed_terracotta_yellow", 29, 15}},
    {"granite", {"stone_granite", 16, 7}, {"stone_granite", 16, 7}, {"stone_granite", 16, 7}},
    {"diorite", {"stone_diorite", 16, 5}, {"stone_diorite", 16, 5}, {"stone_diorite", 16, 5}},
    {"andesite", {"stone_andesite", 16, 3}, {"stone_andesite", 16, 3}, {"stone_andesite", 16, 3}},
};

quint32 Score(const QString& commit, const char* block)
{
    const QByteArray digest = QCryptographicHash::hash((commit + ':' + block).toUtf8(), QCryptographicHash::Sha1);
    return (static_cast<quint32>(static_cast<quint8>(digest[0])) << 24)
           | (static_cast<quint32>(static_cast<quint8>(digest[1])) << 16)
           | (static_cast<quint32>(static_cast<quint8>(digest[2])) << 8) | static_cast<quint8>(digest[3]);
}

const Block& Pick(const QString& commit)
{
    const QString key = commit.trimmed().toLower();
    const Block* best = &kBlocks[0];
    quint32 bestScore = Score(key, best->name);
    for (const Block& block : kBlocks)
    {
        const quint32 score = Score(key, block.name);
        if (score > bestScore)
        {
            best = &block;
            bestScore = score;
        }
    }
    return *best;
}

QImage Shaded(const QImage& face, double shade)
{
    QImage image = face.convertToFormat(QImage::Format_ARGB32);
    for (int y = 0; y < image.height(); ++y)
    {
        QRgb* line = reinterpret_cast<QRgb*>(image.scanLine(y));
        for (int x = 0; x < image.width(); ++x)
        {
            const QRgb pixel = line[x];
            line[x] = qRgba(static_cast<int>(qRed(pixel) * shade), static_cast<int>(qGreen(pixel) * shade),
                            static_cast<int>(qBlue(pixel) * shade), qAlpha(pixel));
        }
    }
    return image;
}

QImage TileImage(const QImage& atlas, const Tile& tile)
{
    return atlas.copy(tile.column * kTile, tile.row * kTile, kTile, kTile);
}
}

namespace BlockIcons
{
QString BlockFor(const QString& commit)
{
    return Pick(commit).name;
}

QImage RenderCube(const QImage& top, const QImage& front, const QImage& side, int size)
{
    QImage image(size, size, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    if (top.isNull() || front.isNull() || side.isNull())
        return image;

    const double half = size / 2.0;
    const double quarter = size / 4.0;
    QPainter painter(&image);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    const struct
    {
        const QImage& face;
        double shade;
        QTransform transform;
    } faces[] = {
        {top, 1.0, QTransform(half / top.width(), -quarter / top.width(), half / top.height(), quarter / top.height(), 0,
                              quarter)},
        {side, 0.6, QTransform(half / side.width(), quarter / side.width(), 0, half / side.height(), 0, quarter)},
        {front, 0.8, QTransform(half / front.width(), -quarter / front.width(), 0, half / front.height(), half,
                                2 * quarter)},
    };
    for (const auto& face : faces)
    {
        painter.setTransform(face.transform);
        painter.drawImage(0, 0, Shaded(face.face, face.shade));
    }
    return image;
}

QImage Render(const QString& commit, int size)
{
    const QImage atlas = GameArt::Terrain();
    if (atlas.isNull() || commit.isEmpty())
        return QImage();
    const Block& block = Pick(commit);
    return RenderCube(TileImage(atlas, block.top), TileImage(atlas, block.front), TileImage(atlas, block.side), size);
}
}
