#ifndef NET_NETMESSAGES_HPP
#define NET_NETMESSAGES_HPP

#include <stddef.h>
#include <stdint.h>
#include <string>
#include <vector>

#include "game/core/GameSessionData.hpp"
#include "game/world/generation/PlayerUID.hpp"

namespace lcenet
{
const uint16_t kProtocolVersion = 1;
const uint32_t kBeaconMagic = 0x4c434542;

enum ControlChannel
{
    kChannelSessionInfo = 0xf9,
    kChannelQuery = 0xfa,
    kChannelJoinFailed = 0xfc,
    kChannelLeave = 0xfd,
    kChannelJoin = 0xfe,
    kChannelRoomSync = 0xff
};

class ByteWriter
{
public:
    void U8(unsigned int value);
    void U16(unsigned int value);
    void U32(uint32_t value);
    void Bytes(const void* data, size_t size);
    void Text(const std::wstring& text);
    void Uid(const PlayerUID& uid);
    const std::vector<unsigned char>& Data() const { return m_data; }

private:
    std::vector<unsigned char> m_data;
};

class ByteReader
{
public:
    ByteReader(const unsigned char* data, size_t size) : m_data(data), m_size(size), m_offset(0), m_ok(true) {}
    explicit ByteReader(const std::vector<unsigned char>& data)
        : m_data(data.empty() ? 0 : &data[0]), m_size(data.size()), m_offset(0), m_ok(true)
    {
    }

    unsigned int U8();
    unsigned int U16();
    uint32_t U32();
    void Bytes(void* data, size_t size);
    std::wstring Text();
    PlayerUID Uid();
    bool Ok() const { return m_ok; }

private:
    bool Need(size_t size);

    const unsigned char* m_data;
    size_t m_size;
    size_t m_offset;
    bool m_ok;
};

struct SessionDescription
{
    SessionDescription() : port(0), nonce(0) {}

    unsigned short port;
    uint32_t nonce;
    std::wstring hostName;
    GameSessionData data;
};

void WriteSession(ByteWriter& writer, const SessionDescription& session);
bool ReadSession(ByteReader& reader, SessionDescription& session);
}

#endif
