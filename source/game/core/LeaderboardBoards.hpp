#ifndef GAME_CORE_LEADERBOARDBOARDS_HPP
#define GAME_CORE_LEADERBOARDBOARDS_HPP

class Stat;
class StatsCounter;

namespace LeaderboardBoards
{
enum EBoard
{
    eBoard_Travelling,
    eBoard_Mining,
    eBoard_Farming,
    eBoard_Kills,
    eBoard_Count
};

const int MAX_COLUMNS = 7;

int ColumnCount(int board);
bool FindColumn(Stat* stat, int* board, int* column);
unsigned int CounterValue(StatsCounter* counter, int board, int column);
}

#endif
