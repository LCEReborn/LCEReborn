#include "platform/pc/audio/PcMixer.hpp"

#include <math.h>
#include <string.h>

#include <SDL3/SDL.h>

#include "platform/pc/Log.hpp"

namespace
{
const Logger sLog("Audio");
const int VOICE_COUNT = 128;
const int SLOT_BITS = 8;
const int SLOT_MASK = (1 << SLOT_BITS) - 1;
const int DEFAULT_OUTPUT_RATE = 48000;
const int MIX_CHUNK_FRAMES = 512;
const int OUTPUT_CHANNELS = 2;
}

PcMixer::PcMixer() : m_stream(NULL), m_outputRate(DEFAULT_OUTPUT_RATE), m_subsystem(false)
{
}

PcMixer::~PcMixer()
{
    Close();
}

bool PcMixer::Open()
{
    if (m_stream != NULL)
        return true;

    if (!SDL_InitSubSystem(SDL_INIT_AUDIO))
    {
        LOG_WARN(sLog, "SDL audio could not be started: %s", SDL_GetError());
        return false;
    }
    m_subsystem = true;

    SDL_AudioSpec device;
    int deviceFrames = 0;
    m_outputRate = DEFAULT_OUTPUT_RATE;
    if (SDL_GetAudioDeviceFormat(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &device, &deviceFrames) && device.freq > 0)
        m_outputRate = device.freq;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_voices.clear();
        m_voices.resize(VOICE_COUNT);
        for (size_t i = 0; i < m_voices.size(); ++i)
        {
            m_voices[i].used = false;
            m_voices[i].generation = 1;
        }
        m_mixBuffer.assign(MIX_CHUNK_FRAMES * OUTPUT_CHANNELS, 0.0f);
    }

    SDL_AudioSpec spec;
    spec.format = SDL_AUDIO_F32;
    spec.channels = OUTPUT_CHANNELS;
    spec.freq = m_outputRate;
    m_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, Callback, this);
    if (m_stream == NULL)
    {
        LOG_WARN(sLog, "The audio device could not be opened: %s", SDL_GetError());
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        m_subsystem = false;
        return false;
    }

    SDL_ResumeAudioStreamDevice(m_stream);
    LOG_INFO(sLog, "Audio output at %d Hz", m_outputRate);
    return true;
}

void PcMixer::Close()
{
    if (m_stream != NULL)
    {
        SDL_DestroyAudioStream(m_stream);
        m_stream = NULL;
    }
    if (m_subsystem)
    {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        m_subsystem = false;
    }
    std::lock_guard<std::mutex> lock(m_mutex);
    m_voices.clear();
}

PcMixer::Voice* PcMixer::Find(int voice)
{
    if (voice < 0)
        return NULL;
    int slot = voice & SLOT_MASK;
    if (slot >= static_cast<int>(m_voices.size()))
        return NULL;
    Voice& found = m_voices[slot];
    if (!found.used || found.generation != static_cast<unsigned int>(voice >> SLOT_BITS))
        return NULL;
    return &found;
}

int PcMixer::Play(const unsigned char* data, size_t size, std::vector<unsigned char>* owned, int loopCount, bool muted,
                  float rate, float left, float right)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    int slot = -1;
    for (size_t i = 0; i < m_voices.size(); ++i)
    {
        if (!m_voices[i].used)
        {
            slot = static_cast<int>(i);
            break;
        }
    }
    if (slot < 0)
        return INVALID_VOICE;

    Voice& voice = m_voices[slot];
    voice.owned.clear();
    if (owned != NULL)
    {
        voice.owned.swap(*owned);
        data = voice.owned.empty() ? NULL : &voice.owned[0];
        size = voice.owned.size();
    }
    if (data == NULL || !voice.decoder.Open(data, size))
    {
        voice.owned.clear();
        return INVALID_VOICE;
    }

    voice.used = true;
    voice.finished = false;
    voice.muted = muted;
    voice.paused = false;
    voice.generation = (voice.generation + 1) & ((1u << (31 - SLOT_BITS)) - 1);
    if (voice.generation == 0)
        voice.generation = 1;
    voice.frames.assign((voice.decoder.MaxBlockFrames() + 1) * voice.decoder.Channels(), 0.0f);
    voice.framesBase = 0;
    voice.framesCount = 0;
    voice.position = 0.0;
    voice.rate = rate > 0.0f ? rate : 1.0;
    voice.step = voice.rate * voice.decoder.SampleRate() / m_outputRate;
    voice.loopsLeft = loopCount;
    voice.level[0] = voice.target[0] = left;
    voice.level[1] = voice.target[1] = right;
    return slot | static_cast<int>(voice.generation << SLOT_BITS);
}

void PcMixer::SetLevels(int voice, float left, float right)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    Voice* found = Find(voice);
    if (found != NULL)
    {
        found->target[0] = left;
        found->target[1] = right;
    }
}

void PcMixer::SetRate(int voice, float rate)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    Voice* found = Find(voice);
    if (found != NULL && rate > 0.0f)
    {
        found->rate = rate;
        found->step = found->rate * found->decoder.SampleRate() / m_outputRate;
    }
}

void PcMixer::SetMuted(int voice, bool muted)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    Voice* found = Find(voice);
    if (found != NULL && found->muted != muted)
    {
        found->muted = muted;
        found->decoder.Rewind();
        found->framesBase = 0;
        found->framesCount = 0;
        if (!muted)
            FetchFrames(*found, static_cast<long long>(found->position));
    }
}

void PcMixer::SetPaused(int voice, bool paused)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    Voice* found = Find(voice);
    if (found != NULL)
        found->paused = paused;
}

void PcMixer::Stop(int voice)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    Voice* found = Find(voice);
    if (found != NULL)
        found->finished = true;
}

bool PcMixer::IsFinished(int voice)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    Voice* found = Find(voice);
    return found == NULL || found->finished;
}

void PcMixer::Release(int voice)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    Voice* found = Find(voice);
    if (found != NULL)
    {
        found->used = false;
        found->finished = true;
        found->owned.clear();
    }
}

void PcMixer::Callback(void* user, SDL_AudioStream* stream, int additional, int)
{
    PcMixer* mixer = static_cast<PcMixer*>(user);
    int frames = additional / static_cast<int>(sizeof(float) * OUTPUT_CHANNELS);
    while (frames > 0)
    {
        int chunk = frames < MIX_CHUNK_FRAMES ? frames : MIX_CHUNK_FRAMES;
        {
            std::lock_guard<std::mutex> lock(mixer->m_mutex);
            if (mixer->m_mixBuffer.empty())
                return;
            mixer->Mix(&mixer->m_mixBuffer[0], chunk);
            SDL_PutAudioStreamData(stream, &mixer->m_mixBuffer[0],
                                   chunk * static_cast<int>(sizeof(float) * OUTPUT_CHANNELS));        }
        frames -= chunk;
    }
}

void PcMixer::Mix(float* out, int frames)
{
    memset(out, 0, frames * OUTPUT_CHANNELS * sizeof(float));
    for (size_t i = 0; i < m_voices.size(); ++i)
    {
        Voice& voice = m_voices[i];
        if (voice.used && !voice.finished && !voice.paused)
            MixVoice(voice, out, frames);
    }
    for (int i = 0; i < frames * OUTPUT_CHANNELS; ++i)
        out[i] = out[i] > 1.0f ? 1.0f : (out[i] < -1.0f ? -1.0f : out[i]);
}

bool PcMixer::Restart(Voice& voice)
{
    if (voice.loopsLeft == 1)
    {
        voice.finished = true;
        return false;
    }
    if (voice.loopsLeft > 1)
        --voice.loopsLeft;
    voice.position -= voice.decoder.TotalFrames();
    if (voice.position < 0.0)
        voice.position = 0.0;
    voice.decoder.Rewind();
    voice.framesBase = 0;
    voice.framesCount = 0;
    return voice.decoder.TotalFrames() > 0;
}

bool PcMixer::FetchFrames(Voice& voice, long long frame)
{
    const int channels = voice.decoder.Channels();
    while (frame >= voice.framesBase + voice.framesCount)
    {
        int keep = voice.framesCount > 0 ? 1 : 0;
        if (keep)
        {
            memmove(&voice.frames[0], &voice.frames[(voice.framesCount - 1) * channels], channels * sizeof(float));
            voice.framesBase += voice.framesCount - 1;
        }
        int decoded = voice.decoder.DecodeBlock(&voice.frames[keep * channels]);
        if (decoded <= 0)
        {
            voice.framesCount = keep;
            return false;
        }
        voice.framesCount = keep + decoded;
    }
    return true;
}

void PcMixer::MixVoice(Voice& voice, float* out, int frames)
{
    const int channels = voice.decoder.Channels();
    const long long total = voice.decoder.TotalFrames();
    float stepLeft = (voice.target[0] - voice.level[0]) / frames;
    float stepRight = (voice.target[1] - voice.level[1]) / frames;
    float left = voice.level[0];
    float right = voice.level[1];

    for (int i = 0; i < frames; ++i)
    {
        left += stepLeft;
        right += stepRight;

        if (voice.position >= total && !Restart(voice))
            break;

        if (!voice.muted)
        {
            long long frame = static_cast<long long>(voice.position);
            if (!FetchFrames(voice, frame))
            {
                if (!Restart(voice))
                    break;
                continue;
            }
            float fraction = static_cast<float>(voice.position - frame);
            long long next = frame + 1 < total ? frame + 1 : frame;
            const float* first = &voice.frames[(frame - voice.framesBase) * channels];
            const float* second = first;
            if (next != frame && FetchFrames(voice, next))
            {
                first = &voice.frames[(frame - voice.framesBase) * channels];
                second = &voice.frames[(next - voice.framesBase) * channels];
            }
            float sampleLeft = first[0] + (second[0] - first[0]) * fraction;
            float sampleRight = channels > 1 ? first[1] + (second[1] - first[1]) * fraction : sampleLeft;
            out[i * OUTPUT_CHANNELS] += sampleLeft * left;
            out[i * OUTPUT_CHANNELS + 1] += sampleRight * right;
        }

        voice.position += voice.step;
    }

    voice.level[0] = voice.target[0];
    voice.level[1] = voice.target[1];
}
