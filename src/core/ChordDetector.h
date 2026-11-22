#pragma once

#include "core/NoteListener.h"

#include <array>
#include <atomic>
#include <bitset>
#include <cstdint>
#include <vector>

namespace piano
{
class ChordDetector
{
public:
    static constexpr int lowestPitch = 21, highestPitch = 108;

    void prepare (double sampleRate);
    void reset();

    void setWatched (const std::vector<int>& chord, const std::vector<int>& held = {});

    void process (const float* samples, int numSamples, std::vector<NoteEvent>& out);

    float salienceDb (int pitch) const;
    bool  isSounding (int pitch) const;

    bool  hasFundamental (int pitch) const;

private:
    struct Fft
    {
        int size = 0;
        std::vector<float> window, re, im, power;
        std::vector<int> bitrev;
        std::vector<float> cosTable, sinTable;
        struct Peak { float hz; float amp; };
        std::vector<Peak> peaks;
        void prepare (int n);
        void run (const float* newestLast, int available, double sampleRate);
    };

    struct PitchState
    {
        std::array<float, 16> history {};
        int   historyPos = 0;
        float peakSinceOnset = -200.0f;
        float troughSincePeak = -200.0f;
        std::array<float, 3> raw { -200.0f, -200.0f, -200.0f };
        bool  sounding = false;
        double onsetTime = -1.0;
        int   refractory = 0;
        int   uncertainHops = 0;
        int   quietHops = 0;
    };

    float measure (int pitch) const;
    float unsharedEvidence (const Fft& f, int pitch, int ratio) const;
    static float peakAmplitude (const Fft& f, double hz, double centsTolerance);

    double rate = 48000.0;
    int hop = 256;
    Fft fftLong, fftShort;
    std::vector<float> ring;
    int writePos = 0, filled = 0, sinceHop = 0;
    std::int64_t samplesSeen = 0;
    std::vector<float> linear;

    std::array<std::atomic<std::uint64_t>, 2> watchedBits {};
    std::array<std::atomic<std::uint64_t>, 2> chordBits {};
    std::array<std::uint64_t, 2> prevWatched {};
    std::array<PitchState, 128> states {};
    std::array<float, 128> lastSalience {};
};

}
