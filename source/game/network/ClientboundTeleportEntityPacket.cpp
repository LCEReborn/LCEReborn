#include "game/network/ClientboundTeleportEntityPacket.hpp"
#include "game/core/DataInputStream.hpp"
#include "game/core/DataOutputStream.hpp"
#include "game/network/PacketListener.hpp"
#include "game/entity/mob/Entity.hpp"
#include <boost/pointer_cast.hpp>
#include "game/core/Mth.hpp"

ClientboundTeleportEntityPacket::ClientboundTeleportEntityPacket()
    : mId(-1), mX(0), mY(0), mZ(0), myRot(0), mxRot(0), mOnGround(false), mWide(false)
{
}

ClientboundTeleportEntityPacket::ClientboundTeleportEntityPacket(boost::shared_ptr<Entity> entity)
{
    mWide = false;
    mId = entity->getId();
    mX = Mth::floor(entity->mX * 32.0);
    mY = Mth::floor(entity->mY * 32.0);
    mZ = Mth::floor(entity->mZ * 32.0);
    myRot = (unsigned char)Mth::floor(entity->mYRot * 256.0f / 360.0f);
    mxRot = (unsigned char)Mth::floor(entity->mXRot * 256.0f / 360.0f);
    mOnGround = entity->mOnGround;
}

int ClientboundTeleportEntityPacket::getPacketId()
{
    return 34;
}
void ClientboundTeleportEntityPacket::read(DataInputStream* input)
{
    mId = input->readVarInt();
    mX = mWide ? input->readInt() : input->readShort();
    mY = mWide ? input->readInt() : input->readShort();
    mZ = mWide ? input->readInt() : input->readShort();
    myRot = input->readByte();
    mxRot = input->readByte();
    mOnGround = input->readBoolean();
}

void ClientboundTeleportEntityPacket::write(DataOutputStream* output)
{
    output->writeVarInt(mId);
    output->writeShort((short)mX);
    output->writeShort((short)mY);
    output->writeShort((short)mZ);
    output->writeByte(myRot);
    output->writeByte(mxRot);
    output->writeBoolean(mOnGround);
}

void ClientboundTeleportEntityPacket::handle(PacketListener* listener)
{
    listener->handleTeleportEntity(shared_from_this());
}

int ClientboundTeleportEntityPacket::getEstimatedSize()
{
    return 10;
}

bool ClientboundTeleportEntityPacket::hasWideForm()
{
    return true;
}

int ClientboundTeleportEntityPacket::getWidePacketId()
{
    return WIDE_PACKET_ID;
}

void ClientboundTeleportEntityPacket::writeWide(DataOutputStream* output)
{
    output->writeVarInt(mId);
    output->writeInt(mX);
    output->writeInt(mY);
    output->writeInt(mZ);
    output->writeByte(myRot);
    output->writeByte(mxRot);
    output->writeBoolean(mOnGround);
}

boost::shared_ptr<Packet> ClientboundTeleportEntityPacket::createWide()
{
    ClientboundTeleportEntityPacket* packet = new ClientboundTeleportEntityPacket();
    packet->mWide = true;
    return boost::shared_ptr<Packet>(packet);
}

bool ClientboundTeleportEntityPacket::canBeInvalidated()
{
    return true;
}

bool ClientboundTeleportEntityPacket::isInvalidatedBy(boost::shared_ptr<Packet> packet)
{
    boost::shared_ptr<ClientboundTeleportEntityPacket> other =
        boost::dynamic_pointer_cast<ClientboundTeleportEntityPacket>(packet);
    return other->mId == mId;
}

int ClientboundTeleportEntityPacket::getId()
{
    return mId;
}

int ClientboundTeleportEntityPacket::getX()
{
    return mX;
}

int ClientboundTeleportEntityPacket::getY()
{
    return mY;
}

int ClientboundTeleportEntityPacket::getZ()
{
    return mZ;
}

unsigned char ClientboundTeleportEntityPacket::getyRot()
{
    return myRot;
}

unsigned char ClientboundTeleportEntityPacket::getxRot()
{
    return mxRot;
}

unsigned char ClientboundTeleportEntityPacket::isOnGround()
{
    return mOnGround;
}
