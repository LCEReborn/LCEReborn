#include "game/command/chat/SetBlockChatCommand.hpp"

#include "game/command/CommandArguments.hpp"
#include "game/world/Level.hpp"
#include "game/world/block/Block.hpp"
#include "game/world/block/BlockPos.hpp"
#include "game/world/block/BlockState.hpp"

using CommandArguments::Lower;

SetBlockChatCommand::SetBlockChatCommand()
    : ChatCommand(L"setblock", L"<x y z> <block> [data] [replace|destroy|keep]",
                  L"coord coord coord block number replace|destroy|keep", ePermission_Operator)
{
}

bool SetBlockChatCommand::run(const CommandSource& source, const Words& words)
{
    if (words.size() < 5 || words.size() > 7)
        return false;
    BlockPos pos;
    if (!ParseBlockPos(source, words, 1, &pos))
        return true;
    Block* block = FindBlock(source, words[4]);
    int data = 0;
    if (block == NULL || !ParseBlockData(source, words, 5, &data))
        return true;
    const std::wstring mode = words.size() > 6 ? Lower(words[6]) : std::wstring(L"replace");
    if (mode != L"replace" && mode != L"destroy" && mode != L"keep")
        return false;
    Level* level = source.level;
    if (!level->isInWorldBounds(pos) || !level->hasChunkAt(pos))
    {
        Fail(source, L"Cannot place block outside of the world");
        return true;
    }
    if (mode == L"keep" && !level->isEmptyBlock(pos))
    {
        Fail(source, L"Could not set the block");
        return true;
    }
    if (mode == L"destroy")
        level->destroyBlock(pos, true);
    if (!level->setBlock(pos, block->getBlockState(data), Block::UPDATE_ALL, false))
    {
        Fail(source, L"Could not set the block");
        return true;
    }
    Tell(source, L"Block placed");
    return true;
}
