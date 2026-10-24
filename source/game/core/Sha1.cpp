#include "game/core/Sha1.hpp"

#include <cstring>

static uint32_t RotateLeft(uint32_t value, int bits)
{
    return (value << bits) | (value >> (32 - bits));
}

Sha1::Sha1() : m_length(0), m_buffered(0)
{
    m_state[0] = 0x67452301u;
    m_state[1] = 0xefcdab89u;
    m_state[2] = 0x98badcfeu;
    m_state[3] = 0x10325476u;
    m_state[4] = 0xc3d2e1f0u;
}

void Sha1::processBlock(const unsigned char* block)
{
    uint32_t words[80];
    for (int index = 0; index < 16; ++index)
        words[index] = (static_cast<uint32_t>(block[index * 4]) << 24)
                       | (static_cast<uint32_t>(block[index * 4 + 1]) << 16)
                       | (static_cast<uint32_t>(block[index * 4 + 2]) << 8) | static_cast<uint32_t>(block[index * 4 + 3]);
    for (int index = 16; index < 80; ++index)
        words[index] = RotateLeft(words[index - 3] ^ words[index - 8] ^ words[index - 14] ^ words[index - 16], 1);

    uint32_t a = m_state[0];
    uint32_t b = m_state[1];
    uint32_t c = m_state[2];
    uint32_t d = m_state[3];
    uint32_t e = m_state[4];
    for (int index = 0; index < 80; ++index)
    {
        uint32_t f;
        uint32_t k;
        if (index < 20)
        {
            f = (b & c) | (~b & d);
            k = 0x5a827999u;
        }
        else if (index < 40)
        {
            f = b ^ c ^ d;
            k = 0x6ed9eba1u;
        }
        else if (index < 60)
        {
            f = (b & c) | (b & d) | (c & d);
            k = 0x8f1bbcdcu;
        }
        else
        {
            f = b ^ c ^ d;
            k = 0xca62c1d6u;
        }
        const uint32_t next = RotateLeft(a, 5) + f + e + k + words[index];
        e = d;
        d = c;
        c = RotateLeft(b, 30);
        b = a;
        a = next;
    }
    m_state[0] += a;
    m_state[1] += b;
    m_state[2] += c;
    m_state[3] += d;
    m_state[4] += e;
}

void Sha1::update(const void* data, size_t size)
{
    const unsigned char* bytes = static_cast<const unsigned char*>(data);
    m_length += size;
    while (size > 0)
    {
        const size_t take = size < 64 - m_buffered ? size : 64 - m_buffered;
        std::memcpy(m_buffer + m_buffered, bytes, take);
        m_buffered += take;
        bytes += take;
        size -= take;
        if (m_buffered == 64)
        {
            processBlock(m_buffer);
            m_buffered = 0;
        }
    }
}

void Sha1::finish(unsigned char digest[DIGEST_SIZE])
{
    const uint64_t bits = m_length * 8;
    const unsigned char pad = 0x80;
    update(&pad, 1);
    const unsigned char zero = 0;
    while (m_buffered != 56)
        update(&zero, 1);
    unsigned char length[8];
    for (int index = 0; index < 8; ++index)
        length[index] = static_cast<unsigned char>(bits >> (56 - index * 8));
    update(length, 8);
    for (int index = 0; index < 5; ++index)
    {
        digest[index * 4] = static_cast<unsigned char>(m_state[index] >> 24);
        digest[index * 4 + 1] = static_cast<unsigned char>(m_state[index] >> 16);
        digest[index * 4 + 2] = static_cast<unsigned char>(m_state[index] >> 8);
        digest[index * 4 + 3] = static_cast<unsigned char>(m_state[index]);
    }
}

std::string Sha1::toHex(const unsigned char digest[DIGEST_SIZE])
{
    static const char kDigits[] = "0123456789abcdef";
    std::string text;
    for (int index = 0; index < DIGEST_SIZE; ++index)
    {
        text += kDigits[digest[index] >> 4];
        text += kDigits[digest[index] & 15];
    }
    return text;
}
