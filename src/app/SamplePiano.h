#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

#include <memory>
#include <vector>

namespace app
{
struct SampleRegion
{
    std::shared_ptr<const std::vector<std::int16_t>> data;
    int    frames = 0;
    double sampleRate = 44100.0;
    int    loKey = 0, hiKey = 127, keyCenter = 60;
    int    loVel = 1, hiVel = 127;
    double tuneCents = 0.0;
    float  gain = 1.0f;
    float  release = 0.35f;
    float  loRand = 0.0f, hiRand = 1.0f;
};

struct SampleSet
{
    juce::String name;
    std::vector<SampleRegion> regions;
    size_t bytes = 0;
    bool velocityLayers = false;

    const SampleRegion* find (int key, int velocity, float random) const;

    juce::Range<int> keyRange() const;
};

std::shared_ptr<const SampleSet> loadSampleSet (const juce::File& location, juce::String& error);

bool builtInPianoData (const void*& data, size_t& size);
bool hasBuiltInPiano();
std::shared_ptr<const SampleSet> loadBuiltInPiano (juce::String& error);

juce::Array<juce::File> soundFolders (const juce::File& appDataFolder, bool includeProject);
juce::Array<juce::File> findSounds (const juce::Array<juce::File>& folders);

juce::SynthesiserSound* createSampleSound (std::shared_ptr<const SampleSet> set);
juce::SynthesiserVoice* createSampleVoice();

}
