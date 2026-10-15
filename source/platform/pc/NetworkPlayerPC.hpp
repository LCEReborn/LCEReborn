#ifndef PLATFORM_PC_NETWORKPLAYERPC_HPP
#define PLATFORM_PC_NETWORKPLAYERPC_HPP

#include <stdint.h>
#include <string>

#include "game/network/INetworkPlayer.hpp"
#include "game/world/generation/PlayerUID.hpp"

class CPlatformNetworkManagerPC;
class IQNetPlayer;
class Socket;

class NetworkPlayerPC : public INetworkPlayer
{
public:
    enum EType
    {
        eType_Host,
        eType_Local
    };

    NetworkPlayerPC(CPlatformNetworkManagerPC* manager, EType type, unsigned int systemId, int userIndex,
                    unsigned char smallId, const PlayerUID& uid, const std::wstring& onlineName,
                    const std::wstring& displayName);
    virtual ~NetworkPlayerPC();

    virtual unsigned char GetSmallId();
    virtual void SendData(INetworkPlayer* to, const void* data, int size, bool reliable, bool requiresAck);
    virtual bool IsSameSystem(INetworkPlayer* other);
    virtual int GetOutstandingAckCount();
    virtual int GetSinglePlayerOutstandingAckCount();
    virtual void DecrementAcksOutstanding();
    virtual int GetSendQueueSizeBytes(INetworkPlayer* to, bool reliable);
    virtual int GetSendQueueSizeMessages(INetworkPlayer* to, bool reliable);
    virtual int GetCurrentRtt();
    virtual bool IsHost();
    virtual bool IsGuest();
    virtual bool IsLocal();
    virtual int GetSessionIndex();
    virtual bool IsTalking();
    virtual bool IsMutedByLocalUser(int userIndex);
    virtual bool HasVoice();
    virtual bool HasCamera();
    virtual int GetUserIndex();
    virtual void SetSocket(Socket* socket);
    virtual Socket* GetSocket();
    virtual const wchar_t* GetOnlineName();
    virtual std::wstring GetDisplayName();
    virtual PlayerUID GetUID();
    virtual void SetUID(PlayerUID uid);
    virtual void SentChunkPacket();
    virtual int GetTimeSinceLastChunkPacket_ms();
    virtual bool isFake();
    virtual IQNetPlayer* GetQNetPlayer();

    EType GetType() const;
    unsigned int GetSystemId() const;

private:
    CPlatformNetworkManagerPC* m_manager;
    EType m_type;
    unsigned int m_systemId;
    int m_userIndex;
    unsigned char m_smallId;
    PlayerUID m_uid;
    std::wstring m_onlineName;
    std::wstring m_displayName;
    Socket* m_socket;
    int64_t m_lastChunkPacketTime;
};

#endif
