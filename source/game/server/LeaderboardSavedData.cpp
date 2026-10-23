#include "game/server/LeaderboardSavedData.hpp"

#include <algorithm>
#include <cstring>
#include <typeinfo>

#include "game/core/StringUtils.hpp"
#include "game/data/CompoundTag.hpp"
#include "game/data/ListTag.hpp"
#include "game/data/Tag.hpp"
#include "game/server/MinecraftServer.hpp"
#include "game/server/ServerLevel.hpp"
#include "game/server/ServerPlayer.hpp"

const std::wstring LeaderboardSavedData::FILE_ID = L"leaderboard";

namespace
{
const wchar_t* const kBoardTags[LeaderboardBoards::eBoard_Count] = {L"Travelling", L"Mining", L"Farming", L"Kills"};

struct RankOrder
{
    const std::vector<unsigned long long>* totals;

    bool operator()(const LeaderboardSavedData::Ranked& first, const LeaderboardSavedData::Ranked& second) const
    {
        const unsigned long long a = (*totals)[first.rank];
        const unsigned long long b = (*totals)[second.rank];
        if (a != b)
            return a > b;
        return toLower(first.row->name) < toLower(second.row->name);
    }
};
}

LeaderboardSavedData::LeaderboardSavedData(const std::wstring& id) : SavedData(id)
{
}

void LeaderboardSavedData::load(CompoundTag* tag)
{
    mRows.clear();
    mIndex.clear();
    ListTag* players = tag->getList(L"Players", Tag::TAG_COMPOUND);
    for (int i = 0; i < players->size(); ++i)
    {
        CompoundTag* entry = players->getCompound(i);
        const std::wstring id = entry->getString(L"UUID");
        if (id.empty() || mIndex.find(id) != mIndex.end())
            continue;
        Row& row = rowFor(id, entry->getString(L"Name"));
        for (int board = 0; board < LeaderboardBoards::eBoard_Count; ++board)
        {
            arrayWithLength<int> values = entry->getIntArray(kBoardTags[board]);
            const int count = std::min(static_cast<int>(values.length), LeaderboardBoards::MAX_COLUMNS);
            for (int column = 0; column < count; ++column)
                row.values[board][column] = static_cast<unsigned int>(values[column]);
        }
    }
}

CompoundTag* LeaderboardSavedData::save(CompoundTag* tag)
{
    ListTag* players = new ListTag();
    for (std::vector<Row>::const_iterator it = mRows.begin(); it != mRows.end(); ++it)
    {
        CompoundTag* entry = new CompoundTag();
        entry->putString(L"UUID", it->id);
        entry->putString(L"Name", it->name);
        for (int board = 0; board < LeaderboardBoards::eBoard_Count; ++board)
        {
            const int count = LeaderboardBoards::ColumnCount(board);
            arrayWithLength<int> values(count, true);
            for (int column = 0; column < count; ++column)
                values[column] = static_cast<int>(it->values[board][column]);
            entry->putIntArray(kBoardTags[board], values);
        }
        players->add(entry);
    }
    tag->put(L"Players", players);
    return tag;
}

LeaderboardSavedData::Row& LeaderboardSavedData::rowFor(const std::wstring& id, const std::wstring& name)
{
    boost::unordered_map<std::wstring, size_t>::iterator found = mIndex.find(id);
    if (found != mIndex.end())
    {
        Row& row = mRows[found->second];
        if (!name.empty() && row.name != name)
        {
            row.name = name;
            setDirty();
        }
        return row;
    }
    Row row;
    row.id = id;
    row.name = name;
    std::memset(row.values, 0, sizeof(row.values));
    mIndex[id] = mRows.size();
    mRows.push_back(row);
    setDirty();
    return mRows.back();
}

void LeaderboardSavedData::touch(const std::wstring& id, const std::wstring& name)
{
    rowFor(id, name);
}

void LeaderboardSavedData::record(const std::wstring& id, const std::wstring& name, Stat* stat, unsigned int amount)
{
    int board;
    int column;
    if (amount == 0 || !LeaderboardBoards::FindColumn(stat, &board, &column))
        return;
    unsigned int& value = rowFor(id, name).values[board][column];
    value = amount > 0xffffffffU - value ? 0xffffffffU : value + amount;
    setDirty();
}

unsigned long long LeaderboardSavedData::total(const Row& row, int board)
{
    unsigned long long sum = 0;
    for (int column = 0; column < LeaderboardBoards::ColumnCount(board); ++column)
        sum += row.values[board][column];
    return sum;
}

unsigned int LeaderboardSavedData::query(int board, bool aroundId, const std::wstring& id, unsigned int startRank,
                                         unsigned int count, std::vector<Ranked>& out)
{
    out.clear();
    if (board < 0 || board >= LeaderboardBoards::eBoard_Count || mRows.empty() || count == 0)
        return 0;

    std::vector<unsigned long long> totals(mRows.size());
    std::vector<Ranked> order(mRows.size());
    for (size_t i = 0; i < mRows.size(); ++i)
    {
        totals[i] = total(mRows[i], board);
        order[i].rank = static_cast<unsigned int>(i);
        order[i].row = &mRows[i];
    }
    RankOrder compare = {&totals};
    std::sort(order.begin(), order.end(), compare);
    for (size_t i = 0; i < order.size(); ++i)
        order[i].rank = static_cast<unsigned int>(i + 1);

    const unsigned int rows = static_cast<unsigned int>(order.size());
    unsigned int first = startRank < 1 ? 1 : startRank;
    if (aroundId)
    {
        first = 0;
        for (size_t i = 0; i < order.size(); ++i)
        {
            if (order[i].row->id == id)
            {
                first = order[i].rank;
                break;
            }
        }
        if (first == 0)
            return rows;
        first = first > count / 2 ? first - count / 2 : 1;
        if (rows >= count && first + count - 1 > rows)
            first = rows - count + 1;
    }
    for (unsigned int rank = first; rank <= rows && out.size() < count; ++rank)
        out.push_back(order[rank - 1]);
    return rows;
}

boost::shared_ptr<LeaderboardSavedData> LeaderboardSavedData::get()
{
    ServerLevel* level = gMinecraftServer != NULL ? gMinecraftServer->getLevel(0) : NULL;
    if (level == NULL || level->getSavedDataStorage() == NULL)
        return boost::shared_ptr<LeaderboardSavedData>();
    boost::shared_ptr<LeaderboardSavedData> data = boost::dynamic_pointer_cast<LeaderboardSavedData>(
        level->getSavedData(typeid(LeaderboardSavedData), FILE_ID));
    if (!data)
    {
        data = boost::shared_ptr<LeaderboardSavedData>(new LeaderboardSavedData(FILE_ID));
        level->setSavedData(FILE_ID, data);
    }
    return data;
}

void LeaderboardSavedData::onJoin(ServerPlayer* player)
{
    boost::shared_ptr<LeaderboardSavedData> data = get();
    if (data)
        data->touch(player->getXuid().toString(), player->getName());
}

void LeaderboardSavedData::onAward(ServerPlayer* player, Stat* stat, unsigned int amount)
{
    boost::shared_ptr<LeaderboardSavedData> data = get();
    if (data)
        data->record(player->getXuid().toString(), player->getName(), stat, amount);
}
