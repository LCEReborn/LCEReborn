#include "game/world/chunk/PlayerChunkMap.hpp"

#include <algorithm>
#include <limits.h>

#include "game/core/Mth.hpp"
#include "game/core/Minecraft.hpp"
#include "game/core/MasterGameMode.hpp"
#include "game/core/PIXNamedEvents.hpp"
#include "game/core/System.hpp"
#include "game/network/ChunkVisibilityAreaPacket.hpp"
#include "game/network/ServerGamePacketListenerImpl.hpp"
#include "game/server/ServerChunkCache.hpp"
#include "game/server/MinecraftServer.hpp"
#include "game/server/ServerPlayer.hpp"
#include "game/server/ServerLevel.hpp"
#include "game/world/generation/PlayerList.hpp"
#include "game/network/INetworkPlayer.hpp"
#include "game/minigame/EMiniGameId.hpp"
#include "game/world/Dimension.hpp"
#include "game/world/DimensionType.hpp"
#include "game/world/Level.hpp"
#include "game/world/chunk/ClientChunkCache.hpp"
#include "game/world/chunk/LevelChunk.hpp"
#include "game/world/chunk/PlayerChunkContainer.hpp"
#include "game/core/VideoSettings.hpp"

PlayerChunkMap::PlayerChunkMap(ServerLevel* level, int dimension, int viewDistance)
{
    mViewDistance = Mth::clamp(viewDistance, static_cast<int>(VideoSettings::RENDER_DISTANCE_MIN),
                               static_cast<int>(VideoSettings::RENDER_DISTANCE_MAX));
    mPauseBlockUpdatePackets = false;
    mLevel = (Level*)level;
    mDimension = dimension;
    mLastUpdate = 0;
    mTicksUntilNextUpdate = 0;
    mShouldDropChunks = false;
}

PlayerChunkMap::~PlayerChunkMap()
{
    for (ContainerMap::iterator it = mChunks.begin(); it != mChunks.end(); ++it)
    {
        delete it->second;
    }
}

void PlayerChunkMap::flagEntitiesToBeRemoved(unsigned int* count, bool* removed)
{
    for (PlayerVector::iterator it = mPlayers.begin(); it != mPlayers.end(); ++it)
    {
        boost::shared_ptr<ServerPlayer> player = *it;
        player->flagEntitiesToBeRemoved(count, removed);
    }
}

Level* PlayerChunkMap::getLevel()
{
    return mLevel;
}

long long PlayerChunkMap::tickAddRequests(boost::shared_ptr<ServerPlayer> player, bool force)
{
    if (!mAddRequests.empty())
    {
        int playerX = static_cast<int>(player->mX);
        int playerZ = static_cast<int>(player->mZ);

        int bestDistance = -1;
        RequestVector::iterator best = mAddRequests.end();
        if (force)
        {
            for (RequestVector::iterator it = mAddRequests.begin(); it != mAddRequests.end(); ++it)
            {
                if (it->player == player)
                {
                    int xd = it->x * 16 + 8 - playerX;
                    int zd = it->z * 16 + 8 - playerZ;
                    int distance = xd * xd + zd * zd;
                    if (bestDistance == -1 || distance < bestDistance)
                    {
                        bestDistance = distance;
                        best = it;
                    }
                }
            }
        }
        else
        {
            for (RequestVector::iterator it = mAddRequests.begin(); it != mAddRequests.end(); ++it)
            {
                if (it->player == player)
                    return System::processTimeInMilliSecs() - player->mChunkRequestTime;
            }
        }

        if (best != mAddRequests.end())
        {
            player->mChunkRequestTime = System::processTimeInMilliSecs();
            getOrCreateChunk(best->x, best->z)->add(best->player, best->add);
            mAddRequests.erase(best);
        }
    }
    return 0;
}

bool PlayerChunkMap::chunkInRange(int x, int z, int centerX, int centerZ, int distance)
{
    int xd = x - centerX;
    int zd = z - centerZ;
    if (xd < -distance || xd > distance)
    {
        return false;
    }
    if (zd < -distance || zd > distance)
    {
        return false;
    }
    return true;
}

int PlayerChunkMap::viewDistanceFor(boost::shared_ptr<ServerPlayer> player)
{
    int requested = player->mConnection != NULL ? player->mConnection->getRequestedViewDistance()
                                                : static_cast<int>(VideoSettings::RENDER_DISTANCE_DEFAULT);
    return Mth::clamp(requested, static_cast<int>(VideoSettings::RENDER_DISTANCE_MIN), mViewDistance);
}

bool PlayerChunkMap::shouldActuallyDropChunks()
{
    return mShouldDropChunks;
}

bool PlayerChunkMap::clientsMustDropChunks()
{
    return mLevel->mDimension->getXZSize() > ClientChunkCache::RING_WIDTH;
}

void PlayerChunkMap::setViewDistance(int distance)
{
    int newDistance = Mth::clamp(distance, static_cast<int>(VideoSettings::RENDER_DISTANCE_MIN),
                                 static_cast<int>(VideoSettings::RENDER_DISTANCE_MAX));
    if (newDistance == mViewDistance)
        return;
    mViewDistance = newDistance;
    PlayerVector players(mPlayers);
    for (PlayerVector::iterator it = players.begin(); it != players.end(); ++it)
        updatePlayerViewDistance(*it);
}

void PlayerChunkMap::updatePlayerViewDistance(boost::shared_ptr<ServerPlayer> player)
{
    if (std::find(mPlayers.begin(), mPlayers.end(), player) == mPlayers.end())
        return;
    const int oldDistance = player->mChunkViewDistance;
    const int newDistance = viewDistanceFor(player);
    if (newDistance == oldDistance)
        return;

    const int chunkX = static_cast<int>(player->mLastSentX) >> 4;
    const int chunkZ = static_cast<int>(player->mLastSentZ) >> 4;
    const int outer = std::max(oldDistance, newDistance);
    for (int x = chunkX - outer; x <= chunkX + outer; ++x)
    {
        for (int z = chunkZ - outer; z <= chunkZ + outer; ++z)
        {
            const bool wasIn = chunkInRange(x, z, chunkX, chunkZ, oldDistance);
            const bool isIn = chunkInRange(x, z, chunkX, chunkZ, newDistance);
            if (isIn && !wasIn)
                getChunkAndAddPlayer(x, z, player, true);
            else if (wasIn && !isIn)
                getChunkAndRemovePlayer(x, z, player, true);
        }
    }
    player->mChunkViewDistance = newDistance;
}

void PlayerChunkMap::tick()
{
    long long gameTime = mLevel->getGameTime();
    if (gameTime - mLastUpdate > 8000)
    {
        mLastUpdate = gameTime;
        for (unsigned int i = 0; i < mAllChunks.size(); i++)
        {
            mAllChunks.at(i)->updateInhabitedTime();
        }
    }

#if LCE_PLATFORM_PC
    ServerChunkCache* cache = static_cast<ServerChunkCache*>(static_cast<ServerLevel*>(mLevel)->getChunkSource());
    for (unsigned int i = 0; i < mAllChunks.size(); i++)
    {
        ChunkPos pos = mAllChunks[i]->getPos();
        if (!cache->hasChunk(pos.x, pos.z))
            continue;
        LevelChunk* chunk = cache->getChunkIfLoaded(pos.x, pos.z);
        if (!chunk->isEmpty())
            chunk->tick(true);
    }
#endif

    if (!mPauseBlockUpdatePackets)
    {
        bool sentRegionUpdate = false;
        for (unsigned int i = 0; i < mChangedChunks.size();)
        {
            sentRegionUpdate |= mChangedChunks[i]->broadcastChanges(!sentRegionUpdate);
            if (mChangedChunks[i]->mBlockChangesCount == 0)
            {
                mChangedChunks[i] = mChangedChunks.back();
                mChangedChunks.pop_back();
            }
            else
            {
                ++i;
            }
        }
    }

    mTicksUntilNextUpdate += 25;
    if (mTicksUntilNextUpdate > 25)
        mTicksUntilNextUpdate = 25;

    PIXBeginNamedEvent(0.0f, "PlayerChunkMap tickAddRequests");
    uint64_t tickStart = System::processTimeInMilliSecs();
    while (mTicksUntilNextUpdate > 0)
    {
        uint64_t start = System::processTimeInMilliSecs();

        int bestIndex = -1;
        long long bestWait = 0;
        for (unsigned int i = 0; i < mPlayers.size(); i++)
        {
            long long wait = tickAddRequests(mPlayers[i], false);
            if (wait > bestWait)
            {
                bestIndex = i;
                bestWait = wait;
            }
        }
        if (bestIndex < 0)
            break;

        tickAddRequests(mPlayers[bestIndex], true);

        uint64_t end = System::processTimeInMilliSecs();
        mTicksUntilNextUpdate -= static_cast<int>(end - start);
    }
    uint64_t tickEnd = System::processTimeInMilliSecs();
    if (mLevel->mDimension->getType() == DimensionType::OVERWORLD)
        PIXAddNamedCounter(static_cast<float>(static_cast<long long>(tickEnd - tickStart)), "Tick add request");
    PIXEndNamedEvent();
}

long long PlayerChunkMap::getKeyForChunk(int x, int z)
{
    return (static_cast<long long>(x) + INT_MAX) | ((static_cast<long long>(z) + INT_MAX) << 32);
}

PlayerChunkContainer* PlayerChunkMap::getChunkIfLoaded(int x, int z)
{
    ContainerMap::iterator it = mChunks.find(getKeyForChunk(x, z));
    PlayerChunkContainer* chunk = NULL;
    if (it != mChunks.end())
        chunk = it->second;
    return chunk;
}

PlayerChunkContainer* PlayerChunkMap::getOrCreateChunk(int x, int z)
{
    long long key = getKeyForChunk(x, z);
    ContainerMap::iterator it = mChunks.find(key);
    PlayerChunkContainer* chunk = NULL;
    if (it != mChunks.end())
    {
        chunk = it->second;
    }
    else
    {
        chunk = new PlayerChunkContainer(this, x, z);
        mChunks[key] = chunk;
        mAllChunks.push_back(chunk);
    }
    return chunk;
}

void PlayerChunkMap::getChunkAndAddPlayer(int x, int z, boost::shared_ptr<ServerPlayer> player, bool send)
{
    MasterGameMode* gameMode = gMinecraftServer->mMasterGameMode;
    if (!Minecraft::InMiniGame(eMiniGameId_Any, true) || !gameMode->IsRoundPlaying()
        || MasterGameMode::IsChunkWithinActiveBounds(gameMode, x, z))
    {
        ContainerMap::iterator it = mChunks.find(getKeyForChunk(x, z));
        if (it != mChunks.end())
            it->second->add(player, send);
        else
            mAddRequests.push_back(PlayerChunkAddRequest(x, z, player, send));
    }
}

void PlayerChunkMap::getChunkAndRemovePlayer(int x, int z, boost::shared_ptr<ServerPlayer> player, bool send)
{
    RequestVector::iterator request = mAddRequests.begin();
    while (request != mAddRequests.end())
    {
        if (request->x == x && request->z == z && request->player.get() == player.get())
        {
            mAddRequests.erase(request);
            return;
        }
        ++request;
    }
    ContainerMap::iterator it = mChunks.find(getKeyForChunk(x, z));
    if (it != mChunks.end())
    {
        it->second->remove(player, send);
    }
}

void PlayerChunkMap::broadcastBlockUpdate(boost::shared_ptr<Packet> packet, int x, int y, int z)
{
    PlayerChunkContainer* chunk = getChunkIfLoaded(x >> 4, z >> 4);
    if (chunk != NULL)
    {
        chunk->broadcast(packet);
    }
}

void PlayerChunkMap::blockChanged(const BlockPos& pos)
{
    int chunkX = pos.x >> 4;
    int chunkZ = pos.z >> 4;
    PlayerChunkContainer* chunk;
    if (static_cast<ServerChunkCache*>(static_cast<ServerLevel*>(mLevel)->getChunkSource())->isCreatingOnRequest())
    {
        chunk = getOrCreateChunk(chunkX, chunkZ);
    }
    else
    {
        chunk = getChunkIfLoaded(chunkX, chunkZ);
    }
    if (chunk != NULL)
    {
        chunk->blockChanged(pos.x & 15, pos.y, pos.z & 15);
    }
}

bool PlayerChunkMap::isTrackingBlock(int x, int y, int z)
{
    return getChunkIfLoaded(x >> 4, z >> 4) != NULL;
}

void PlayerChunkMap::prioritiseBlockChanges(int x, int y, int z)
{
    PlayerChunkContainer* chunk = getChunkIfLoaded(x >> 4, z >> 4);
    if (chunk != NULL)
    {
        chunk->prioritiseBlockChanges();
    }
}

void PlayerChunkMap::add(boost::shared_ptr<ServerPlayer> player)
{
    static int DIRECTIONS[4][2] = {{1, 0}, {0, 1}, {-1, 0}, {0, -1}};

    const int chunkX = static_cast<int>(player->mX) >> 4;
    const int chunkZ = static_cast<int>(player->mZ) >> 4;
    player->mLastSentX = player->mX;
    player->mLastSentZ = player->mZ;

    int direction = 0;
    const int viewDistance = viewDistanceFor(player);
    player->mChunkViewDistance = viewDistance;
    int offsetX = 0;
    int offsetZ = 0;
    getOrCreateChunk(chunkX, chunkZ)->add(player, false);

    int minX = chunkX;
    int maxX = chunkX;
    int minZ = chunkZ;
    int maxZ = chunkZ;
    for (int length = 1; length <= viewDistance * 2; ++length)
    {
        for (int side = 0; side < 2; ++side)
        {
            const int* step = DIRECTIONS[direction++ % 4];
            for (int i = 0; i < length; ++i)
            {
                offsetX += step[0];
                offsetZ += step[1];
                const int x = chunkX + offsetX;
                const int z = chunkZ + offsetZ;
                if (x > maxX)
                    maxX = x;
                if (x < minX)
                    minX = x;
                if (z > maxZ)
                    maxZ = z;
                if (z < minZ)
                    minZ = z;
                if (length < 14 || (length == 14 && (side == 0 || i < 13)))
                    getOrCreateChunk(x, z)->add(player, false);
                else
                    getChunkAndAddPlayer(x, z, player, false);
            }
        }
    }

    direction %= 4;
    for (int i = 0; i < viewDistance * 2; ++i)
    {
        offsetX += DIRECTIONS[direction][0];
        offsetZ += DIRECTIONS[direction][1];
        const int x = chunkX + offsetX;
        const int z = chunkZ + offsetZ;
        if (x > maxX)
            maxX = x;
        if (x < minX)
            minX = x;
        if (z > maxZ)
            maxZ = z;
        if (z < minZ)
            minZ = z;
        if (viewDistance * 2 < 15)
            getOrCreateChunk(x, z)->add(player, false);
        else
            getChunkAndAddPlayer(x, z, player, false);
    }

    INetworkPlayer* networkPlayer = player->mConnection->getNetworkPlayer();
    if (networkPlayer != NULL && !networkPlayer->IsLocal())
    {
        player->mConnection->send(boost::shared_ptr<ChunkVisibilityAreaPacket>(
            new ChunkVisibilityAreaPacket(minX, maxX, minZ, maxZ, true, shouldActuallyDropChunks(), false)));
    }

    static_cast<ServerChunkCache*>(static_cast<ServerLevel*>(getLevel())->getChunkSource())->dontDrop(chunkX, chunkZ);
    mPlayers.push_back(player);
}

void PlayerChunkMap::remove(boost::shared_ptr<ServerPlayer> player)
{
    const int chunkX = static_cast<int>(player->mLastSentX) >> 4;
    const int chunkZ = static_cast<int>(player->mLastSentZ) >> 4;
    const int viewDistance = player->mChunkViewDistance;
    for (int x = chunkX - viewDistance; x <= chunkX + viewDistance; ++x)
    {
        for (int z = chunkZ - viewDistance; z <= chunkZ + viewDistance; ++z)
        {
            PlayerChunkContainer* chunk = getChunkIfLoaded(x, z);
            if (chunk != NULL)
                chunk->remove(player, true);
        }
    }

    PlayerVector::iterator it = std::find(mPlayers.begin(), mPlayers.end(), player);
    if (!mPlayers.empty() && it != mPlayers.end())
        mPlayers.erase(std::find(mPlayers.begin(), mPlayers.end(), player));

    RequestVector::iterator request = mAddRequests.begin();
    while (request != mAddRequests.end())
    {
        if (request->player.get() == player.get())
            request = mAddRequests.erase(request);
        else
            ++request;
    }
}

void PlayerChunkMap::removeOutsideArea(boost::shared_ptr<ServerPlayer> player, int minX, int minZ, int maxX, int maxZ)
{
    const int chunkX = static_cast<int>(player->mLastSentX) >> 4;
    const int chunkZ = static_cast<int>(player->mLastSentZ) >> 4;
    const int viewDistance = player->mChunkViewDistance;
    for (int x = chunkX - viewDistance; x <= chunkX + viewDistance; ++x)
    {
        for (int z = chunkZ - viewDistance; z <= chunkZ + viewDistance; ++z)
        {
            if (x < minX || x > maxX || z < minZ || z > maxZ)
            {
                PlayerChunkContainer* chunk = getChunkIfLoaded(x, z);
                if (chunk != NULL)
                    chunk->remove(player, false);
            }
        }
    }

    player->mConnection->send(boost::shared_ptr<ChunkVisibilityAreaPacket>(
        new ChunkVisibilityAreaPacket(minX, maxX, minZ, maxZ, false, shouldActuallyDropChunks(), true)));

    PlayerVector::iterator it = std::find(mPlayers.begin(), mPlayers.end(), player);
    if (!mPlayers.empty() && it != mPlayers.end())
        mPlayers.erase(std::find(mPlayers.begin(), mPlayers.end(), player));

    RequestVector::iterator request = mAddRequests.begin();
    while (request != mAddRequests.end())
    {
        if (request->player.get() == player.get())
            request = mAddRequests.erase(request);
        else
            ++request;
    }
}

void PlayerChunkMap::move(boost::shared_ptr<ServerPlayer> player)
{
    const int chunkX = static_cast<int>(player->mX) >> 4;
    const int chunkZ = static_cast<int>(player->mZ) >> 4;
    const double deltaXPos = player->mLastSentX - player->mX;
    const double deltaZPos = player->mLastSentZ - player->mZ;
    if (deltaXPos * deltaXPos + deltaZPos * deltaZPos < 64.0)
        return;

    const int lastChunkX = static_cast<int>(player->mLastSentX) >> 4;
    const int lastChunkZ = static_cast<int>(player->mLastSentZ) >> 4;
    const int deltaX = chunkX - lastChunkX;
    const int deltaZ = chunkZ - lastChunkZ;
    if (deltaX == 0 && deltaZ == 0)
        return;

    const bool inMiniGame = Minecraft::InMiniGame(eMiniGameId_Any, true);
    const int viewDistance = player->mChunkViewDistance;
    std::vector<std::pair<int, int> > addedChunks;
    for (int x = chunkX - viewDistance; x <= chunkX + viewDistance; ++x)
    {
        for (int z = chunkZ - viewDistance; z <= chunkZ + viewDistance; ++z)
        {
            if (!chunkInRange(x, z, lastChunkX, lastChunkZ, viewDistance))
            {
                MasterGameMode* gameMode = gMinecraftServer->mMasterGameMode;
                if (!Minecraft::InMiniGame(eMiniGameId_Any, true) || !gameMode->IsRoundPlaying()
                    || MasterGameMode::IsChunkWithinActiveBounds(gameMode, x, z))
                {
                    getChunkAndAddPlayer(x, z, player, false);
                    addedChunks.push_back(std::make_pair(x, z));
                }
            }

            if (!chunkInRange(x - deltaX, z - deltaZ, chunkX, chunkZ, viewDistance) && !inMiniGame)
                getChunkAndRemovePlayer(x - deltaX, z - deltaZ, player, true);
        }
    }

    INetworkPlayer* networkPlayer = player->mConnection->getNetworkPlayer();
    if (networkPlayer != NULL && !networkPlayer->IsLocal())
    {
        while (!addedChunks.empty())
        {
            const int firstX = addedChunks[0].first;
            const int firstZ = addedChunks[0].second;
            int sameRow = 0;
            int sameColumn = 0;
            for (unsigned int i = 0; i < addedChunks.size(); ++i)
            {
                if (addedChunks[i].second == firstZ)
                    ++sameRow;
                if (addedChunks[i].first == firstX)
                    ++sameColumn;
            }

            if (sameRow > sameColumn)
            {
                std::vector<std::pair<int, int> >::iterator it = addedChunks.begin();
                while (it != addedChunks.end())
                {
                    if (it->second == firstZ)
                        it = addedChunks.erase(it);
                    else
                        ++it;
                }
                player->mConnection->send(boost::shared_ptr<ChunkVisibilityAreaPacket>(new ChunkVisibilityAreaPacket(
                    firstX, firstX + sameRow, firstZ, firstZ, true, shouldActuallyDropChunks(), false)));
            }
            else
            {
                std::vector<std::pair<int, int> >::iterator it = addedChunks.begin();
                while (it != addedChunks.end())
                {
                    if (it->first == firstX)
                        it = addedChunks.erase(it);
                    else
                        ++it;
                }
                player->mConnection->send(boost::shared_ptr<ChunkVisibilityAreaPacket>(new ChunkVisibilityAreaPacket(
                    firstX, firstX, firstZ, firstZ + sameColumn, true, shouldActuallyDropChunks(), false)));
            }
        }
    }

    player->mLastSentX = player->mX;
    player->mLastSentZ = player->mZ;
}

bool PlayerChunkMap::isPlayerIn(boost::shared_ptr<ServerPlayer> player, int x, int z)
{
    PlayerChunkContainer* chunk = getChunkIfLoaded(x, z);
    if (chunk == NULL)
        return false;

    std::list<ChunkPos>::iterator pending = std::find(player->mLoadedChunks.begin(), player->mLoadedChunks.end(),
                                                      chunk->getPos());
    return chunk->hasPlayer(player) && pending == player->mLoadedChunks.end();
}

int PlayerChunkMap::convertChunkRangeToBlock(int range)
{
    return (range << 4) - 0x10;
}

void PlayerChunkMap::setRadius(int radius)
{
    if (radius <= mViewDistance)
        return;
    setViewDistance(radius);
}

void PlayerChunkMap::removeOutsideAreaForAllPlayers(int minX, int minZ, int maxX, int maxZ)
{
    (void)minX;
    (void)minZ;
    (void)maxX;
    (void)maxZ;
}

void PlayerChunkMap::reloadAllPlayers(int minX, int minZ, int maxX, int maxZ)
{
    PlayerVector players;
    players.insert(players.end(), mPlayers.begin(), mPlayers.end());

    mShouldDropChunks = true;
    while (!mPlayers.empty())
    {
        removeOutsideArea(mPlayers[0], minX, minZ, maxX, maxZ);
    }
    mShouldDropChunks = false;

    int oldRadius = mViewDistance;
    setRadius(3);
    for (PlayerVector::iterator it = players.begin(); it != players.end(); it++)
    {
        add(*it);
    }
    setRadius(oldRadius);
}

void PlayerChunkMap::PauseBlockUpdatePackets(bool pause)
{
    mPauseBlockUpdatePackets = pause;
}

void PlayerChunkMap::onPlayerJoin(boost::shared_ptr<ServerPlayer> player)
{
    (void)player;
}

void PlayerChunkMap::onPlayerLeave(boost::shared_ptr<ServerPlayer> player)
{
    (void)player;
}
