#include "net/NetMessages.hpp"

#include <string.h>

namespace lcenet
{
namespace
{
const size_t kMaxTextLength = 256;

void WriteGamertag(ByteWriter& writer, const GameSessionData::Gamertag& gamertag)
{
    writer.Bytes(gamertag.m_name, sizeof(gamertag.m_name));
}

void ReadGamertag(ByteReader& reader, GameSessionData::Gamertag& gamertag)
{
    reader.Bytes(gamertag.m_name, sizeof(gamertag.m_name));
    gamertag.m_name[sizeof(gamertag.m_name) - 1] = 0;
}
}

void ByteWriter::U8(unsigned int value)
{
    m_data.push_back(static_cast<unsigned char>(value));
}

void ByteWriter::U16(unsigned int value)
{
    U8(value >> 8);
    U8(value);
}

void ByteWriter::U32(uint32_t value)
{
    U16(value >> 16);
    U16(value & 0xffff);
}

void ByteWriter::Bytes(const void* data, size_t size)
{
    const unsigned char* bytes = static_cast<const unsigned char*>(data);
    m_data.insert(m_data.end(), bytes, bytes + size);
}

void ByteWriter::Text(const std::wstring& text)
{
    Text(text, kMaxTextLength);
}

void ByteWriter::Text(const std::wstring& text, size_t maxLength)
{
    size_t length = text.size() < maxLength ? text.size() : maxLength;
    U16(static_cast<unsigned int>(length));
    for (size_t index = 0; index < length; ++index)
        U16(text[index] <= 0xffff ? static_cast<unsigned int>(text[index]) : 0xfffdu);
}

void ByteWriter::Uid(const PlayerUID& uid)
{
    if ((uid.flags & PlayerUID::FlagUuid) != 0)
    {
        Bytes(uid.uuid.data, sizeof(uid.uuid.data));
    }
    else
    {
        Bytes(uid.mii.createId, sizeof(uid.mii.createId));
        U16(0);
        U32(uid.mii.principalId);
    }
    U32(uid.flags);
}

bool ByteReader::Need(size_t size)
{
    if (!m_ok || m_size - m_offset < size)
        m_ok = false;
    return m_ok;
}

unsigned int ByteReader::U8()
{
    if (!Need(1))
        return 0;
    return m_data[m_offset++];
}

unsigned int ByteReader::U16()
{
    unsigned int high = U8();
    return (high << 8) | U8();
}

uint32_t ByteReader::U32()
{
    uint32_t high = U16();
    return (high << 16) | U16();
}

void ByteReader::Bytes(void* data, size_t size)
{
    if (!Need(size))
    {
        memset(data, 0, size);
        return;
    }
    memcpy(data, m_data + m_offset, size);
    m_offset += size;
}

std::wstring ByteReader::Text()
{
    return Text(kMaxTextLength);
}

std::wstring ByteReader::Text(size_t maxLength)
{
    size_t length = U16();
    if (length > maxLength)
    {
        m_ok = false;
        return std::wstring();
    }
    std::wstring text;
    for (size_t index = 0; index < length && m_ok; ++index)
        text.push_back(static_cast<wchar_t>(U16()));
    return text;
}

PlayerUID ByteReader::Uid()
{
    unsigned char body[16];
    Bytes(body, sizeof(body));
    PlayerUID uid(0);
    uid.flags = U32();
    if ((uid.flags & PlayerUID::FlagUuid) != 0)
    {
        memcpy(uid.uuid.data, body, sizeof(uid.uuid.data));
    }
    else
    {
        memcpy(uid.mii.createId, body, sizeof(uid.mii.createId));
        uid.mii.principalId = ((uint32_t)body[12] << 24) | ((uint32_t)body[13] << 16) | ((uint32_t)body[14] << 8)
                              | body[15];
    }
    return uid;
}

uint32_t DetailsHash(const std::wstring& motd, const std::vector<unsigned char>& icon)
{
    uint32_t hash = 2166136261u;
    for (size_t index = 0; index < motd.size(); ++index)
        hash = (hash ^ (static_cast<uint32_t>(motd[index]) & 0xffff)) * 16777619u;
    hash = (hash ^ 0x1f) * 16777619u;
    for (size_t index = 0; index < icon.size(); ++index)
        hash = (hash ^ icon[index]) * 16777619u;
    return hash;
}

void WriteSession(ByteWriter& writer, const SessionDescription& session, bool details)
{
    const GameSessionData& data = session.data;
    writer.U16(kProtocolVersion);
    writer.U16(session.port);
    writer.U32(session.nonce);
    writer.Text(session.hostName);
    writer.U16(data.m_version);
    WriteGamertag(writer, data.m_hostGamertag);
    for (int index = 0; index < GameSessionData::MAX_SESSION_PLAYERS; ++index)
        WriteGamertag(writer, data.m_playerGamertags[index]);
    writer.U32(data.m_gameHostOptions);
    writer.U32(data.m_texturePackParentId);
    writer.U8(data.m_subTexturePackId);
    writer.U8(data.m_bPublicGame ? 1 : 0);
    writer.U8(data.m_numPlayers);
    writer.U8(data.m_bGameStarted ? 1 : 0);
    writer.U8(data.m_gameModeId);
    writer.U8(data.m_miniGameId);
    writer.U32(data.m_customGameModeHostOptions);
    writer.U8(session.maxPlayers);
    writer.U8(session.dedicated ? 1 : 0);
    writer.U32(session.detailsHash);
    writer.U8(details ? 1 : 0);
    if (!details)
        return;
    writer.Text(session.motd, kMaxMotdLength);
    const size_t iconSize = session.icon.size() <= kMaxIconSize ? session.icon.size() : 0;
    writer.U16(static_cast<unsigned int>(iconSize));
    if (iconSize != 0)
        writer.Bytes(&session.icon[0], iconSize);
}

bool ReadSession(ByteReader& reader, SessionDescription& session, unsigned int* version)
{
    const unsigned int sentVersion = reader.U16();
    if (version != NULL)
        *version = reader.Ok() ? sentVersion : 0;
    if (sentVersion != kProtocolVersion)
        return false;
    GameSessionData& data = session.data;
    session.port = static_cast<unsigned short>(reader.U16());
    session.nonce = reader.U32();
    session.hostName = reader.Text();
    data.m_version = static_cast<unsigned short>(reader.U16());
    ReadGamertag(reader, data.m_hostGamertag);
    for (int index = 0; index < GameSessionData::MAX_SESSION_PLAYERS; ++index)
        ReadGamertag(reader, data.m_playerGamertags[index]);
    data.m_gameHostOptions = reader.U32();
    data.m_texturePackParentId = reader.U32();
    data.m_subTexturePackId = static_cast<unsigned char>(reader.U8());
    data.m_bPublicGame = reader.U8() != 0;
    data.m_numPlayers = static_cast<unsigned char>(reader.U8());
    data.m_bGameStarted = reader.U8() != 0;
    data.m_gameModeId = reader.U8() & 0xf;
    data.m_miniGameId = reader.U8() & 0xf;
    data.m_customGameModeHostOptions = reader.U32();
    session.maxPlayers = static_cast<unsigned char>(reader.U8());
    session.dedicated = reader.U8() != 0;
    session.detailsHash = reader.U32();
    session.hasDetails = reader.U8() != 0;
    session.motd.clear();
    session.icon.clear();
    if (session.hasDetails)
    {
        session.motd = reader.Text(kMaxMotdLength);
        const size_t iconSize = reader.U16();
        if (iconSize > kMaxIconSize)
            return false;
        session.icon.resize(iconSize);
        if (iconSize != 0)
            reader.Bytes(&session.icon[0], iconSize);
    }
    return reader.Ok();
}
}
