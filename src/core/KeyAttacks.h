#pragma once

#include "core/KeyMatcher.h"
#include "core/NoteListener.h"
#include "core/Spectrum.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

namespace piano
{
class KeyAttacks
{
public:
    void prepare (double sampleRate, const ListenerOptions& options = {});
    void reset();

    void setExpected (const std::vector<int>& chord, const std::vector<int>& held);

    void process (const float* samples, int numSamples, std::vector<NoteEvent>& out,
                  std::vector<NoteEvent>* rejected = nullptr);

    void setFingerprints (std::shared_ptr<const KeyFingerprints> fingerprints);
    bool usesFingerprints() const noexcept { return matcher.fingerprints() != nullptr; }

    void setCapture (bool on) noexcept { captureWanted = on; }
    std::vector<CapturedStrike> takeCaptured();

    static int longWindowFor (double sampleRate) noexcept { return sampleRate > 60000.0 ? 8192 : 4096; }

    ListenerState state() const noexcept;
    double latencySeconds() const noexcept;

    double lastAttackTime() const noexcept { return lastAttack; }
    int attackCount() const noexcept { return attacks; }
    bool isSounding (int pitch) const noexcept;

private:
    struct Snapshot
    {
        double time = -1.0;
        std::vector<float> shortPower, longPower, fluxPower;
    };

    struct Key
    {
        bool  sounding = false;
        double onTime = -1.0;
        std::array<float, 16> history {};
        int   historyPos = 0;
        std::array<float, 3> raw { -200.0f, -200.0f, -200.0f };
        int   quietUpdates = 0;
        int   refractory = 0;
    };

    void hop (std::vector<NoteEvent>& out, std::vector<NoteEvent>* rejected);
    void decide (std::vector<NoteEvent>& out, std::vector<NoteEvent>* rejected, double until);
    bool decideByFingerprints (std::vector<NoteEvent>& out, std::vector<NoteEvent>* rejected, double until);
    void press (int pitch, std::vector<NoteEvent>& out, NoteEvent::Source source, float riseDb, float fitCost);
    void captureFrom (const Snapshot& snap);
    void trackReleases (std::vector<NoteEvent>& out);

    bool  usesLong (int pitch) const noexcept { return pitch < longBelow; }
    const Spectrum& fftFor (int pitch) const noexcept { return usesLong (pitch) ? longFft : shortFft; }
    float salience (const std::vector<Spectrum::Peak>& peaks, int pitch) const;
    float partialAmp (const std::vector<Spectrum::Peak>& peaks, int pitch, int k) const;
    float salience (const std::vector<float>& power, int pitch) const;
    float partialAmp (const std::vector<float>& power, int pitch, int k) const;
    float noiseSalience (int pitch, bool bands) const;
    int   partialCount (int pitch) const;
    bool  expected (int pitch) const noexcept;

    ListenerOptions opts;
    double rate = 48000.0;
    int hopSize = 128, longBelow = 84, lowPitch = 21, highPitch = 108;
    Spectrum fluxFft, shortFft, longFft;

    std::vector<float> ring, linear;
    int writePos = 0, filled = 0, sinceHop = 0;
    std::int64_t samplesSeen = 0, hops = 0;

    int fluxBin0 = 2, fluxBin1 = 200;
    std::vector<float> db, dbPrev1, dbPrev2;
    std::vector<float> fluxHistory;
    int fluxPos = 0;
    double lastAttack = -1.0e9;
    int attacks = 0;

    std::array<Snapshot, 16> snapshots {};
    int snapPos = 0;

    bool   pending = false;
    double attackStart = 0.0, decideAt = 0.0, decideAfter = 0.09;
    float  attackStrength = 0.0f, strikeLevelDb = -120.0f;
    Snapshot before;

    std::array<Key, 128> keys {};
    int lastPressed = -1;
    float levelDb = -120.0f;
    float noiseShort = 0.0f, noiseLong = 0.0f;

    std::array<std::atomic<std::uint64_t>, 2> chordBits {};

    KeyMatcher matcher;
    float loudness = 1.0f;
    std::mutex fingerprintLock;
    std::shared_ptr<const KeyFingerprints> givenFingerprints;
    std::atomic<bool> fingerprintsChanged { false };
    int fpBin0 = 0, fpBins = 0;
    std::vector<double> fpOffsets;

    std::atomic<bool> captureWanted { false };
    CapturedStrike capturing;
    std::vector<float> captureBefore;
    int captureNext = -1;
    std::vector<CapturedStrike> captured;
};

}
