#include "game/util/GameModeUtilities.hpp"
#include "game/world/block/PressurePlateBlock.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "game/core/Mth.hpp"
#include "game/util/Math.hpp"
#include "game/core/MasterGameMode.hpp"
#include "game/core/Scoreboard.hpp"
#include "game/core/WeighedTreasure.hpp"
#include "game/entity/mob/Entity.hpp"
#include "game/item/AddItemRuleDefinition.hpp"
#include "game/item/Container.hpp"
#include "game/item/ItemInstance.hpp"
#include "game/server/ServerPlayer.hpp"
#include "game/server/MinecraftServer.hpp"
#include "game/server/ServerLevel.hpp"
#include "game/network/ServerGamePacketListenerImpl.hpp"
#include "game/minigame/GameRuleDefinition.hpp"
#include "game/minigame/EMiniGameId.hpp"
#include "game/minigame/MiniGameDef.hpp"
#include "game/core/Minecraft.hpp"
#include "game/util/AABB.hpp"
#include "game/util/Random.hpp"
#include "game/util/Vec3.hpp"
#include "game/world/Level.hpp"
#include "game/world/block/BlockPos.hpp"
#include "game/world/block/BlockState.hpp"
#include "game/world/block/Blocks.hpp"
#include "game/world/block/Material.hpp"
#include "game/world/generation/PlayerTeam.hpp"
#include "game/world/generation/TumbleGenerator.hpp"
#include "game/world/chunk/ChunkSource.hpp"

namespace
{
struct ChestItemsInfoHigherScore
{
    bool operator()(const MiniGameDef::ChestItemsInfo& left, const MiniGameDef::ChestItemsInfo& right) const
    {
        return (unsigned int)left.mKpScore > (unsigned int)right.mKpScore;
    }
};
}

const std::wstring SPECTATOR_TEAM_NAME = L"s";

GameModeUtilities::Position::Position() : x(0.0), z(0.0)
{
}

GameModeUtilities::Position::Position(double xValue, double zValue) : x(xValue), z(zValue), y(Math::random() * 360.0)
{
}

void GameModeUtilities::Position::randomize(Random* random, double minX, double minZ, double maxX, double maxZ)
{
    x = Mth::nextDouble(random, minX, maxX);
    z = Mth::nextDouble(random, minZ, maxZ);
}

bool GameModeUtilities::Position::isSafeInRange(Level* level, int minY, int maxY)
{
    int blockX = Mth::floor(x);
    int blockZ = Mth::floor(z);
    int top = std::min(Level::maxBuildHeight, maxY);
    int bottom = std::max(minY, 2);

    for (int y = top; y > bottom; --y)
    {
        BlockPos floorPos = BlockPos(blockX, y, blockZ).below();
        const BlockState* floor = level->getBlockState(floorPos);
        const BlockState* head = level->getBlockState(floorPos.above());
        if (floor != NULL && floor->getBlock() != Blocks::air)
        {
            return floor->isSolidBlockingCube() && head->getMaterial() != Material::FIRE
                   && dynamic_cast<PressurePlateBlock*>(head->getBlock()) == NULL;
        }
    }
    return false;
}

void GameModeUtilities::Position::randomize(Random* random, std::vector<long long>* positions)
{
    z = x = 0.0;
    if (positions != NULL && !positions->empty())
    {
        int index = Mth::nextInt(random, 0, positions->size() - 1);
        long long packed = positions->at(index);
        int packedX = (int)(packed >> 32);
        int packedZ = (int)(packed & 0xFFFFFFFF);
        x = packedX;
        z = packedZ;
    }
}

void GameModeUtilities::Position::set(double xValue, double zValue)
{
    x = xValue;
    z = zValue;
}

double GameModeUtilities::Position::dist(Position* other)
{
    double dx = x - other->x;
    double dz = z - other->z;
    return std::sqrt(dx * dx + dz * dz);
}

int GameModeUtilities::Position::getSpawnY(Level* level)
{
    int ix = Mth::floor(x);
    int iz = Mth::floor(z);
    for (int y = 256; y > 0; --y)
    {
        Block* block = level->getBlockState(BlockPos(ix, y, iz))->getBlock();
        if (block != NULL && block != Blocks::air)
        {
            if (block == Blocks::lava || block == Blocks::flowing_lava)
                return y + 50;
            return y + 1;
        }
    }
    return 257;
}

bool GameModeUtilities::DistributeItemsSpread(const std::vector<boost::shared_ptr<Container> >* containers,
                                              const std::vector<not_null_ptr<ItemInstance> >* items)
{
    std::vector<not_null_ptr<ItemInstance> > remaining(*items);
    delete items;
    bool result = false;
    if (!remaining.empty() && !containers->empty())
    {
        Random random;
        unsigned int containerCount = (unsigned int)containers->size();
        unsigned int itemCount = (unsigned int)remaining.size();
        unsigned int baseCount = itemCount / containerCount;
        unsigned int remainder = itemCount - baseCount * containerCount;
        std::vector<unsigned int> counts(containerCount, baseCount);
        for (unsigned int i = 0; i < remainder; ++i)
        {
            unsigned int index;
            do
            {
                index = (unsigned int)random.nextInt((int)containerCount);
            } while (counts[index] != baseCount);
            ++counts[index];
        }
        for (unsigned int ci = 0; ci < containerCount; ++ci)
        {
            boost::shared_ptr<Container> container = containers->at(ci);
            std::vector<not_null_ptr<ItemInstance> > batch;
            for (unsigned int j = 0; j < counts[ci]; ++j)
            {
                unsigned int index = (unsigned int)random.nextInt((int)remaining.size());
                batch.push_back(remaining.at(index));
                remaining.erase(remaining.begin() + index);
            }
            while (!batch.empty())
            {
                unsigned int index = (unsigned int)random.nextInt((int)batch.size());
                AddItemRuleDefinition::addItemToContainer(container, batch.at(index), -1, true);
                batch.erase(batch.begin() + index);
            }
        }
        result = true;
    }
    return result;
}

const MiniGameDef::ChestKitInfo* GameModeUtilities::SelectRandomKit(const std::vector<MiniGameDef::ChestKitInfo>* kits,
                                                                     unsigned int* weights, Random* random)
{
    unsigned int kitCount = kits->size();
    unsigned int totalWeight = 0;
    for (unsigned int i = 0; i < kitCount; ++i)
        totalWeight += weights[i];
    int value = random->nextInt(totalWeight);
    for (unsigned int i = 0; i < kitCount; ++i)
    {
        value -= weights[i];
        if (value < 0)
        {
            weights[i] /= 2;
            return &kits->at(i);
        }
    }
    return NULL;
}

void GameModeUtilities::GenerateContainerContentsGreedy(std::vector<not_null_ptr<ItemInstance> >* output,
                                                        const std::vector<MiniGameDef::ChestItemsInfo>* items,
                                                        Random* random, unsigned int targetScore, unsigned int score)
{
    std::vector<MiniGameDef::ChestItemsInfo> sorted(items->begin(), items->end());
    MiniGameDef::ChestItemsInfo item;
    std::sort(sorted.begin(), sorted.end(), ChestItemsInfoHigherScore());
    int runLength = 0;
    bool countRun = true;
    std::vector<MiniGameDef::ChestItemsInfo>::iterator it = sorted.begin();
    while (it != sorted.end())
    {
        if (it->mKpScore + score > targetScore)
        {
            countRun = true;
            ++it;
            continue;
        }
        if (countRun)
        {
            countRun = false;
            std::vector<MiniGameDef::ChestItemsInfo>::iterator runEnd = it;
            while (runEnd != sorted.end() && runEnd->mKpScore == it->mKpScore)
                ++runEnd;
            runLength = runEnd - it;
        }
        item = it[random->nextInt(runLength)];
        output->push_back(item.mItem->copy());
        score += item.mKpScore;
    }
}

void GameModeUtilities::GenerateContainerContentsRandom(std::vector<not_null_ptr<ItemInstance> >* output,
                                                        const std::vector<MiniGameDef::ChestItemsInfo>* items,
                                                        Random* random, unsigned int targetScore, unsigned int score)
{
    do
    {
        const MiniGameDef::ChestItemsInfo& item = items->at(random->nextInt(items->size()));
        output->push_back(item.mItem->copy());
        score += item.mKpScore;
    } while (score < targetScore);
}

bool GameModeUtilities::DistributeItemsKitsAndKP(const std::vector<boost::shared_ptr<Container> >* containers,
                                                 const std::vector<MiniGameDef::ChestItemsInfo>* items,
                                                 const std::vector<MiniGameDef::ChestKitInfo>* kits,
                                                 unsigned int chestKpScore, bool greedy)
{
    Random random;
    std::vector<not_null_ptr<ItemInstance> > contents;
    unsigned int* weights = new unsigned int[kits->size()];
    for (unsigned int i = 0; i < kits->size(); ++i)
        weights[i] = kits->at(i).mWeight;

    unsigned int kpScore = 0;
    for (std::vector<boost::shared_ptr<Container> >::const_iterator it = containers->begin(); it != containers->end();
         ++it)
    {
        boost::shared_ptr<Container> container = *it;
        unsigned int itemCount = 0;
        unsigned int containerSize = container->getContainerSize();
        if (!kits->empty())
        {
            const MiniGameDef::ChestKitInfo* kit = SelectRandomKit(kits, weights, &random);
            if (kit != NULL)
            {
                kpScore = kit->mKpScore;
                for (std::vector<not_null_ptr<ItemInstance> >::const_iterator item = kit->mItems.begin();
                     item != kit->mItems.end() && itemCount < containerSize; ++item, ++itemCount)
                    AddItemRuleDefinition::addItemToContainer(container, (*item)->copy(), -1, true);
            }
        }
        if (kpScore < chestKpScore && !items->empty())
        {
            if (greedy)
                GenerateContainerContentsGreedy(&contents, items, &random, chestKpScore, kpScore);
            else
                GenerateContainerContentsRandom(&contents, items, &random, chestKpScore, kpScore);
        }
        for (std::vector<not_null_ptr<ItemInstance> >::iterator item = contents.begin();
             item != contents.end() && itemCount < containerSize; ++item, ++itemCount)
            AddItemRuleDefinition::addItemToContainer(container, *item, -1, true);
        contents.clear();
    }
    delete[] weights;
    return true;
}

bool GameModeUtilities::DistributeItemsKitsFixed(const std::vector<boost::shared_ptr<Container> >* containers,
                                                 const std::vector<MiniGameDef::ChestKitInfo>* kits)
{
    if (kits->empty())
        return true;
    const MiniGameDef::ChestKitInfo& kit = kits->at(0);
    for (std::vector<boost::shared_ptr<Container> >::const_iterator it = containers->begin(); it != containers->end();
         ++it)
    {
        for (std::vector<not_null_ptr<ItemInstance> >::const_iterator item = kit.mItems.begin();
             item != kit.mItems.end(); ++item)
            AddItemRuleDefinition::addItemToContainer(*it, (*item)->copy(), -1, false);
    }
    return true;
}

bool GameModeUtilities::DistributeItemsWeightedRandom(const std::vector<boost::shared_ptr<Container> >* containers,
                                                      arrayWithLength<WeighedTreasure*> treasures, unsigned int score)
{
    Random random;
    for (unsigned int i = 0; i < containers->size(); ++i)
        WeighedTreasure::addChestItems(&random, treasures, containers->at(i), (int)score, true);
    for (unsigned int i = 0; i < treasures.length; ++i)
        if (treasures.data[i] != NULL)
            delete treasures.data[i];
    delete[] treasures.data;
    return true;
}

void GameModeUtilities::SpawnWithinArea(boost::shared_ptr<ServerPlayer> player, AABB* area)
{
    Random random;
    double x = area->mMinX + (area->mMaxX - area->mMinX) * random.nextDouble();
    double y = area->mMinY + (area->mMaxY - area->mMinY) * random.nextDouble();
    double z = area->mMinZ + (area->mMaxZ - area->mMinZ) * random.nextDouble();
    player->absMoveTo(x, y, z, 0.0f, 0.0f);
    player->teleportTo(player->mX, player->mY, player->mZ);
}

void GameModeUtilities::DistributePlayers(std::vector<boost::shared_ptr<ServerPlayer> >* players,
                                          std::vector<GameRuleDefinition*>* rules, arrayWithLength<unsigned char>* data)
{
    ServerLevel* level = gMinecraftServer->getLevel(0);
    ChunkSource* chunks = level->getChunkSource();
    std::vector<int> availableRules;
    for (unsigned int index = 0; index < rules->size(); ++index)
        availableRules.push_back(static_cast<int>(index));

    Random random;
    for (std::vector<boost::shared_ptr<ServerPlayer> >::iterator it = players->begin(); it != players->end(); ++it)
    {
        int ruleIndex = -1;
        while (!availableRules.empty())
        {
            const int candidate = random.nextInt(static_cast<int>(availableRules.size()));
            const int index = availableRules.at(static_cast<unsigned int>(candidate));
            if (data == NULL || index < 0 || index >= static_cast<int>(data->length)
                || data->data[index] == static_cast<unsigned char>(-1))
            {
                ruleIndex = index;
                break;
            }
            availableRules.erase(availableRules.begin() + candidate);
        }
        if (ruleIndex < 0)
            ruleIndex = random.nextInt(static_cast<int>(rules->size()));

        if (data != NULL && ruleIndex >= 0 && ruleIndex < static_cast<int>(data->length))
            data->data[ruleIndex] = (*it)->mPlayerIndex;

        boost::shared_ptr<ServerPlayer> player = *it;
        rules->at(static_cast<unsigned int>(ruleIndex))->postProcessPlayer(player);

        const int chunkX = static_cast<int>(player->mX) >> 4;
        const int chunkZ = static_cast<int>(player->mZ) >> 4;
        if (!chunks->hasChunk(chunkX, chunkZ))
            chunks->create(chunkX, chunkZ);

        player->doTick(true, true, true);
        player->mConnection->teleport(player->mX, player->mY, player->mZ, player->mYRot, player->mXRot, true, true);
        if (Minecraft::InMiniGame(eMiniGameId_Battle, false))
            player->ResetSpawnInvulnerableTime();
    }
}

std::vector<GameModeUtilities::Position*>* GameModeUtilities::createInitialPositions(Random* random, int count,
                                                                                     double minX, double minZ,
                                                                                     double maxX, double maxZ)
{
    std::vector<Position*>* positions = new std::vector<Position*>();
    positions->reserve(count);
    for (int i = 0; i < count; ++i)
    {
        Position* position = new Position();
        position->randomize(random, minX, minZ, maxX, maxZ);
        positions->push_back(position);
    }
    return positions;
}

int GameModeUtilities::getNumberOfTeams(std::vector<boost::shared_ptr<ServerPlayer> >* players)
{
    return 0;
}

int GameModeUtilities::spreadPositions(double distance, Level* level, Random* random, const AABB* area,
                                       std::vector<Position*>* positions, bool teams)
{
    std::vector<long long> candidates;
    for (int gx = (int)std::floor(area->mMinX); gx < (int)std::ceil(area->mMaxX); ++gx)
    {
        for (int gz = (int)std::floor(area->mMinZ); gz < (int)std::ceil(area->mMaxZ); ++gz)
        {
            long long packed = ((long long)gx << 32) | (unsigned int)gz;
            int minY = (int)std::floor(area->mMinY);
            int maxY = (int)std::ceil(area->mMaxY);
            Position cell((double)gx, (double)gz);
            if (cell.isSafeInRange(level, minY, maxY))
                candidates.push_back(packed);
        }
    }

    for (std::vector<Position*>::iterator it = positions->begin(); it != positions->end(); ++it)
        (*it)->randomize(random, &candidates);

    int placed = 0;
    for (std::vector<Position*>::iterator it = positions->begin(); it != positions->end(); ++it)
    {
        double savedX = (*it)->x;
        double savedZ = (*it)->z;
        (*it)->randomize(random, &candidates);
        int roundedX = (int)(long long)(*it)->x;
        int roundedZ = (int)(long long)(*it)->z;
        for (std::vector<long long>::iterator candidate = candidates.begin(); candidate != candidates.end();)
        {
            int candidateX = (int)(*candidate >> 32);
            int candidateZ = (int)*candidate;
            double length =
                Vec3::newTemp((double)roundedX - (double)candidateX, 0.0, (double)roundedZ - (double)candidateZ)->length();
            if (length < distance)
                candidate = candidates.erase(candidate);
            else
                ++candidate;
        }
        if (candidates.empty())
        {
            (*it)->set(savedX, savedZ);
            std::random_shuffle(candidates.begin(), candidates.end());
            break;
        }
        ++placed;
    }

    double centerX = (area->mMinX + area->mMaxX) * 0.5;
    double centerZ = (area->mMinZ + area->mMaxZ) * 0.5;
    for (std::vector<Position*>::iterator it = positions->begin(); it != positions->end(); ++it)
        (*it)->y = Mth::atan2((int)((*it)->z - centerZ), (int)((*it)->x - centerX)) * 57.295776f + 90.0f;

    return placed;
}

double GameModeUtilities::setPlayerPositions(std::vector<boost::shared_ptr<ServerPlayer> >* players, Level* level,
                                             std::vector<Position*>* positions, bool, TumbleGenerator* generator)
{
    double totalDistance = 0.0;
    unsigned int playerIndex = 0;
    unsigned int positionIndex = 0;
    while (playerIndex < players->size())
    {
        boost::shared_ptr<ServerPlayer> player = players->at(playerIndex);
        Position* position = positions->at(positionIndex++);
        double spawnY;
        bool foundSpawnY = false;
        if (generator == NULL || level->mIsClientSide)
        {
            spawnY = (double)position->getSpawnY(level);
        }
        else
        {
            int layer = gMinecraftServer->mMasterGameMode->GetSpawnLayerIndex();
            AABB* spawnArea = generator->GetSpawnArea((unsigned int)layer);
            spawnY = spawnArea->mMinY;
            for (double y = spawnY; y <= spawnArea->mMaxY; y += 1.0)
            {
                const BlockState* state = level->getBlockState(BlockPos(position->x, y, position->z));
                if (state->getMaterial()->blocksMotion())
                {
                    foundSpawnY = true;
                    spawnY = y + 1.0;
                }
            }
            if (!foundSpawnY)
            {
                position->randomize(level->mRandom, position->x - 16.0, position->z - 16.0, position->x + 16.0,
                                    position->z + 16.0);
                BlockPos below(BlockPos(position->x, spawnY, position->z).below());
                level->setBlock(below, Blocks::sponge->defaultBlockState(), Block::UPDATE_ALL, false);
                generator->AddAdditionalBlock(below);
            }
        }
        int x = (int)position->x;
        if (position->x < (double)x)
            --x;
        int z = (int)position->z;
        if (position->z < (double)z)
            --z;
        player->teleportTo((double)((float)x + 0.5f), spawnY, (double)z + 0.5, 0.0f, (float)position->y);
        double nearest = 1.7976931348623157e+308;
        for (unsigned int otherIndex = 0; otherIndex < positions->size(); ++otherIndex)
        {
            if (position != positions->at(otherIndex))
                nearest = std::min(position->dist(positions->at(otherIndex)), nearest);
        }
        totalDistance += nearest;
        ++playerIndex;
    }
    return totalDistance / (double)playerIndex;
}

void GameModeUtilities::SpreadPlayers(std::vector<boost::shared_ptr<ServerPlayer> >* players, const AABB* area,
                                      double distance, Level* level, bool teams, TumbleGenerator* generator)
{
    Random random;
    std::vector<Position*>* positions =
        createInitialPositions(&random, teams ? getNumberOfTeams(players) : (int)players->size(), area->mMinX,
                               area->mMinZ, area->mMaxX, area->mMaxZ);
    spreadPositions(distance, level, &random, area, positions, teams);
    setPlayerPositions(players, level, positions, teams, generator);
    for (std::vector<Position*>::iterator it = positions->begin(); it != positions->end(); ++it)
        delete *it;
    delete positions;
}

void GameModeUtilities::DistributePlayersInTeams(std::vector<boost::shared_ptr<ServerPlayer> >* players,
                                                 Scoreboard* scoreboard)
{
    std::vector<PlayerTeam*>* teams = scoreboard->getPlayerTeams();
    if (!teams->empty())
    {
        if (!players->empty())
        {
            for (unsigned int i = 0; i < players->size(); ++i)
            {
                PlayerTeam* smallest = NULL;
                for (unsigned int t = 0; t < teams->size(); ++t)
                {
                    PlayerTeam* team = teams->at(t);
                    if (team->getName() != SPECTATOR_TEAM_NAME)
                    {
                        if (smallest == NULL || team->getPlayerCount() < smallest->getPlayerCount())
                            smallest = team;
                    }
                }
                boost::shared_ptr<ServerPlayer> player = players->at(i);
                scoreboard->addPlayerToTeam(player->mXuid, smallest);
            }
        }
        delete teams;
    }
}
