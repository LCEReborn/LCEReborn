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
#ifndef PLATFORM_PC_AUDIO_BINKAUDIODECODER_HPP
#define PLATFORM_PC_AUDIO_BINKAUDIODECODER_HPP

#include <stddef.h>

#include <vector>

// Decodes a standalone Bink Audio file ("1FCB") held in memory.
class BinkAudioDecoder
{
public:
    static const int MAX_CHANNELS = 2;

    BinkAudioDecoder();

    // The data must stay valid while the decoder uses it.
    bool Open(const unsigned char* data, size_t size);
    void Rewind();

    int Channels() const { return m_channels; }
    int SampleRate() const { return m_sampleRate; }
    unsigned int TotalFrames() const { return m_totalFrames; }
    int MaxBlockFrames() const { return m_frameLength - m_overlapLength; }

    // Decodes the next block as interleaved floats into out, which must hold
    // MaxBlockFrames() * Channels() values. Returns the frames written, 0 at
    // the end of the file, or -1 if the data is damaged.
    int DecodeBlock(float* out);

private:
    struct BitReader
    {
        const unsigned char* data;
        size_t bits;
        size_t position;

        unsigned int Read(int count);
        size_t Left() const { return bits > position ? bits - position : 0; }
    };

    bool NextPacket();
    bool DecodeChannels();
    void InverseDct(float* coefficients, float* out);

    const unsigned char* m_data;
    size_t m_size;
    size_t m_firstPacket;
    size_t m_nextPacket;
    int m_channels;
    int m_sampleRate;
    unsigned int m_totalFrames;
    unsigned int m_framesOut;

    int m_frameLength;
    int m_overlapLength;
    int m_bandCount;
    float m_root;
    unsigned int m_bands[26];
    float m_quantTable[96];
    bool m_first;
    bool m_inPacket;
    BitReader m_reader;

    std::vector<float> m_previous[MAX_CHANNELS];
    std::vector<float> m_blocks[MAX_CHANNELS];
    std::vector<float> m_coefficients;
    std::vector<float> m_fftReal;
    std::vector<float> m_fftImag;
    std::vector<float> m_twiddleReal;
    std::vector<float> m_twiddleImag;
    std::vector<float> m_modulateReal;
    std::vector<float> m_modulateImag;
    std::vector<unsigned int> m_bitReverse;
};

#endif
