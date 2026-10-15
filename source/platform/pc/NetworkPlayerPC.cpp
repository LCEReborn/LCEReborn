#include "platform/pc/NetworkPlayerPC.hpp"

#include <climits>

#include "game/core/System.hpp"
#include "platform/pc/CPlatformNetworkManagerPC.hpp"

NetworkPlayerPC::NetworkPlayerPC(CPlatformNetworkManagerPC* manager, EType type, unsigned int systemId, int userIndex,
                                 unsigned char smallId, const PlayerUID& uid, const std::wstring& onlineName,
                                 const std::wstring& displayName)
    : m_manager(manager),
      m_type(type),
      m_systemId(systemId),
      m_userIndex(userIndex),
      m_smallId(smallId),
      m_uid(uid),
      m_onlineName(onlineName),
      m_displayName(displayName),
      m_socket(0),
      m_lastChunkPacketTime(0)
{
}

NetworkPlayerPC::~NetworkPlayerPC()
{
}

unsigned char NetworkPlayerPC::GetSmallId()
{
    return m_smallId;
}

void NetworkPlayerPC::SendData(INetworkPlayer* to, const void* data, int size, bool, bool)
{
    NetworkPlayerPC* target = static_cast<NetworkPlayerPC*>(to);
    if (target == 0 || size <= 0 || !IsSameSystem(target))
        return;
    if ((m_type == eType_Host && target->m_type == eType_Local) || (m_type == eType_Local && target->m_type == eType_Host))
        m_manager->LocalDataSend(this, target, data, (unsigned int)size);
}

bool NetworkPlayerPC::IsSameSystem(INetworkPlayer* other)
{
    return other != 0 && m_systemId == static_cast<NetworkPlayerPC*>(other)->m_systemId;
}

int NetworkPlayerPC::GetOutstandingAckCount()
{
    return m_manager->GetOutstandingAckCount(this);
}

int NetworkPlayerPC::GetSinglePlayerOutstandingAckCount()
{
    return 0;
}

void NetworkPlayerPC::DecrementAcksOutstanding()
{
}

int NetworkPlayerPC::GetSendQueueSizeBytes(INetworkPlayer*, bool)
{
    return 0;
}

int NetworkPlayerPC::GetSendQueueSizeMessages(INetworkPlayer*, bool)
{
    return 0;
}

int NetworkPlayerPC::GetCurrentRtt()
{
    return 0;
}

bool NetworkPlayerPC::IsHost()
{
    return m_type == eType_Host;
}

bool NetworkPlayerPC::IsGuest()
{
    return (m_uid.flags & PlayerUID::FlagMiiPrincipal) != 0;
}

bool NetworkPlayerPC::IsLocal()
{
    return m_systemId == CPlatformNetworkManagerPC::cLocalSystemId;
}

int NetworkPlayerPC::GetSessionIndex()
{
    return m_manager->GetSessionIndex(this);
}

bool NetworkPlayerPC::IsTalking()
{
    return false;
}

bool NetworkPlayerPC::IsMutedByLocalUser(int)
{
    return false;
}

bool NetworkPlayerPC::HasVoice()
{
    return false;
}

bool NetworkPlayerPC::HasCamera()
{
    return false;
}

int NetworkPlayerPC::GetUserIndex()
{
    return m_userIndex;
}

void NetworkPlayerPC::SetSocket(Socket* socket)
{
    m_socket = socket;
}

Socket* NetworkPlayerPC::GetSocket()
{
    return m_socket;
}

const wchar_t* NetworkPlayerPC::GetOnlineName()
{
    return m_onlineName.c_str();
}

std::wstring NetworkPlayerPC::GetDisplayName()
{
    if (IsLocal())
        return m_displayName;
    return m_onlineName;
}

PlayerUID NetworkPlayerPC::GetUID()
{
    return m_uid;
}

void NetworkPlayerPC::SetUID(PlayerUID uid)
{
    m_uid = uid;
}

void NetworkPlayerPC::SentChunkPacket()
{
    m_lastChunkPacketTime = System::processTimeInMilliSecs();
}

int NetworkPlayerPC::GetTimeSinceLastChunkPacket_ms()
{
    if (m_lastChunkPacketTime == 0)
        return INT_MAX;
    int64_t now = System::processTimeInMilliSecs();
    return (int)(now - m_lastChunkPacketTime);
}

bool NetworkPlayerPC::isFake()
{
    return false;
}

IQNetPlayer* NetworkPlayerPC::GetQNetPlayer()
{
    return 0;
}

NetworkPlayerPC::EType NetworkPlayerPC::GetType() const
{
    return m_type;
}

unsigned int NetworkPlayerPC::GetSystemId() const
{
    return m_systemId;
}
