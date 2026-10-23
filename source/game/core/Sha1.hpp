#ifndef GAME_CORE_SHA1_HPP
#define GAME_CORE_SHA1_HPP

#include <stdint.h>
#include <string>

class Sha1
{
public:
    enum
    {
        DIGEST_SIZE = 20
    };

    Sha1();

    void update(const void* data, size_t size);
    void finish(unsigned char digest[DIGEST_SIZE]);

    static std::string toHex(const unsigned char digest[DIGEST_SIZE]);

private:
    void processBlock(const unsigned char* block);

    uint32_t m_state[5];
    uint64_t m_length;
    unsigned char m_buffer[64];
    size_t m_buffered;
};

#endif
