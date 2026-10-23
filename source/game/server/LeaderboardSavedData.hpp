#ifndef GAME_SERVER_LEADERBOARDSAVEDDATA_HPP
#define GAME_SERVER_LEADERBOARDSAVEDDATA_HPP

#include <string>
#include <vector>

#include <boost/shared_ptr.hpp>
#include <boost/unordered_map.hpp>

#include "game/core/LeaderboardBoards.hpp"
#include "game/data/SavedData.hpp"

class CompoundTag;
class ServerPlayer;
class Stat;

class LeaderboardSavedData : public SavedData
{
public:
    struct Row
    {
        std::wstring id;
        std::wstring name;
        unsigned int values[LeaderboardBoards::eBoard_Count][LeaderboardBoards::MAX_COLUMNS];
    };

    struct Ranked
    {
        unsigned int rank;
        const Row* row;
    };

    static const std::wstring FILE_ID;

    LeaderboardSavedData(const std::wstring& id);

    virtual void load(CompoundTag* tag);
    virtual CompoundTag* save(CompoundTag* tag);

    void touch(const std::wstring& id, const std::wstring& name);
    void record(const std::wstring& id, const std::wstring& name, Stat* stat, unsigned int amount);
    unsigned int query(int board, bool aroundId, const std::wstring& id, unsigned int startRank, unsigned int count,
                       std::vector<Ranked>& out);

    static boost::shared_ptr<LeaderboardSavedData> get();
    static void onJoin(ServerPlayer* player);
    static void onAward(ServerPlayer* player, Stat* stat, unsigned int amount);

private:
    Row& rowFor(const std::wstring& id, const std::wstring& name);
    static unsigned long long total(const Row& row, int board);

    std::vector<Row> mRows;
    boost::unordered_map<std::wstring, size_t> mIndex;
};

#endif
