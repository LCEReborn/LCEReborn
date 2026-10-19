#ifndef PLATFORM_PC_AUDIO_PCSOUNDBANK_HPP
#define PLATFORM_PC_AUDIO_PCSOUNDBANK_HPP

#include <stddef.h>

#include <string>
#include <unordered_map>
#include <vector>

// Reads the game's sound bank (Minecraft.msscmp): its events and the Bink
// Audio files they play.
class PcSoundBank
{
public:
    struct Sound
    {
        std::string name;
        const unsigned char* data;
        size_t size;
        int channels;
        int sampleRate;
        int maxInstances;
        int lengthMs;
    };

    struct Choice
    {
        std::string sound;
        int weight;
    };

    enum SelectionMode
    {
        SELECT_RANDOM = 0,
        SELECT_RANDOM_NO_REPEAT = 1,
        SELECT_IN_ORDER = 2
    };

    enum LimitPolicy
    {
        LIMIT_LOWER_PRIORITY = 0,
        LIMIT_FARTHEST = 1,
        LIMIT_QUIETEST = 2,
        LIMIT_OLDEST = 3
    };

    struct Play
    {
        std::vector<Choice> choices;
        std::string completionEvent;
        std::string labels;
        bool stream;
        bool loadOnDemand;
        int delayMinimumMs;
        int delayMaximumMs;
        int priority;
        int loopCount;
        float volumeMinimum;
        float volumeMaximum;
        float pitchMinimum;
        float pitchMaximum;
        float fadeTime;
        int limitPolicy;
        int selectionMode;
    };

    struct Event
    {
        std::string name;
        std::vector<Play> plays;
        std::vector<std::string> cache;
    };

    PcSoundBank();

    // Takes the whole bank file. Fails only when the bank itself is unreadable;
    // events and sounds this reader cannot play are left out and listed in
    // Skipped().
    bool Load(std::vector<unsigned char>& bytes, std::string* error);

    const Event* FindEvent(const char* name) const;
    const Sound* FindSound(const char* name) const;

    const std::vector<Event>& Events() const { return m_events; }
    const std::vector<Sound>& Sounds() const { return m_sounds; }
    const std::vector<std::string>& Skipped() const { return m_skipped; }

private:
    int Word(size_t offset) const;
    bool Contains(long long offset, long long length) const;
    const char* String(long long offset) const;
    bool ReadSounds(std::string* error);
    bool ReadEvents(std::string* error);
    bool ParseEvent(const std::string& name, const char* text, Event* event, std::string* reason);

    std::vector<unsigned char> m_bytes;
    std::vector<Sound> m_sounds;
    std::vector<Event> m_events;
    std::vector<std::string> m_skipped;
    std::unordered_map<std::string, size_t> m_soundIndex;
    std::unordered_map<std::string, size_t> m_eventIndex;
};

#endif
