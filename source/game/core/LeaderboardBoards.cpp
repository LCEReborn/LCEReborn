#include "game/core/LeaderboardBoards.hpp"

#include "game/core/Stat.hpp"
#include "game/core/Stats.hpp"
#include "game/core/StatsCounter.hpp"
#include "game/item/Item.hpp"
#include "game/item/Items.hpp"
#include "game/world/block/Block.hpp"
#include "game/world/block/Blocks.hpp"

namespace
{
const int kStatsPerColumn = 2;

struct Column
{
    Stat* stats[kStatsPerColumn];
};

Column gColumns[LeaderboardBoards::eBoard_Count][LeaderboardBoards::MAX_COLUMNS];
int gColumnCounts[LeaderboardBoards::eBoard_Count];
bool gReady = false;

void Add(int board, Stat* first, Stat* second = NULL)
{
    Column& column = gColumns[board][gColumnCounts[board]++];
    column.stats[0] = first;
    column.stats[1] = second;
}

void Setup()
{
    if (gReady || Stats::walkOneM == NULL)
        return;
    gReady = true;
    Add(LeaderboardBoards::eBoard_Travelling, Stats::walkOneM);
    Add(LeaderboardBoards::eBoard_Travelling, Stats::fallOneM);
    Add(LeaderboardBoards::eBoard_Travelling, Stats::minecartOneM);
    Add(LeaderboardBoards::eBoard_Travelling, Stats::boatOneM);

    Add(LeaderboardBoards::eBoard_Mining, Stats::blockMinedStats[Block::getId(Blocks::dirt)]);
    Add(LeaderboardBoards::eBoard_Mining, Stats::blockMinedStats[Block::getId(Blocks::cobblestone)]);
    Add(LeaderboardBoards::eBoard_Mining, Stats::blockMinedStats[Block::getId(Blocks::sand)]);
    Add(LeaderboardBoards::eBoard_Mining, Stats::blockMinedStats[Block::getId(Blocks::stone)]);
    Add(LeaderboardBoards::eBoard_Mining, Stats::blockMinedStats[Block::getId(Blocks::gravel)]);
    Add(LeaderboardBoards::eBoard_Mining, Stats::blockMinedStats[Block::getId(Blocks::clay)]);
    Add(LeaderboardBoards::eBoard_Mining, Stats::blockMinedStats[Block::getId(Blocks::obsidian)]);

    Add(LeaderboardBoards::eBoard_Farming, Stats::itemCollectedStats[Item::getId(Items::egg)]);
    Add(LeaderboardBoards::eBoard_Farming, Stats::blockMinedStats[Block::getId(Blocks::wheat)]);
    Add(LeaderboardBoards::eBoard_Farming, Stats::blockMinedStats[Block::getId(Blocks::brown_mushroom)]);
    Add(LeaderboardBoards::eBoard_Farming, Stats::blockMinedStats[Block::getId(Blocks::reeds)]);
    Add(LeaderboardBoards::eBoard_Farming, Stats::cowsMilked);
    Add(LeaderboardBoards::eBoard_Farming, Stats::itemCollectedStats[Block::getId(Blocks::pumpkin)]);

    Add(LeaderboardBoards::eBoard_Kills, Stats::killsZombie);
    Add(LeaderboardBoards::eBoard_Kills, Stats::killsSkeleton);
    Add(LeaderboardBoards::eBoard_Kills, Stats::killsCreeper);
    Add(LeaderboardBoards::eBoard_Kills, Stats::killsSpider);
    Add(LeaderboardBoards::eBoard_Kills, Stats::killsSpiderJockey);
    Add(LeaderboardBoards::eBoard_Kills, Stats::killsZombiePigman, Stats::killsNetherZombiePigman);
    Add(LeaderboardBoards::eBoard_Kills, Stats::killsSlime);
}
}

int LeaderboardBoards::ColumnCount(int board)
{
    Setup();
    return board >= 0 && board < eBoard_Count ? gColumnCounts[board] : 0;
}

bool LeaderboardBoards::FindColumn(Stat* stat, int* board, int* column)
{
    Setup();
    if (stat == NULL)
        return false;
    for (int b = 0; b < eBoard_Count; ++b)
    {
        for (int c = 0; c < gColumnCounts[b]; ++c)
        {
            for (int s = 0; s < kStatsPerColumn; ++s)
            {
                if (gColumns[b][c].stats[s] == stat)
                {
                    *board = b;
                    *column = c;
                    return true;
                }
            }
        }
    }
    return false;
}

unsigned int LeaderboardBoards::CounterValue(StatsCounter* counter, int board, int column)
{
    if (counter == NULL || column < 0 || column >= ColumnCount(board))
        return 0;
    unsigned long long total = 0;
    for (int s = 0; s < kStatsPerColumn; ++s)
    {
        Stat* stat = gColumns[board][column].stats[s];
        if (stat != NULL)
            total += static_cast<unsigned int>(counter->getTotalValue(stat));
    }
    return total > 0xffffffffULL ? 0xffffffffU : static_cast<unsigned int>(total);
}
