#pragma once

#include "app/SamplePiano.h"
#include "core/Score.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>
#include <atomic>
#include <vector>

namespace app
{
class PianoPlayer
{
public:
    PianoPlayer();
    ~PianoPlayer();

    void prepare (double sampleRate, int maxBlockSize);

    void start (std::vector<piano::Note> notes, std::vector<std::uint8_t> lit, double fromSeconds);
    void stop();
    bool isPlaying() const noexcept;
    double position() const noexcept { return pos.load(); }
    std::vector<int> soundingPitches() const;

    void takeStrikes (std::array<std::uint32_t, 128>& seen, std::vector<int>& struck) const;
    void setVolume (float gain) noexcept { volume = gain; }
    void setSpeed (float ratio) noexcept { speed = ratio; }

    void setSamples (std::shared_ptr<const SampleSet> samples);

    void render (float* const* outputs, int numOutputs, int numSamples);

private:
    struct Command
    {
        std::vector<piano::Note> notes;
        std::vector<std::uint8_t> lit;
        double from = 0.0;
        bool play = false;
        int generation = 0;
    };

    void takeCommand();

    juce::Synthesiser synth;
    juce::MidiBuffer midi;
    juce::AudioBuffer<float> scratch;
    double sampleRate = 48000.0;

    juce::SpinLock lock;
    Command pending;
    bool hasPending = false;

    std::vector<piano::Note> notes;
    std::vector<std::uint8_t> lit;
    struct Off { double end; int pitch; bool lit; };
    std::vector<Off> offs;
    std::array<int, 128> counts {};
    size_t next = 0;
    double time = 0.0, endTime = 0.0;
    bool playing = false;
    int generation = 0;

    std::atomic<double> pos { 0.0 };
    std::atomic<int> requested { 0 }, finished { 0 };
    std::atomic<bool> wanted { false };
    std::array<std::atomic<std::uint64_t>, 2> sounding {};
    std::array<std::atomic<std::uint32_t>, 128> strikes {};
    std::atomic<float> volume { 0.7f };
    std::atomic<float> speed { 1.0f };
};

}
