#ifndef GAME_NETWORK_CLIENTBOUNDCUSTOMPAYLOADPACKET_HPP
#define GAME_NETWORK_CLIENTBOUNDCUSTOMPAYLOADPACKET_HPP

#include <string>

#include <boost/enable_shared_from_this.hpp>
#include <boost/shared_ptr.hpp>

#include "game/network/Packet.hpp"
#include "game/world/biome/arrayWithLength.hpp"

class DataInputStream;
class DataOutputStream;
class PacketListener;

class ClientboundCustomPayloadPacket : public Packet,
                                       public boost::enable_shared_from_this<ClientboundCustomPayloadPacket>
{
public:
    ClientboundCustomPayloadPacket();
    ClientboundCustomPayloadPacket(const std::wstring& identifier, arrayWithLength<unsigned char> data);
    virtual ~ClientboundCustomPayloadPacket();

    virtual int getPacketId();
    virtual void read(DataInputStream* input);
    virtual void write(DataOutputStream* output);
    virtual void handle(PacketListener* listener);
    virtual int getEstimatedSize();

    static const std::wstring WORLD_SIZE_PACKET;
    static const std::wstring LEADERBOARD_PACKET;
    static const std::wstring COMMANDS_PACKET;
    static const std::wstring PACK_PACKET;

    std::wstring getIdentifier();
    arrayWithLength<unsigned char> getData();

    static boost::shared_ptr<Packet> create()
    {
        return boost::shared_ptr<Packet>(new ClientboundCustomPayloadPacket());
    }

private:
    std::wstring identifier;
    arrayWithLength<unsigned char> data;
    int dataLength;
};

#endif
