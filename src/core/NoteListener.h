#pragma once

#include "core/PitchDetector.h"

#include <array>
#include <cstdint>
#include <vector>

namespace piano
{
struct ListenerOptions
{
    float minFrequency   = 60.0f;
    float maxFrequency   = 2200.0f;
    int   hopSize        = 128;
    float yinThreshold   = 0.15f;
    float minClarity     = 0.80f;
    float gateDb         = -50.0f;
    float onsetRiseDb    = 4.0f;
    float onsetOverPeakDb = 0.5f;
    float maxCentsOff    = 45.0f;
    int   stableFrames   = 3;
    int   releaseFrames  = 10;
    bool  fastHighNotes  = true;
};

struct NoteEvent
{
    enum class Type { On, Off };
    Type   type    = Type::On;
    int    pitch   = 0;
    double time    = 0.0;
    float  cents   = 0.0f;
    float  levelDb = -100.0f;
    enum class Source : unsigned char { Single, Chord, Attack, Fingerprint };
    Source source  = Source::Single;
    enum class Rejected : unsigned char { No, Overtones, Unconfirmed, TooQuiet, NoStrike, NoKey, NotInSound, TooSoon };
    Rejected rejected = Rejected::No;
    int    otherPitch = -1;
    float  riseDb = 0.0f;
    float  attack = 0.0f;
    float  fitCost = -1.0f;
};

struct ListenerState
{
    float levelDb   = -100.0f;
    float frequency = 0.0f;
    float clarity   = 0.0f;
    int   pitch     = -1;
};

class NoteListener
{
public:
    void prepare (double sampleRate, const ListenerOptions& options = {});
    void reset();

    void process (const float* samples, int numSamples, std::vector<NoteEvent>& out);

    ListenerState state() const noexcept { return current; }
    const ListenerOptions& options() const noexcept { return opts; }
    double sampleRate() const noexcept { return rate; }

    double lastStrikeTime() const noexcept { return lastStrike; }
    double latencySeconds() const noexcept;

private:
    void analyse (std::vector<NoteEvent>& out);
    void emitOff (std::vector<NoteEvent>& out, double time);
    bool playedRecently (int pitch, double now) const noexcept;

    ListenerOptions  opts;
    YinPitchDetector yin;
    YinPitchDetector yinFast;
    bool useFast = false;
    double rate = 48000.0;

    std::vector<float> ring;
    std::vector<float> frame;
    int     writePos      = 0;
    int     filled        = 0;
    int     sinceLastHop  = 0;
    int64_t samplesSeen   = 0;

    std::vector<float> levelHistory;
    int     levelPos      = 0;
    int     levelWindow   = 512;
    int     recentFrames  = 2;

    int     candidate     = -1;
    int     candidateRun  = 0;
    int     runAfterOnset = 0;
    float   candidateCents = 0.0f;
    bool    onsetPending  = false;
    double  lastStrike    = -1.0e9;
    int64_t onsetSample   = 0;
    int     refractory    = 0;
    int     silentRun     = 0;

    struct RecentOn { int pitch; double time; };
    std::array<RecentOn, 8> recentOns {};
    int     recentPos     = 0;

    ListenerState current;
};

}
