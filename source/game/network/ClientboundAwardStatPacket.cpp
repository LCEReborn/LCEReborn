#include "game/network/ClientboundAwardStatPacket.hpp"

#include "game/core/DataInputStream.hpp"
#include "game/core/DataOutputStream.hpp"
#include "game/network/PacketListener.hpp"

ClientboundAwardStatPacket::ClientboundAwardStatPacket()
{
}

ClientboundAwardStatPacket::ClientboundAwardStatPacket(int statId, int count)
{
    this->statId = statId;
    paramData = arrayWithLength<unsigned char>(4, true);
    *(int*)paramData.data = count;
}

ClientboundAwardStatPacket::ClientboundAwardStatPacket(int statId, arrayWithLength<unsigned char> paramData)
{
    this->statId = statId;
    this->paramData = paramData;
}

ClientboundAwardStatPacket::~ClientboundAwardStatPacket()
{
    if (paramData.data != NULL)
    {
        delete[] paramData.data;
        paramData.data = NULL;
    }
}

int ClientboundAwardStatPacket::getPacketId()
{
    return 200;
}

void ClientboundAwardStatPacket::read(DataInputStream* input)
{
    statId = input->readInt();
    int length = input->readInt();
    if (length > 0)
    {
        paramData = arrayWithLength<unsigned char>(length, true);
        input->readBytes(paramData);
#if LCE_PLATFORM_PC
        if (length == 4)
        {
            unsigned char* bytes = paramData.data;
            *(int*)paramData.data = (int)(((unsigned int)bytes[0] << 24) | ((unsigned int)bytes[1] << 16)
                                          | ((unsigned int)bytes[2] << 8) | bytes[3]);
        }
#endif
    }
}

void ClientboundAwardStatPacket::write(DataOutputStream* output)
{
    output->writeInt(statId);
    output->writeInt(paramData.length);
#if LCE_PLATFORM_PC
    if (paramData.length == 4)
    {
        output->writeInt(*(int*)paramData.data);
        return;
    }
#endif
    if (paramData.length != 0)
    {
        output->writeBytes(paramData);
    }
}

void ClientboundAwardStatPacket::handle(PacketListener* listener)
{
    listener->handleAwardStat(shared_from_this());
    paramData.data = NULL;
}

int ClientboundAwardStatPacket::getEstimatedSize()
{
    return 6;
}

bool ClientboundAwardStatPacket::isAync()
{
    return true;
}

int ClientboundAwardStatPacket::getCount()
{
    return *(int*)paramData.data;
}

arrayWithLength<unsigned char> ClientboundAwardStatPacket::getParamData()
{
    return paramData;
}
