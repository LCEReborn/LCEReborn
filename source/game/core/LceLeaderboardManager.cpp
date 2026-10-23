#include "game/core/LceLeaderboardManager.hpp"

#include <cstring>

#include <boost/shared_ptr.hpp>

#include "game/core/ByteArrayInputStream.hpp"
#include "game/core/ByteArrayOutputStream.hpp"
#include "game/core/DataInputStream.hpp"
#include "game/core/DataOutputStream.hpp"
#include "game/core/LeaderboardBoards.hpp"
#include "game/core/Minecraft.hpp"
#include "game/core/System.hpp"
#include "game/network/ClientPacketListener.hpp"
#include "game/network/LeaderboardReadListener.hpp"
#include "game/network/ServerboundCustomPayloadPacket.hpp"
#include "platform/CProfile.hpp"

LeaderboardManager* g_pLeaderboardManager = new LceLeaderboardManager();

namespace
{
const long long kServerTimeoutMs = 5000;
}

LceLeaderboardManager::LceLeaderboardManager()
    : m_pending(ePending_None),
      m_result(eStatsReturn_Failed),
      m_listener(NULL),
      m_board(0),
      m_pad(0),
      m_totalRows(0),
      m_sentAt(0)
{
}

LceLeaderboardManager::~LceLeaderboardManager()
{
}

void LceLeaderboardManager::Tick()
{
    if (m_pending == ePending_Server && System::processTimeInMilliSecs() - m_sentAt > kServerTimeoutMs)
        Finish(eStatsReturn_Failed);
    if (m_pending != ePending_Ready)
        return;
    m_pending = ePending_None;
    LeaderboardReadListener* listener = m_listener;
    m_listener = NULL;
    if (listener == NULL)
        return;
    ReadView view;
    view.m_uiNumScores = static_cast<unsigned int>(m_rows.size());
    view.m_pScores = m_rows.empty() ? NULL : &m_rows[0];
    listener->OnStatsReadComplete(m_result, static_cast<int>(m_totalRows), view);
}

bool LceLeaderboardManager::OpenSession()
{
    return true;
}

void LceLeaderboardManager::CloseSession()
{
}

void LceLeaderboardManager::DeleteSession()
{
}

bool LceLeaderboardManager::WriteStats(unsigned int, RegisterScore*)
{
    return true;
}

void LceLeaderboardManager::FlushStats()
{
}

void LceLeaderboardManager::CancelOperation()
{
    m_pending = ePending_None;
    m_listener = NULL;
}

bool LceLeaderboardManager::isIdle()
{
    return m_pending == ePending_None;
}

void LceLeaderboardManager::Begin(LeaderboardReadListener* listener, int userIndex, EStatsType board)
{
    m_listener = listener;
    m_pad = userIndex >= 0 && userIndex < 4 ? userIndex : gProfile->GetPrimaryPad();
    m_board = static_cast<int>(board);
    m_rows.clear();
    m_totalRows = 0;
}

void LceLeaderboardManager::Finish(eStatsReturn result)
{
    m_result = result == eStatsReturn_Success && m_rows.empty() ? eStatsReturn_NoScores : result;
    m_pending = ePending_Ready;
}

bool LceLeaderboardManager::ReadStats_Friends(LeaderboardReadListener* pReadListener, int iUserIndex,
                                              EStatsType eStatsType, PlayerUID, unsigned int, unsigned int)
{
    Begin(pReadListener, iUserIndex, eStatsType);
    const int pad = m_pad;
    Minecraft* minecraft = Minecraft::GetInstance();
    StatsCounter* counter = minecraft != NULL && pad >= 0 && pad < 4 ? minecraft->mStats[pad] : NULL;
    const int columns = LeaderboardBoards::ColumnCount(m_board);
    if (counter == NULL || columns == 0)
    {
        Finish(eStatsReturn_Failed);
        return true;
    }
    ReadScore score;
    std::memset(score.m_auiStats, 0, sizeof(score.m_auiStats));
    gProfile->GetXUID(pad, &score.m_uid, false);
    score.m_uiRank = 1;
    score.m_wsGamertag = gProfile->GetDisplayName(pad);
    score.m_uiRating = 0;
    score.m_usNumStats = static_cast<unsigned short>(columns);
    score.m_iMessageStringId = 0;
    for (int column = 0; column < columns; ++column)
        score.m_auiStats[column] = LeaderboardBoards::CounterValue(counter, m_board, column);
    m_rows.push_back(score);
    m_totalRows = 1;
    Finish(eStatsReturn_Success);
    return true;
}

bool LceLeaderboardManager::ReadStats_MyScore(LeaderboardReadListener* pReadListener, int iUserIndex,
                                              EStatsType eStatsType, PlayerUID, unsigned int uiNumRows)
{
    Begin(pReadListener, iUserIndex, eStatsType);
    RequestFromServer(true, 1, uiNumRows);
    return true;
}

bool LceLeaderboardManager::ReadStats_TopRank(LeaderboardReadListener* pReadListener, int iUserIndex,
                                              EStatsType eStatsType, unsigned int uiStartRank, unsigned int uiNumRows)
{
    Begin(pReadListener, iUserIndex, eStatsType);
    RequestFromServer(false, uiStartRank, uiNumRows);
    return true;
}

void LceLeaderboardManager::RequestFromServer(bool aroundMe, unsigned int startRank, unsigned int count)
{
    Minecraft* minecraft = Minecraft::GetInstance();
    const int pad = m_pad;
    ClientPacketListener* connection = minecraft != NULL && pad >= 0 && pad < 4 ? minecraft->getConnection(pad) : NULL;
    if (connection == NULL)
    {
        Finish(eStatsReturn_Failed);
        return;
    }
    ByteArrayOutputStream bytes;
    DataOutputStream output(&bytes);
    output.writeByte(static_cast<unsigned char>(m_board));
    output.writeByte(aroundMe ? 1 : 0);
    output.writeInt(static_cast<int>(startRank));
    output.writeInt(static_cast<int>(count));
    connection->send(boost::shared_ptr<Packet>(
        new ServerboundCustomPayloadPacket(ServerboundCustomPayloadPacket::LEADERBOARD_PACKET, bytes.toByteArray())));
    m_sentAt = System::processTimeInMilliSecs();
    m_pending = ePending_Server;
}

void LceLeaderboardManager::OnServerRows(arrayWithLength<unsigned char> data)
{
    ByteArrayInputStream bytes(data);
    DataInputStream input(&bytes);
    const int board = input.readByte();
    const int total = input.readInt();
    const int rows = input.readShort();
    if (m_pending != ePending_Server || board != m_board)
    {
        bytes.reset();
        input.close();
        return;
    }
    m_rows.clear();
    for (int i = 0; i < rows; ++i)
    {
        ReadScore score;
        std::memset(score.m_auiStats, 0, sizeof(score.m_auiStats));
        score.m_uiRank = static_cast<unsigned int>(input.readInt());
        score.m_wsGamertag = input.readUTF();
        score.m_uid = PlayerUID(input.readUTF());
        const int columns = input.readByte();
        score.m_usNumStats = static_cast<unsigned short>(columns < MAX_STATS_PER_SCORE ? columns : MAX_STATS_PER_SCORE);
        for (int column = 0; column < columns; ++column)
        {
            const unsigned int value = static_cast<unsigned int>(input.readInt());
            if (column < MAX_STATS_PER_SCORE)
                score.m_auiStats[column] = value;
        }
        score.m_uiRating = 0;
        score.m_iMessageStringId = 0;
        m_rows.push_back(score);
    }
    bytes.reset();
    input.close();
    m_totalRows = total < 0 ? 0 : static_cast<unsigned int>(total);
    Finish(eStatsReturn_Success);
}
