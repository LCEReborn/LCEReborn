/*
 * Bink Audio decoder for the PC build.
 *
 * The container parsing and the bitstream decoding are ported from FFmpeg's
 * libavformat/binka.c and libavcodec/binkaudio.c:
 *   Copyright (c) 2007-2011 Peter Ross (pross@xvid.org)
 *   Copyright (c) 2009 Daniel Verkamp (daniel@drv.nu)
 *   Copyright (c) 2021 Paul B Mahol
 * FFmpeg is licensed under the GNU Lesser General Public License, version 2.1
 * or later; this port is used under version 3 of the GNU General Public
 * License, as that license allows.
 */
#include "platform/pc/audio/BinkAudioDecoder.hpp"

#include <math.h>
#include <string.h>

namespace
{
const unsigned int MAGIC = 0x31464342;
const size_t HEADER_BYTES = 22;
const size_t PACKET_HEADER_BYTES = 4;
const float DCT_SCALE_NUMERATOR = 2.0f;
const double PI = 3.14159265358979323846;
const float MAX_SAMPLE = 32767.0f / 32768.0f;

const unsigned short CRITICAL_FREQUENCIES[25] = {
    100,  200,  300,  400,  510,  630,  770,  920,  1080, 1270,  1480,  1720,  2000,
    2320, 2700, 3150, 3700, 4400, 5300, 6400, 7700, 9500, 12000, 15500, 24500,
};

const unsigned char RLE_LENGTHS[16] = {2, 3, 4, 5, 6, 8, 9, 10, 11, 12, 13, 14, 15, 16, 32, 64};

unsigned int ReadLE16(const unsigned char* bytes)
{
    return bytes[0] | (bytes[1] << 8);
}

unsigned int ReadLE32(const unsigned char* bytes)
{
    return bytes[0] | (bytes[1] << 8) | (bytes[2] << 16) | (static_cast<unsigned int>(bytes[3]) << 24);
}

unsigned int ReadBE32(const unsigned char* bytes)
{
    return (static_cast<unsigned int>(bytes[0]) << 24) | (bytes[1] << 16) | (bytes[2] << 8) | bytes[3];
}
}

unsigned int BinkAudioDecoder::BitReader::Read(int count)
{
    unsigned int value = 0;
    int written = 0;
    while (written < count)
    {
        if (position >= bits)
            return value;
        size_t byte = position >> 3;
        int shift = static_cast<int>(position & 7);
        int take = 8 - shift;
        if (take > count - written)
            take = count - written;
        unsigned int chunk = (data[byte] >> shift) & ((1u << take) - 1);
        value |= chunk << written;
        written += take;
        position += take;
    }
    return value;
}

BinkAudioDecoder::BinkAudioDecoder()
    : m_data(NULL),
      m_size(0),
      m_firstPacket(0),
      m_nextPacket(0),
      m_channels(0),
      m_sampleRate(0),
      m_totalFrames(0),
      m_framesOut(0),
      m_frameLength(0),
      m_overlapLength(0),
      m_bandCount(0),
      m_root(0.0f),
      m_first(true),
      m_inPacket(false)
{
    memset(m_bands, 0, sizeof(m_bands));
    memset(m_quantTable, 0, sizeof(m_quantTable));
    memset(&m_reader, 0, sizeof(m_reader));
}

bool BinkAudioDecoder::Open(const unsigned char* data, size_t size)
{
    if (data == NULL || size < HEADER_BYTES || ReadBE32(data) != MAGIC || (data[4] != 1 && data[4] != 2))
        return false;

    int channels = data[5];
    int sampleRate = static_cast<int>(ReadLE16(data + 6));
    if (channels < 1 || channels > MAX_CHANNELS || sampleRate <= 0)
        return false;

    size_t seekEntries = ReadLE16(data + 20);
    size_t firstPacket = HEADER_BYTES + seekEntries * 2 + 2;
    if (firstPacket > size)
        return false;

    m_data = data;
    m_size = size;
    m_firstPacket = firstPacket;
    m_channels = channels;
    m_sampleRate = sampleRate;
    m_totalFrames = ReadLE32(data + 8);

    int frameLengthBits = sampleRate < 22050 ? 9 : (sampleRate < 44100 ? 10 : 11);
    m_frameLength = 1 << frameLengthBits;
    m_overlapLength = m_frameLength / 16;

    int sampleRateHalf = (sampleRate + 1) / 2;
    m_root = m_frameLength / (sqrtf(static_cast<float>(m_frameLength)) * 32768.0f);
    for (int i = 0; i < 96; ++i)
        m_quantTable[i] = expf(i * 0.15289164787221953823f) * m_root;

    for (m_bandCount = 1; m_bandCount < 25; ++m_bandCount)
    {
        if (sampleRateHalf <= CRITICAL_FREQUENCIES[m_bandCount - 1])
            break;
    }
    m_bands[0] = 2;
    for (int i = 1; i < m_bandCount; ++i)
        m_bands[i] = (CRITICAL_FREQUENCIES[i - 1] * m_frameLength / sampleRateHalf) & ~1u;
    m_bands[m_bandCount] = m_frameLength;

    for (int channel = 0; channel < MAX_CHANNELS; ++channel)
    {
        m_previous[channel].assign(m_overlapLength, 0.0f);
        m_blocks[channel].assign(m_frameLength, 0.0f);
    }
    m_coefficients.assign(m_frameLength + 2, 0.0f);

    int fftLength = m_frameLength * 2;
    m_fftReal.assign(fftLength, 0.0f);
    m_fftImag.assign(fftLength, 0.0f);
    m_twiddleReal.resize(fftLength / 2);
    m_twiddleImag.resize(fftLength / 2);
    for (int i = 0; i < fftLength / 2; ++i)
    {
        double angle = 2.0 * PI * i / fftLength;
        m_twiddleReal[i] = static_cast<float>(cos(angle));
        m_twiddleImag[i] = static_cast<float>(sin(angle));
    }
    m_modulateReal.resize(m_frameLength);
    m_modulateImag.resize(m_frameLength);
    for (int i = 0; i < m_frameLength; ++i)
    {
        double angle = PI * i / fftLength;
        m_modulateReal[i] = static_cast<float>(cos(angle));
        m_modulateImag[i] = static_cast<float>(sin(angle));
    }
    m_bitReverse.resize(fftLength);
    int fftBits = frameLengthBits + 1;
    for (int i = 0; i < fftLength; ++i)
    {
        unsigned int reversed = 0;
        for (int bit = 0; bit < fftBits; ++bit)
        {
            if (i & (1 << bit))
                reversed |= 1u << (fftBits - 1 - bit);
        }
        m_bitReverse[i] = reversed;
    }

    Rewind();
    return true;
}

void BinkAudioDecoder::Rewind()
{
    m_nextPacket = m_firstPacket;
    m_framesOut = 0;
    m_first = true;
    m_inPacket = false;
    for (int channel = 0; channel < MAX_CHANNELS; ++channel)
        m_previous[channel].assign(m_overlapLength, 0.0f);
}

bool BinkAudioDecoder::NextPacket()
{
    if (m_nextPacket + PACKET_HEADER_BYTES > m_size)
        return false;
    size_t payload = ReadLE16(m_data + m_nextPacket + 2);
    size_t start = m_nextPacket + PACKET_HEADER_BYTES;
    if (payload == 0 || start + payload > m_size)
        return false;
    m_reader.data = m_data + start;
    m_reader.bits = payload * 8;
    m_reader.position = 0;
    m_nextPacket = start + payload;
    m_inPacket = true;
    return true;
}

void BinkAudioDecoder::InverseDct(float* coefficients, float* out)
{
    const int length = m_frameLength;
    const int fftLength = length * 2;
    float* real = &m_fftReal[0];
    float* imag = &m_fftImag[0];

    for (int i = 0; i < fftLength; ++i)
    {
        unsigned int source = m_bitReverse[i];
        if (source < static_cast<unsigned int>(length))
        {
            float coefficient = source == 0 ? coefficients[0] * 0.5f : coefficients[source];
            real[i] = coefficient * m_modulateReal[source];
            imag[i] = coefficient * m_modulateImag[source];
        }
        else
        {
            real[i] = 0.0f;
            imag[i] = 0.0f;
        }
    }

    for (int size = 2; size <= fftLength; size <<= 1)
    {
        int half = size >> 1;
        int step = fftLength / size;
        for (int start = 0; start < fftLength; start += size)
        {
            for (int k = 0; k < half; ++k)
            {
                float wr = m_twiddleReal[k * step];
                float wi = m_twiddleImag[k * step];
                int a = start + k;
                int b = a + half;
                float br = real[b] * wr - imag[b] * wi;
                float bi = real[b] * wi + imag[b] * wr;
                real[b] = real[a] - br;
                imag[b] = imag[a] - bi;
                real[a] += br;
                imag[a] += bi;
            }
        }
    }

    float scale = DCT_SCALE_NUMERATOR / length;
    for (int i = 0; i < length; ++i)
        out[i] = real[i] * scale;
}

bool BinkAudioDecoder::DecodeChannels()
{
    BitReader& reader = m_reader;
    float* coefficients = &m_coefficients[0];
    int channels = m_channels;

    reader.Read(2);

    for (int channel = 0; channel < channels; ++channel)
    {
        if (reader.Left() < 58)
            return false;
        for (int i = 0; i < 2; ++i)
        {
            int power = static_cast<int>(reader.Read(5));
            float value = ldexpf(static_cast<float>(reader.Read(23)), power - 23);
            if (reader.Read(1))
                value = -value;
            coefficients[i] = value * m_root;
        }

        if (reader.Left() < static_cast<size_t>(m_bandCount) * 8)
            return false;
        float quant[25];
        for (int i = 0; i < m_bandCount; ++i)
        {
            unsigned int value = reader.Read(8);
            quant[i] = m_quantTable[value < 95 ? value : 95];
        }

        int band = 0;
        float q = quant[0];
        int i = 2;
        while (i < m_frameLength)
        {
            int end;
            if (reader.Read(1))
                end = i + RLE_LENGTHS[reader.Read(4)] * 8;
            else
                end = i + 8;
            if (end > m_frameLength)
                end = m_frameLength;

            int width = static_cast<int>(reader.Read(4));
            if (width == 0)
            {
                memset(coefficients + i, 0, (end - i) * sizeof(float));
                i = end;
                while (m_bands[band] < static_cast<unsigned int>(i))
                    q = quant[band++];
            }
            else
            {
                while (i < end)
                {
                    if (m_bands[band] == static_cast<unsigned int>(i))
                        q = quant[band++];
                    unsigned int coefficient = reader.Read(width);
                    if (coefficient != 0)
                        coefficients[i] = reader.Read(1) ? -q * coefficient : q * coefficient;
                    else
                        coefficients[i] = 0.0f;
                    ++i;
                }
            }
        }

        coefficients[0] /= 0.5f;
        InverseDct(coefficients, &m_blocks[channel][0]);
    }

    for (int channel = 0; channel < channels; ++channel)
    {
        float* block = &m_blocks[channel][0];
        float* previous = &m_previous[channel][0];
        for (int i = 0; i < m_frameLength; ++i)
            block[i] = block[i] < -1.0f ? -1.0f : (block[i] > MAX_SAMPLE ? MAX_SAMPLE : block[i]);
        int count = m_overlapLength * channels;
        if (!m_first)
        {
            int j = channel;
            for (int i = 0; i < m_overlapLength; ++i, j += channels)
                block[i] = (previous[i] * (count - j) + block[i] * j) / count;
        }
        memcpy(previous, block + m_frameLength - m_overlapLength, m_overlapLength * sizeof(float));
    }

    m_first = false;
    return true;
}

int BinkAudioDecoder::DecodeBlock(float* out)
{
    if (m_data == NULL || m_framesOut >= m_totalFrames)
        return 0;
    if (!m_inPacket && !NextPacket())
        return 0;

    if (!DecodeChannels())
    {
        m_inPacket = false;
        return -1;
    }

    size_t misaligned = m_reader.position & 31;
    if (misaligned != 0)
        m_reader.position += 32 - misaligned;
    if (m_reader.Left() == 0)
        m_inPacket = false;

    int frames = m_frameLength - m_overlapLength;
    if (m_framesOut + frames > m_totalFrames)
        frames = static_cast<int>(m_totalFrames - m_framesOut);

    for (int channel = 0; channel < m_channels; ++channel)
    {
        const float* block = &m_blocks[channel][0];
        for (int i = 0; i < frames; ++i)
            out[i * m_channels + channel] = block[i];
    }

    m_framesOut += frames;
    return frames;
}
