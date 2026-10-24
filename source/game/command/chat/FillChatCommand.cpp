#include "game/command/chat/FillChatCommand.hpp"

#include <algorithm>

#include "game/command/CommandArguments.hpp"
#include "game/world/Level.hpp"
#include "game/world/block/Block.hpp"
#include "game/world/block/BlockPos.hpp"
#include "game/world/block/BlockState.hpp"
#include "game/world/block/Blocks.hpp"

using CommandArguments::Lower;
using CommandArguments::Number;
using CommandArguments::ParseInt;

namespace
{
const int kFillLimit = 32768;

Block* StillLiquid(Block* block)
{
    if (block == Blocks::flowing_water)
        return Blocks::water;
    if (block == Blocks::flowing_lava)
        return Blocks::lava;
    return block;
}
}

FillChatCommand::FillChatCommand()
    : ChatCommand(L"fill", L"<x1 y1 z1> <x2 y2 z2> <block> [data] [mode] [replaceBlock] [replaceData]",
                  L"coord coord coord coord coord coord block number replace|destroy|keep|hollow|outline block number",
                  ePermission_Operator)
{
}

bool FillChatCommand::run(const CommandSource& source, const Words& words)
{
    if (words.size() < 8 || words.size() > 12)
        return false;
    BlockPos first;
    BlockPos second;
    if (!ParseBlockPos(source, words, 1, &first) || !ParseBlockPos(source, words, 4, &second))
        return true;
    Block* block = FindBlock(source, words[7]);
    int data = 0;
    if (block == NULL || !ParseBlockData(source, words, 8, &data))
        return true;
    const std::wstring mode = words.size() > 9 ? Lower(words[9]) : std::wstring(L"replace");
    if (mode != L"replace" && mode != L"destroy" && mode != L"keep" && mode != L"hollow" && mode != L"outline")
        return false;
    Block* filter = NULL;
    int filterData = -1;
    if (words.size() > 10)
    {
        if (mode != L"replace")
            return false;
        filter = FindBlock(source, words[10]);
        if (filter == NULL)
            return true;
        filter = StillLiquid(filter);
        if (words.size() > 11 && !ParseInt(words[11], -1, 15, &filterData))
        {
            Fail(source, L"The data value must be a whole number from -1 to 15.");
            return true;
        }
    }

    const int x0 = std::min(first.x, second.x);
    const int y0 = std::min(first.y, second.y);
    const int z0 = std::min(first.z, second.z);
    const int x1 = std::max(first.x, second.x);
    const int y1 = std::max(first.y, second.y);
    const int z1 = std::max(first.z, second.z);
    const long long volume = (long long)(x1 - x0 + 1) * (y1 - y0 + 1) * (z1 - z0 + 1);
    if (volume > kFillLimit)
    {
        Fail(source,
             L"Too many blocks in the specified area (" + LongNumber(volume) + L" > " + Number(kFillLimit) + L")");
        return true;
    }
    Level* level = source.level;
    if (!level->isInWorldBounds(x0, y0, z0) || !level->isInWorldBounds(x1, y1, z1))
    {
        Fail(source, L"Cannot place blocks outside of the world");
        return true;
    }
    for (int chunkX = x0 & ~15; chunkX <= x1; chunkX += 16)
    {
        for (int chunkZ = z0 & ~15; chunkZ <= z1; chunkZ += 16)
        {
            if (!level->hasChunkAt(BlockPos(chunkX, y0, chunkZ)))
            {
                Fail(source, L"Cannot place blocks outside of the world");
                return true;
            }
        }
    }

    const BlockState* state = block->getBlockState(data);
    const BlockState* air = Blocks::air->defaultBlockState();
    int changed = 0;
    for (int y = y0; y <= y1; ++y)
    {
        for (int z = z0; z <= z1; ++z)
        {
            for (int x = x0; x <= x1; ++x)
            {
                const BlockPos pos(x, y, z);
                const bool edge = x == x0 || x == x1 || y == y0 || y == y1 || z == z0 || z == z1;
                const BlockState* placed = state;
                if (mode == L"hollow" && !edge)
                    placed = air;
                else if (mode == L"outline" && !edge)
                    continue;
                else if (mode == L"keep" && !level->isEmptyBlock(pos))
                    continue;
                else if (filter != NULL)
                {
                    const BlockState* current = level->getBlockState(pos);
                    Block* currentBlock = current->getBlock();
                    if (StillLiquid(currentBlock) != filter
                        || (filterData >= 0 && currentBlock->convertBlockStateToLegacyData(current) != filterData))
                        continue;
                }
                if (mode == L"destroy")
                    level->destroyBlock(pos, true);
                if (level->setBlock(pos, placed, Block::UPDATE_ALL, false))
                    ++changed;
            }
        }
    }
    if (changed == 0)
    {
        Fail(source, L"No blocks filled");
        return true;
    }
    Tell(source, Count(changed, L"block", L"blocks") + L" filled");
    return true;
}
