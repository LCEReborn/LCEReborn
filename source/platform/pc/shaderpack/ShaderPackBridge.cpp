#include "game/client/render/ShaderPackBridge.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include "game/client/render/BlockRenderer.hpp"
#include "game/client/render/GameRenderer.hpp"
#include "game/client/render/LevelRenderer.hpp"
#include "game/client/render/Renderer.hpp"
#include "game/client/render/TexturePack.hpp"
#include "game/client/render/TexturePackRepository.hpp"
#include "game/client/render/Textures.hpp"
#include "game/client/render/Texture.hpp"
#include "game/client/render/TextureAtlas.hpp"
#include "game/core/FoodData.hpp"
#include "game/core/GameType.hpp"
#include "game/core/HumanoidArm.hpp"
#include "game/core/Minecraft.hpp"
#include "game/core/Options.hpp"
#include "game/core/VideoSettings.hpp"
#include "game/entity/mob/LivingEntity.hpp"
#include "game/entity/mob/MobEffects.hpp"
#include "game/item/Item.hpp"
#include "game/item/ItemInstance.hpp"
#include "game/util/ResourceLocation.hpp"
#include "game/util/Vec3.hpp"
#include "game/world/Dimension.hpp"
#include "game/world/DimensionType.hpp"
#include "game/world/Level.hpp"
#include "game/world/LevelSource.hpp"
#include "game/world/biome/Biome.hpp"
#include "game/world/block/Block.hpp"
#include "game/world/block/BlockEntity.hpp"
#include "game/world/block/BlockPos.hpp"
#include "game/world/block/BlockState.hpp"
#include "game/world/block/IdMapper.hpp"
#include "game/world/block/Material.hpp"
#include "game/world/block/Property.hpp"
#include "game/world/generation/MultiPlayerGameMode.hpp"
#include "game/world/generation/MultiPlayerLevel.hpp"
#include "game/world/generation/MultiplayerLocalPlayer.hpp"
#include "platform/pc/Log.hpp"
#include "platform/pc/PcPaths.hpp"
#include "platform/pc/shaderpack/ShaderPackManager.hpp"
#include "platform/pc/shaderpack/ShaderPackProperties.hpp"
#include "platform/pc/shaderpack/ShaderPipeline.hpp"
#include "platform/pc/shaderpack/ShaderPreprocessor.hpp"

static const Logger sLog("Shaders");

namespace
{
struct BlockRule
{
    int id;
    std::vector<std::pair<std::string, std::vector<std::string>>> properties;
    std::vector<int> metas;
};

struct Mappings
{
    std::vector<int> blocks;
    std::map<std::string, int> entities;
    std::map<std::string, int> items;
};

Mappings sMappings;
const ShaderPack* sMappedPack = nullptr;
std::string sMappedOptions;
bool sBiomeConstantsSet = false;
bool sActive = false;
bool sPackFrame = false;

std::string Narrow(const std::wstring& text)
{
    std::string result;
    result.reserve(text.size());
    for (wchar_t character : text)
        result.push_back(character < 128 ? static_cast<char>(character) : '?');
    return result;
}

std::string Lower(std::string text)
{
    for (char& character : text)
        character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    return text;
}

std::string Upper(std::string text)
{
    for (char& character : text)
        character = static_cast<char>(std::toupper(static_cast<unsigned char>(character)));
    return text;
}

bool IsNumber(const std::string& text)
{
    if (text.empty())
        return false;
    for (char character : text)
    {
        if (!std::isdigit(static_cast<unsigned char>(character)))
            return false;
    }
    return true;
}

std::vector<std::string> SplitOn(const std::string& text, char separator)
{
    std::vector<std::string> parts;
    size_t start = 0;
    for (;;)
    {
        size_t end = text.find(separator, start);
        parts.push_back(text.substr(start, end == std::string::npos ? std::string::npos : end - start));
        if (end == std::string::npos)
            break;
        start = end + 1;
    }
    return parts;
}

std::string QualifiedName(const std::string& name)
{
    std::string lower = Lower(name);
    return lower.find(':') == std::string::npos ? "minecraft:" + lower : lower;
}

ShaderPropertyList LoadMappingFile(const char* file)
{
    ShaderPackManager& manager = ShaderPackManager::Get();
    ShaderPack* pack = manager.ActivePack();
    std::string text;
    if (pack == nullptr || !pack->ReadFile(file, text))
        return ShaderPropertyList();
    ShaderMacros macros = manager.StandardMacros();
    for (const auto& option : manager.Options().Macros())
        macros[option.first] = option.second;
    return ShaderParseProperties(ShaderPreprocessConditionals(text, macros));
}

std::vector<std::string> ModernNames(const std::string& name, std::map<std::string, std::string>& values)
{
    static const std::map<std::string, std::string> kRenames = {
        {"grass", "grass_block"}, {"deadbush", "dead_bush"}, {"yellow_flower", "dandelion"}, {"snow", "snow_block"},
        {"snow_layer", "snow"}, {"brick_block", "bricks"}, {"nether_brick", "nether_bricks"},
        {"red_nether_brick", "red_nether_bricks"}, {"hardened_clay", "terracotta"}, {"reeds", "sugar_cane"},
        {"waterlily", "lily_pad"}, {"web", "cobweb"}, {"portal", "nether_portal"}, {"magma", "magma_block"},
        {"mob_spawner", "spawner"}, {"lit_pumpkin", "jack_o_lantern"}, {"pumpkin", "carved_pumpkin"},
        {"melon_block", "melon"}, {"quartz_ore", "nether_quartz_ore"}, {"lit_furnace", "furnace"},
        {"lit_redstone_ore", "redstone_ore"}, {"lit_redstone_lamp", "redstone_lamp"},
        {"unlit_redstone_torch", "redstone_torch"}, {"powered_repeater", "repeater"}, {"unpowered_repeater", "repeater"},
        {"powered_comparator", "comparator"}, {"unpowered_comparator", "comparator"}, {"wooden_door", "oak_door"},
        {"fence", "oak_fence"}, {"fence_gate", "oak_fence_gate"}, {"wooden_pressure_plate", "oak_pressure_plate"},
        {"wooden_button", "oak_button"}, {"trapdoor", "oak_trapdoor"}, {"standing_sign", "oak_sign"},
        {"wall_sign", "oak_wall_sign"}, {"noteblock", "note_block"}, {"golden_rail", "powered_rail"},
        {"slime", "slime_block"}, {"flowing_water", "water"}, {"flowing_lava", "lava"},
        {"stone_stairs", "cobblestone_stairs"}, {"grass_path", "dirt_path"}, {"double_stone_slab", "stone_slab"},
        {"double_stone_slab2", "stone_slab2"}, {"double_wooden_slab", "wooden_slab"}, {"purpur_double_slab", "purpur_slab"},
        {"silver_glazed_terracotta", "light_gray_glazed_terracotta"}, {"silver_shulker_box", "light_gray_shulker_box"}};
    static const std::map<std::string, std::string> kVariants = {
        {"tallgrass|tall_grass", "short_grass"}, {"tallgrass|fern", "fern"}, {"tallgrass|dead_bush", "dead_bush"},
        {"red_flower|houstonia", "azure_bluet"}, {"double_plant|syringa", "lilac"},
        {"double_plant|double_grass", "tall_grass"}, {"double_plant|double_fern", "large_fern"},
        {"double_plant|double_rose", "rose_bush"}, {"double_plant|paeonia", "peony"},
        {"stone|smooth_granite", "polished_granite"}, {"stone|smooth_diorite", "polished_diorite"},
        {"stone|smooth_andesite", "polished_andesite"}, {"sandstone|smooth_sandstone", "cut_sandstone"},
        {"red_sandstone|smooth_red_sandstone", "cut_red_sandstone"}, {"stonebrick|stonebrick", "stone_bricks"},
        {"stonebrick|mossy_stonebrick", "mossy_stone_bricks"}, {"stonebrick|cracked_stonebrick", "cracked_stone_bricks"},
        {"stonebrick|chiseled_stonebrick", "chiseled_stone_bricks"}, {"quartz_block|default", "quartz_block"},
        {"quartz_block|chiseled", "chiseled_quartz_block"}, {"quartz_block|lines_x", "quartz_pillar"},
        {"quartz_block|lines_y", "quartz_pillar"}, {"quartz_block|lines_z", "quartz_pillar"},
        {"stone_slab|stone", "smooth_stone_slab"}, {"stone_slab|sand", "sandstone_slab"},
        {"stone_slab|wood_old", "petrified_oak_slab"}, {"stone_slab|cobblestone", "cobblestone_slab"},
        {"stone_slab|brick", "brick_slab"}, {"stone_slab|stone_brick", "stone_brick_slab"},
        {"stone_slab|nether_brick", "nether_brick_slab"}, {"stone_slab|quartz", "quartz_slab"},
        {"stone_slab2|red_sandstone", "red_sandstone_slab"}, {"brown_mushroom_block|stem", "mushroom_stem"},
        {"red_mushroom_block|stem", "mushroom_stem"}, {"cobblestone_wall|normal", "cobblestone_wall"},
        {"cobblestone_wall|mossy", "mossy_cobblestone_wall"}, {"prismarine|rough", "prismarine"},
        {"prismarine|bricks", "prismarine_bricks"}, {"prismarine|dark", "dark_prismarine"},
        {"monster_egg|stone", "infested_stone"}, {"monster_egg|cobblestone", "infested_cobblestone"},
        {"monster_egg|stone_brick", "infested_stone_bricks"}, {"monster_egg|mossy_brick", "infested_mossy_stone_bricks"},
        {"monster_egg|cracked_brick", "infested_cracked_stone_bricks"},
        {"monster_egg|chiseled_brick", "infested_chiseled_stone_bricks"}};

    if (name.compare(0, 4, "lit_") == 0 || name == "redstone_torch")
        values.insert(std::make_pair("lit", "true"));
    else if (name == "furnace" || name == "redstone_ore" || name == "redstone_lamp" || name == "unlit_redstone_torch")
        values.insert(std::make_pair("lit", "false"));
    if (name.compare(0, 8, "powered_") == 0)
        values.insert(std::make_pair("powered", "true"));
    else if (name.compare(0, 10, "unpowered_") == 0)
        values.insert(std::make_pair("powered", "false"));

    std::vector<std::string> names;
    auto facing = values.find("facing");
    bool wall = facing != values.end() && facing->second != "up";
    if (name == "torch" && wall)
        names.push_back("wall_torch");
    if ((name == "redstone_torch" || name == "unlit_redstone_torch") && wall)
        names.push_back("redstone_wall_torch");
    if (name == "sponge")
    {
        auto wet = values.find("wet");
        if (wet != values.end() && wet->second == "true")
            names.push_back("wet_sponge");
    }

    std::string variant;
    for (const char* key : {"variant", "type", "color"})
    {
        auto found = values.find(key);
        if (found != values.end())
        {
            variant = found->second == "silver" ? "light_gray" : found->second;
            break;
        }
    }
    auto renamed = kRenames.find(name);
    std::string canonical = renamed != kRenames.end() && name.compare(0, 7, "double_") == 0 ? renamed->second : name;
    if (!variant.empty())
    {
        auto special = kVariants.find(canonical + "|" + variant);
        if (special != kVariants.end())
        {
            names.push_back(special->second);
        }
        else
        {
            std::string base = canonical;
            while (!base.empty() && std::isdigit(static_cast<unsigned char>(base.back())))
                base.pop_back();
            if (base == "stained_hardened_clay")
                base = "terracotta";
            else if (base == "wooden_slab")
                base = "slab";
            auto axis = values.find("axis");
            if (base == "log" && axis != values.end() && axis->second == "none")
                names.push_back(variant + "_wood");
            names.push_back(variant + "_" + base);
            names.push_back(variant);
        }
    }
    if (name == "tallgrass")
    {
        names.push_back("tallgrass");
        names.push_back("grass");
    }
    if (renamed != kRenames.end())
        names.push_back(renamed->second);
    if (name != "grass" && name != "snow")
        names.push_back(name);
    return names;
}

bool UsesModernNames(const std::map<std::string, std::vector<BlockRule>>& named)
{
    static const char* const kMarkers[] = {"grass_block", "short_grass", "oak_log",   "oak_planks", "oak_leaves",
                                           "stone_bricks", "white_wool", "dandelion", "poppy",      "sugar_cane",
                                           "lily_pad",    "cobweb",     "bricks",    "terracotta", "granite"};
    for (const char* marker : kMarkers)
    {
        if (named.count(std::string("minecraft:") + marker) != 0)
            return true;
    }
    return false;
}

void BuildBlockMap()
{
    std::map<std::string, std::vector<BlockRule>> named;
    std::map<int, std::vector<BlockRule>> numbered;
    for (const auto& entry : LoadMappingFile("block.properties"))
    {
        if (entry.first.compare(0, 6, "block.") != 0)
            continue;
        int id = std::atoi(entry.first.c_str() + 6);
        for (const std::string& token : ShaderSplitList(entry.second))
        {
            std::vector<std::string> parts = SplitOn(token, ':');
            BlockRule rule;
            rule.id = id;
            if (IsNumber(parts[0]))
            {
                for (size_t index = 1; index < parts.size(); ++index)
                {
                    for (const std::string& meta : SplitOn(parts[index], ','))
                    {
                        if (IsNumber(meta))
                            rule.metas.push_back(std::atoi(meta.c_str()));
                    }
                }
                numbered[std::atoi(parts[0].c_str())].push_back(rule);
                continue;
            }
            size_t first = 1;
            std::string name = "minecraft:" + Lower(parts[0]);
            if (parts.size() >= 2 && parts[1].find('=') == std::string::npos && !IsNumber(SplitOn(parts[1], ',')[0]))
            {
                name = Lower(parts[0]) + ":" + Lower(parts[1]);
                first = 2;
            }
            for (size_t index = first; index < parts.size(); ++index)
            {
                size_t equals = parts[index].find('=');
                if (equals != std::string::npos)
                {
                    rule.properties.push_back(std::make_pair(Lower(parts[index].substr(0, equals)),
                                                             SplitOn(Lower(parts[index].substr(equals + 1)), ',')));
                    continue;
                }
                for (const std::string& meta : SplitOn(parts[index], ','))
                {
                    if (IsNumber(meta))
                        rule.metas.push_back(std::atoi(meta.c_str()));
                }
            }
            named[name].push_back(rule);
        }
    }

    bool modern = UsesModernNames(named);
    sMappings.blocks.assign(65536, -1);
    for (int element = 1; element < IdMapper<const BlockState*>::BLOCK_STATE_COUNT; ++element)
    {
        if (Block::byId(element >> 4) == nullptr)
            continue;
        const BlockState* state = Block::getStateByElementId(static_cast<unsigned short>(element));
        if (state == nullptr || state->getBlock() == nullptr)
            continue;
        Block* block = state->getBlock();
        int blockId = element >> 4;
        int meta = element & 15;
        std::string name = Lower(Narrow(Block::blockRegistry->getKey(block).toString()));

        std::map<std::string, std::string> values;
        for (const Property* property : state->getProperties())
        {
            Boxed* value = state->getBoxedValue(property);
            if (value != nullptr)
                values[Lower(Narrow(property->getName()))] = Lower(Narrow(property->getName(value)));
        }

        auto matches = [&](const BlockRule& rule) {
            if (!rule.metas.empty() && std::find(rule.metas.begin(), rule.metas.end(), meta) == rule.metas.end())
                return false;
            for (const auto& filter : rule.properties)
            {
                auto found = values.find(filter.first);
                if (found == values.end())
                    return false;
                if (std::find(filter.second.begin(), filter.second.end(), found->second) == filter.second.end())
                    return false;
            }
            return true;
        };

        std::vector<std::string> candidates;
        if (modern && name.compare(0, 10, "minecraft:") == 0)
        {
            for (const std::string& candidate : ModernNames(name.substr(10), values))
                candidates.push_back("minecraft:" + candidate);
        }
        else
        {
            candidates.push_back(name);
        }

        int mapped = -1;
        for (const std::string& candidate : candidates)
        {
            auto byName = named.find(candidate);
            if (byName == named.end())
                continue;
            for (const BlockRule& rule : byName->second)
            {
                if (matches(rule))
                {
                    mapped = rule.id;
                    break;
                }
            }
            if (mapped >= 0)
                break;
        }
        if (mapped < 0)
        {
            auto byNumber = numbered.find(blockId);
            if (byNumber != numbered.end())
            {
                for (const BlockRule& rule : byNumber->second)
                {
                    if (matches(rule))
                    {
                        mapped = rule.id;
                        break;
                    }
                }
            }
        }
        sMappings.blocks[static_cast<size_t>(element)] = mapped;
    }
    for (int element = 0x4010; element < 0x8000; ++element)
    {
        int aliased = element - 0x3000;
        if (aliased < IdMapper<const BlockState*>::BLOCK_STATE_COUNT)
            sMappings.blocks[static_cast<size_t>(element)] = sMappings.blocks[static_cast<size_t>(aliased)];
    }
}

void BuildNameMap(const char* file, const char* prefix, std::map<std::string, int>& map)
{
    map.clear();
    size_t prefixLength = std::char_traits<char>::length(prefix);
    for (const auto& entry : LoadMappingFile(file))
    {
        if (entry.first.compare(0, prefixLength, prefix) != 0)
            continue;
        int id = std::atoi(entry.first.c_str() + prefixLength);
        for (const std::string& token : ShaderSplitList(entry.second))
        {
            std::string name = QualifiedName(token);
            if (map.find(name) == map.end())
                map[name] = id;
        }
    }
}

void SetBiomeConstants()
{
    if (sBiomeConstantsSet)
        return;
    std::map<std::string, float> constants;
    for (int id = 0; id <= Biome::MAX_BIOME_ID; ++id)
    {
        Biome* biome = Biome::biomes[id];
        if (biome == nullptr)
            continue;
        std::string name = Upper(Narrow(biome->mBiomeName));
        if (!name.empty())
            constants["BIOME_" + name] = static_cast<float>(id);
    }
    static const char* const kAliases[][2] = {
        {"ICE_FLATS", "ICE_PLAINS"},
        {"MUTATED_ICE_FLATS", "ICE_PLAINS_SPIKES"},
        {"TAIGA_COLD", "COLD_TAIGA"},
        {"TAIGA_COLD_HILLS", "COLD_TAIGA_HILLS"},
        {"MUTATED_TAIGA_COLD", "COLD_TAIGA_MUTATED"},
        {"REDWOOD_TAIGA", "MEGA_TAIGA"},
        {"REDWOOD_TAIGA_HILLS", "MEGA_TAIGA_HILLS"},
        {"MUTATED_REDWOOD_TAIGA", "MEGA_SPRUCE_TAIGA"},
        {"MUTATED_REDWOOD_TAIGA_HILLS", "REDWOOD_TAIGA_HILLS_MUTATED"},
        {"SMALLER_EXTREME_HILLS", "EXTREME_HILLS_EDGE"},
        {"EXTREME_HILLS_WITH_TREES", "EXTREME_HILLS_PLUS_TREES"},
        {"MUTATED_EXTREME_HILLS", "EXTREME_HILLS_MUTATED"},
        {"MUTATED_EXTREME_HILLS_WITH_TREES", "EXTREME_HILLS_PLUS_TREES_MUTATED"},
        {"BEACHES", "BEACH"},
        {"MESA_ROCK", "MESA_PLATEAU_STONE"},
        {"MESA_CLEAR_ROCK", "MESA_PLATEAU"},
        {"MUTATED_MESA", "MESA_BRYCE"},
        {"MUTATED_MESA_ROCK", "MESA_PLATEAU_STONE_MUTATED"},
        {"MUTATED_MESA_CLEAR_ROCK", "MESA_PLATEAU_MUTATED"},
        {"SAVANNA_ROCK", "SAVANNA_PLATEAU"},
        {"MUTATED_SAVANNA", "SAVANNA_MUTATED"},
        {"MUTATED_SAVANNA_ROCK", "SAVANNA_PLATEAU_MUTATED"},
        {"MUTATED_PLAINS", "SUNFLOWER_PLAINS"},
        {"MUTATED_DESERT", "DESERT_MUTATED"},
        {"MUTATED_FOREST", "FLOWER_FOREST"},
        {"MUTATED_TAIGA", "TAIGA_MUTATED"},
        {"MUTATED_SWAMPLAND", "SWAMPLAND_MUTATED"},
        {"MUTATED_JUNGLE", "JUNGLE_MUTATED"},
        {"MUTATED_JUNGLE_EDGE", "JUNGLE_EDGE_MUTATED"},
        {"MUTATED_BIRCH_FOREST", "BIRCH_FOREST_MUTATED"},
        {"MUTATED_BIRCH_FOREST_HILLS", "BIRCH_FOREST_HILLS_MUTATED"},
        {"MUTATED_ROOFED_FOREST", "ROOFED_FOREST_MUTATED"},
        {"VOID", "THE_VOID"},
        {"SKY", "THE_END"},
        {"SNOWY_PLAINS", "ICE_PLAINS"},
        {"ICE_SPIKES", "ICE_PLAINS_SPIKES"},
        {"SNOWY_TAIGA", "COLD_TAIGA"},
        {"SNOWY_BEACH", "COLD_BEACH"},
        {"OLD_GROWTH_PINE_TAIGA", "MEGA_TAIGA"},
        {"OLD_GROWTH_SPRUCE_TAIGA", "MEGA_SPRUCE_TAIGA"},
        {"WINDSWEPT_HILLS", "EXTREME_HILLS"},
        {"WINDSWEPT_FOREST", "EXTREME_HILLS_PLUS_TREES"},
        {"WINDSWEPT_GRAVELLY_HILLS", "EXTREME_HILLS_MUTATED"},
        {"WINDSWEPT_SAVANNA", "SAVANNA_MUTATED"},
        {"STONY_SHORE", "STONE_BEACH"},
        {"MUSHROOM_FIELDS", "MUSHROOM_ISLAND"},
        {"DARK_FOREST", "ROOFED_FOREST"},
        {"OLD_GROWTH_BIRCH_FOREST", "BIRCH_FOREST_MUTATED"},
        {"SPARSE_JUNGLE", "JUNGLE_EDGE"},
        {"BADLANDS", "MESA"},
        {"WOODED_BADLANDS", "MESA_PLATEAU_STONE"},
        {"ERODED_BADLANDS", "MESA_BRYCE"},
        {"SWAMP", "SWAMPLAND"},
        {"NETHER_WASTES", "HELL"},
        {"SMALL_END_ISLANDS", "THE_END"},
        {"END_MIDLANDS", "THE_END"},
        {"END_HIGHLANDS", "THE_END"},
        {"END_BARRENS", "THE_END"}};
    for (const auto& alias : kAliases)
    {
        auto target = constants.find(std::string("BIOME_") + alias[1]);
        if (target != constants.end())
            constants.insert(std::make_pair(std::string("BIOME_") + alias[0], target->second));
    }
    static const char* const kCategories[] = {"NONE",   "TAIGA",  "EXTREME_HILLS", "JUNGLE", "MESA",  "PLAINS",
                                              "SAVANNA", "ICY",   "THE_END",       "BEACH",  "FOREST", "OCEAN",
                                              "DESERT", "RIVER", "SWAMP",         "MUSHROOM", "NETHER"};
    for (size_t index = 0; index < sizeof(kCategories) / sizeof(kCategories[0]); ++index)
        constants[std::string("CAT_") + kCategories[index]] = static_cast<float>(index);
    ShaderPipeline::Get().SetBiomeConstants(constants);
    sBiomeConstantsSet = true;
}

int BiomeCategory(Biome* biome)
{
    std::string name = Lower(Narrow(biome->mBiomeName));
    auto has = [&](const char* text) { return name.find(text) != std::string::npos; };
    if (has("ocean"))
        return 11;
    if (has("river"))
        return 13;
    if (has("beach") || has("shore"))
        return 9;
    if (has("mushroom"))
        return 15;
    if (has("hell") || has("nether"))
        return 16;
    if (has("sky") || has("end"))
        return 8;
    if (has("ice") || has("frozen"))
        return 7;
    if (has("desert"))
        return 12;
    if (has("mesa") || has("badlands"))
        return 4;
    if (has("savanna"))
        return 6;
    if (has("jungle"))
        return 3;
    if (has("taiga"))
        return 1;
    if (has("hills") || has("mountain"))
        return 2;
    if (has("swamp"))
        return 14;
    if (has("forest"))
        return 10;
    if (has("plains"))
        return 5;
    return 0;
}

int MappedItem(const not_null_ptr<ItemInstance>& item, int& light)
{
    light = 0;
    if (item->isEmpty() || item->getItem() == nullptr)
        return sMappings.items.count("minecraft:air") ? sMappings.items["minecraft:air"] : -1;
    Item* definition = item->getItem();
    std::string name = Lower(Narrow(Item::itemRegistry.getKey(definition).toString()));
    Block* block = Block::byItem(definition);
    if (block != nullptr)
        light = block->getLightEmission(block->defaultBlockState());
    auto found = sMappings.items.find(name);
    return found != sMappings.items.end() ? found->second : -1;
}

void RefreshMappings()
{
    ShaderPackManager& manager = ShaderPackManager::Get();
    const ShaderPack* pack = manager.ActivePack();
    std::string options = manager.Options().SaveValues();
    if (pack == sMappedPack && options == sMappedOptions)
        return;
    sMappedPack = pack;
    sMappedOptions = options;
    if (pack == nullptr)
        return;
    BuildBlockMap();
    BuildNameMap("entity.properties", "entity.", sMappings.entities);
    BuildNameMap("item.properties", "item.", sMappings.items);
    ShaderPipeline::Get().SetBlockMap(sMappings.blocks);
}
}

namespace ShaderPackBridge
{
static GLuint ResolveResourceTexture(const std::string& name)
{
    Minecraft* minecraft = Minecraft::GetInstance();
    Textures* textures = minecraft->getTextureManager();
    if (textures == nullptr)
        return 0;
    if (name == "atlas/blocks")
        return gRenderer->TextureGlName(textures->loadTexture(BLOCKS_LOCATION.getTexture()));

    static std::map<std::string, bool> sAvailable;
    auto found = sAvailable.find(name);
    if (found == sAvailable.end())
    {
        std::wstring wide(name.begin(), name.end());
        std::error_code error;
        bool available = std::filesystem::exists(PcContentRoot() + "/Common/res/1_2_2/" + name + ".png", error);
        TexturePack* selected = minecraft->mTexturePackRepository != nullptr
                                    ? minecraft->mTexturePackRepository->getSelected()
                                    : nullptr;
        if (!available && selected != nullptr && selected != TexturePackRepository::defaultTexturePack)
            available = selected->hasFile(L"res/" + wide + L".png", false);
        if (!available)
            LOG_WARN(sLog, "resource texture %s is not a game texture", name.c_str());
        found = sAvailable.insert(std::make_pair(name, available)).first;
    }
    if (!found->second)
        return 0;
    std::wstring wide(name.begin(), name.end());
    return gRenderer->TextureGlName(textures->loadTexture(TN_UNINDEXED, wide));
}

static void SyncBlockLighting(bool packActive)
{
    ShaderPipeline& pipeline = ShaderPipeline::Get();
    bool oldLighting = !packActive || pipeline.OldLighting();
    bool separateAo = packActive && pipeline.SeparateAo();
    if (!BlockRenderer::SetShaderLighting(oldLighting, separateAo))
        return;
    Minecraft* minecraft = Minecraft::GetInstance();
    if (minecraft->mLevel != nullptr && minecraft->mPlayer != nullptr && minecraft->mLevelRenderer != nullptr)
        minecraft->mLevelRenderer->allChanged();
}

static bool BeginPackWorld(GameRenderer* gameRenderer, float partialTick)
{
    sActive = false;
    sPackFrame = false;
    Minecraft* minecraft = Minecraft::GetInstance();
    ShaderPackManager& manager = ShaderPackManager::Get();
    bool internal = manager.Settings().UsesInternal();
    bool packReady = manager.Settings().UsesPack() && manager.ActivePack() != nullptr;
    if (!internal && !packReady)
    {
        if (ShaderPipeline::Get().Enabled())
            ShaderPipeline::Get().Shutdown();
        return false;
    }
    if (minecraft->mLevel == nullptr)
        return false;
    boost::shared_ptr<Entity> camera = minecraft->getCameraEntity();
    if (camera == nullptr)
        return false;

    SetBiomeConstants();
    RefreshMappings();
    ShaderPipeline::Get().SetResourceResolver(&ResolveResourceTexture);

    Level* level = minecraft->mLevel;
    ShaderWorldState state;
    state.dimension = level->mDimension->getType()->getId();
    state.cameraX = (camera->mX - camera->mXOld) * partialTick + camera->mXOld;
    state.cameraY = (camera->mY - camera->mYOld) * partialTick + camera->mYOld;
    state.cameraZ = (camera->mZ - camera->mZOld) * partialTick + camera->mZOld;
    state.eyeAltitude = static_cast<float>(state.cameraY + camera->getEyeHeight());
    state.celestialAngle = level->getTimeOfDay(partialTick);
    state.worldTime = level->getDayTime();
    state.moonPhase = level->getMoonPhase();
    state.rainStrength = level->getRainLevel(partialTick);
    state.thunderStrength = level->getThunderLevel(partialTick);
    if (camera->isUnderLiquid(Material::WATER))
        state.isEyeInWater = 1;
    else if (camera->isUnderLiquid(Material::LAVA))
        state.isEyeInWater = 2;

    BlockPos eye(static_cast<int>(std::floor(state.cameraX)), static_cast<int>(std::floor(state.eyeAltitude)),
                 static_cast<int>(std::floor(state.cameraZ)));
    state.eyeBlockLight = level->getBrightness(LightLayer::BLOCK, eye);
    state.eyeSkyLight = level->getBrightness(LightLayer::SKY, eye);

    Vec3* sky = level->getSkyColor(camera, partialTick);
    state.skyColor[0] = static_cast<float>(sky->x);
    state.skyColor[1] = static_cast<float>(sky->y);
    state.skyColor[2] = static_cast<float>(sky->z);
    gameRenderer->getFogColor(state.fogColor);
    state.nearPlane = 0.05f;
    state.farPlane = static_cast<float>(VideoSettings::GetRenderDistance() * 16);
    state.hideGui = minecraft->mOptions->mHideGui;
    state.screenBrightness = minecraft->mOptions->getProgressValue(Options::Option::gamma);

    if (eTYPE_DERIVED_FROM(EntityType::LIVING_ENTITY, camera->GetType()))
    {
        LivingEntity* living = static_cast<LivingEntity*>(camera.get());
        if (living->hasEffect(MobEffects::NIGHT_VISION))
            state.nightVision = gameRenderer->getNightVisionScale(boost::static_pointer_cast<LivingEntity>(camera),
                                                                  partialTick);
        if (living->hasEffect(MobEffects::BLINDNESS))
            state.blindness = 1.0f;
        state.heldItemId = MappedItem(living->getMainHandItem(), state.heldBlockLight);
        state.heldItemId2 = MappedItem(living->getOffhandItem(), state.heldBlockLight2);
        state.entityFlags[ShaderEntity_Child] = living->isBaby();
        state.entityFlags[ShaderEntity_Hurt] = living->mHurtTime > 0;
    }
    state.entityFlags[ShaderEntity_Alive] = camera->isAlive();
    state.entityFlags[ShaderEntity_Burning] = camera->isOnFire();
    state.entityFlags[ShaderEntity_Glowing] = camera->isGlowing();
    state.entityFlags[ShaderEntity_InLava] = camera->isInLava();
    state.entityFlags[ShaderEntity_InWater] = camera->isInWater();
    state.entityFlags[ShaderEntity_Invisible] = camera->isInvisible();
    state.entityFlags[ShaderEntity_OnGround] = camera->mOnGround;
    state.entityFlags[ShaderEntity_Ridden] = camera->isVehicle();
    state.entityFlags[ShaderEntity_Riding] = camera->isPassenger();
    state.entityFlags[ShaderEntity_Sneaking] = camera->isSneaking();
    state.entityFlags[ShaderEntity_Sprinting] = camera->isSprinting();
    state.entityFlags[ShaderEntity_Wet] = camera->isInWater() || level->isRainingAt(eye);

    state.eyeX = state.cameraX;
    state.eyeY = state.eyeAltitude;
    state.eyeZ = state.cameraZ;
    Vec3* look = camera->getViewVector(partialTick);
    state.lookVector[0] = static_cast<float>(look->x);
    state.lookVector[1] = static_cast<float>(look->y);
    state.lookVector[2] = static_cast<float>(look->z);
    float bodyYaw = camera->mYRot;
    if (eTYPE_DERIVED_FROM(EntityType::LIVING_ENTITY, camera->GetType()))
    {
        LivingEntity* living = static_cast<LivingEntity*>(camera.get());
        bodyYaw = living->mYBodyRotO + (living->mYBodyRot - living->mYBodyRotO) * partialTick;
        state.elytraFlying = living->isFallFlying();
    }
    state.bodyVector[0] = -std::sin(bodyYaw * 0.017453292f);
    state.bodyVector[1] = 0.0f;
    state.bodyVector[2] = std::cos(bodyYaw * 0.017453292f);
    state.firstPerson = minecraft->mOptions->mThirdPersonView == 0;
    if (eTYPE_DERIVED_FROM(EntityType::PLAYER, camera->GetType()))
    {
        Player* player = static_cast<Player*>(camera.get());
        state.spectator = player->isSpectator();
        state.rightHanded = player->getMainArm() == HumanoidArm::RIGHT;
        const GameType* gameType = minecraft->mGameMode != nullptr ? minecraft->mGameMode->GetGameType() : nullptr;
        if (gameType != nullptr && gameType->isSurvival())
        {
            state.health = player->getHealth();
            state.maxHealth = player->getMaxHealth();
            state.hunger = static_cast<float>(player->getFoodData()->getFoodLevel());
            state.air = static_cast<float>(player->getAirSupply());
            state.maxAir = static_cast<float>(player->getMaxAirSupply());
            state.armor = static_cast<float>(player->getArmorValue());
        }
    }
    for (const boost::shared_ptr<Entity>& entity : level->mGlobalEntities)
    {
        if (entity == nullptr || !eTYPE_DERIVED_FROM(EntityType::LIGHTNING_BOLT, entity->GetType()))
            continue;
        state.lightning[0] = static_cast<float>(entity->mX - state.cameraX);
        state.lightning[1] = static_cast<float>(entity->mY - state.cameraY);
        state.lightning[2] = static_cast<float>(entity->mZ - state.cameraZ);
        state.lightning[3] = 1.0f;
        break;
    }
    state.cloudHeight = level->mDimension->getCloudHeight();
    state.heightLimit = level->getMaxBuildHeight();
    state.logicalHeightLimit = level->getHeight();

    BlockPos feet(static_cast<int>(std::floor(state.cameraX)), static_cast<int>(std::floor(state.cameraY)),
                  static_cast<int>(std::floor(state.cameraZ)));
    Biome* biome = level->getBiome(feet);
    if (biome != nullptr)
    {
        state.biome = Biome::getId(biome);
        state.biomeCategory = BiomeCategory(biome);
        state.temperature = biome->getTemperature(feet);
        state.rainfall = biome->getDownfall();
        if (biome->hasRain() || biome->hasSnow())
            state.biomePrecipitation = state.temperature < 0.15f ? 2 : 1;
    }

    if (minecraft->mTerrainAtlas != nullptr && minecraft->mTerrainAtlas->getStitchedTexture() != nullptr)
    {
        state.atlasWidth = minecraft->mTerrainAtlas->getStitchedTexture()->getWidth();
        state.atlasHeight = minecraft->mTerrainAtlas->getStitchedTexture()->getHeight();
    }

    sActive = gRenderer->ShaderBeginWorld(state);
    sPackFrame = sActive;
    return sActive;
}

bool BeginWorld(GameRenderer* gameRenderer, float partialTick)
{
    bool active = BeginPackWorld(gameRenderer, partialTick);
    SyncBlockLighting(active);
    return active;
}

void EndWorld()
{
    if (!sActive)
        return;
    gRenderer->ShaderEndWorld();
    sActive = false;
}

bool Active()
{
    return sActive;
}

bool InShadow()
{
    return sActive && ShaderPipeline::Get().InShadow();
}

bool SkipVanillaShadows()
{
    return sActive && (ShaderPipeline::Get().InShadow() || ShaderPipeline::Get().ShadowEnabled());
}

int CloudMode()
{
    return sPackFrame ? ShaderPipeline::Get().CloudMode() : -1;
}

bool RenderSkyDisc()
{
    return sPackFrame && ShaderPipeline::Get().RenderSkyDisc();
}

bool RenderSun()
{
    return !sPackFrame || ShaderPipeline::Get().RenderSun();
}

bool RenderMoon()
{
    return !sPackFrame || ShaderPipeline::Get().RenderMoon();
}

bool UnderwaterOverlay()
{
    return !sPackFrame || ShaderPipeline::Get().UnderwaterOverlay();
}

bool Vignette()
{
    return !sPackFrame || ShaderPipeline::Get().Vignette();
}

bool WeatherDepth()
{
    return sActive && ShaderPipeline::Get().RainDepth();
}

bool BeaconBeamDepth()
{
    return sActive && ShaderPipeline::Get().BeaconBeamDepth();
}

float HandDepth()
{
    return sActive ? ShaderPackManager::Get().Settings().handDepthMul : 1.0f;
}

void SetStage(int stage)
{
    if (sActive)
        gRenderer->ShaderSetStage(stage);
}

void BeginTranslucent()
{
    if (sActive)
        gRenderer->ShaderBeginTranslucent();
}

void BeginHand()
{
    if (sActive)
        gRenderer->ShaderBeginHand();
}

void EndHand()
{
    if (sActive)
        gRenderer->ShaderEndHand();
}

void RenderShadows(LevelRenderer* levelRenderer, float partialTick)
{
    if (!sActive || !gRenderer->ShaderBeginShadow())
        return;
    ShaderPipeline& pipeline = ShaderPipeline::Get();
    float distance = pipeline.ShadowRenderDistance();
    float view[16];
    float projection[16];
    pipeline.ShadowMatrices(view, projection);
    if (pipeline.ShadowTerrain())
        levelRenderer->renderShadowChunks(0, partialTick, distance, view, projection);
    bool player = pipeline.ShadowEntities() || pipeline.ShadowPlayer();
    if (pipeline.ShadowEntities() || player || pipeline.ShadowBlockEntities())
        levelRenderer->renderShadowEntities(partialTick, distance, view, projection, pipeline.ShadowEntities(), player,
                                            pipeline.ShadowBlockEntities());
    gRenderer->ShaderEndShadowOpaque();
    if (pipeline.ShadowTerrain() && pipeline.ShadowTranslucent())
    {
        levelRenderer->renderShadowChunks(2, partialTick, distance, view, projection);
        levelRenderer->renderShadowChunks(1, partialTick, distance, view, projection);
    }
    gRenderer->ShaderEndShadow();
}

void SetEntity(Entity* entity)
{
    if (!sActive)
        return;
    int id = -1;
    if (entity != nullptr)
    {
        Minecraft* minecraft = Minecraft::GetInstance();
        std::string name = Lower(Narrow(entity->getEncodeId()));
        if (minecraft->mPlayer != nullptr && entity == static_cast<Entity*>(minecraft->mPlayer.get())
            && sMappings.entities.count("minecraft:current_player"))
            name = "minecraft:current_player";
        else if (eTYPE_DERIVED_FROM(EntityType::PLAYER, entity->GetType()))
            name = "minecraft:player";
        auto found = sMappings.entities.find(name);
        if (found != sMappings.entities.end())
            id = found->second;
    }
    ShaderPipeline::Get().SetEntityId(id);
}

void SetBlockEntity(BlockEntity* blockEntity)
{
    if (!sActive)
        return;
    int id = -1;
    if (blockEntity != nullptr && blockEntity->mLevel != nullptr && !sMappings.blocks.empty())
    {
        const BlockState* state = blockEntity->mLevel->getBlockState(blockEntity->getBlockPos());
        int element = state != nullptr ? Block::getElementId(state) : 0;
        if (element > 0 && element < static_cast<int>(sMappings.blocks.size()))
            id = sMappings.blocks[static_cast<size_t>(element)];
    }
    ShaderPipeline::Get().SetBlockEntityId(id);
}

void ClearEntity()
{
    if (!sActive)
        return;
    ShaderPipeline::Get().SetEntityId(-1);
    ShaderPipeline::Get().SetBlockEntityId(-1);
}
}
