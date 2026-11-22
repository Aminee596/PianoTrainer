#include "core/KeyAttacks.h"
#include "core/Score.h"

#include <algorithm>
#include <cmath>

namespace piano
{
namespace
{
    constexpr float  kInharmonicity  = 0.0004f;
    constexpr int    kSaliencePartials = 10;
    constexpr int    kCancelPartials = 20;
    constexpr double kTopHz          = 6000.0;

    constexpr float  kFluxMin        = 0.35f;
    constexpr float  kFluxOverMedian = 2.0f;
    constexpr float  kFloorBelowMax  = 50.0f;
    constexpr double kMinGap         = 0.03;
    constexpr double kChordJoin      = 0.04;
    constexpr double kBeforeGap      = 0.008;
    constexpr double kJustBeforeGap  = 0.003;

    constexpr float  kPresentDb      = 12.0f;
    constexpr float  kLastsDb        = 20.0f;
    constexpr float  kChordRangeDb   = 18.0f;
    constexpr float  kChordRangeExpectedDb = 30.0f;
    constexpr float  kStrongestRangeDb = 12.0f;
    constexpr float  kOwnDb          = -4.0f;
    constexpr float  kOwnExpectedDb  = -10.0f;
    constexpr float  kRiseDb         = 2.5f;
    constexpr float  kNewShareDb     = -6.0f;
    constexpr float  kRootShareDb    = -12.0f;
    constexpr float  kCrowdedRiseDb  = 6.0f;
    constexpr float  kNeighbourDb    = 3.0f;
    constexpr float  kToneRiseDb     = 4.5f;
    constexpr float  kToneNeighbourDb = 6.0f;
    constexpr float  kTonalDb        = 5.0f;
    constexpr double kRepressGap     = 0.08;
    constexpr float  kLowerKeyDb     = 12.0f;
    constexpr int    kMaxKeys        = 10;
    constexpr int    kWeakFundamentalBelow = 48;
    constexpr int    kFluxRiseFrom   = 55;

    constexpr float  kReleaseDropDb  = 14.0f;
    constexpr int    kDropLookback   = 12;
    constexpr int    kRefractoryUpdates = 12;
    constexpr double kFadeOutSeconds = 1.5;

    float toDb (float amplitude) { return 20.0f * std::log10 (amplitude + 1.0e-12f); }

    double partialHz (int pitch, int k)
    {
        return k * pitchToFrequency (pitch) * std::sqrt (1.0 + kInharmonicity * k * k);
    }

    float median (float* values, int count)
    {
        if (count <= 0) return -100.0f;
        std::sort (values, values + count);
        return count % 2 == 1 ? values[count / 2] : 0.5f * (values[count / 2 - 1] + values[count / 2]);
    }
}

void KeyAttacks::prepare (double sampleRate, const ListenerOptions& options)
{
    rate = sampleRate;
    opts = options;
    const int scale = sampleRate > 60000.0 ? 2 : 1;
    hopSize = std::max (32, (int) std::lround (128.0 * sampleRate / 48000.0));
    fluxFft.prepare (1024 * scale, rate);
    shortFft.prepare (2048 * scale, rate);
    longFft.prepare (4096 * scale, rate);
    decideAfter = longFft.size / rate + 0.004;
    ring.assign ((std::size_t) longFft.size, 0.0f);
    linear.assign ((std::size_t) longFft.size, 0.0f);

    lowPitch  = std::clamp ((int) std::floor (frequencyToPitch (std::max (20.0f, opts.minFrequency))), 21, 108);
    highPitch = std::clamp ((int) std::ceil (frequencyToPitch (std::max (40.0f, opts.maxFrequency))), lowPitch, 108);

    fluxBin0 = std::max (1, (int) (100.0 * fluxFft.size / rate));
    fluxBin1 = std::min (fluxFft.size / 2 - 1, (int) (10000.0 * fluxFft.size / rate));
    db.assign ((std::size_t) fluxFft.size / 2 + 1, -200.0f);

    fpBin0 = (int) std::ceil (KeyFingerprints::kLowHz * longFft.size / rate);
    fpBins = std::max (1, (int) std::floor (KeyFingerprints::kHighHz * longFft.size / rate) - fpBin0 + 1);
    fpOffsets.clear();
    for (double o = 0.02; o < decideAfter - 0.005; o += 0.01) fpOffsets.push_back (std::round (o * 1000.0) / 1000.0);
    fpOffsets.push_back (decideAfter);
    fingerprintsChanged = true;
    reset();
}

void KeyAttacks::setFingerprints (std::shared_ptr<const KeyFingerprints> f)
{
    {
        const std::lock_guard<std::mutex> lock (fingerprintLock);
        givenFingerprints = std::move (f);
    }
    fingerprintsChanged = true;
}

std::vector<CapturedStrike> KeyAttacks::takeCaptured()
{
    std::vector<CapturedStrike> taken;
    taken.swap (captured);
    return taken;
}

void KeyAttacks::reset()
{
    std::fill (ring.begin(), ring.end(), 0.0f);
    writePos = filled = sinceHop = 0;
    samplesSeen = hops = 0;
    std::fill (db.begin(), db.end(), -200.0f);
    dbPrev1 = dbPrev2 = db;
    fluxHistory.assign (48, 0.0f);
    fluxPos = 0;
    lastAttack = -1.0e9;
    attacks = 0;
    for (auto& s : snapshots) s = Snapshot {};
    snapPos = 0;
    pending = false;
    for (auto& k : keys) { k = Key {}; k.history.fill (-200.0f); }
    levelDb = -120.0f;
    captureNext = -1;
    captured.clear();
}

void KeyAttacks::setExpected (const std::vector<int>& chord, const std::vector<int>&)
{
    std::uint64_t lo = 0, hi = 0;
    for (int p : chord)
        if (p >= 0 && p < 128) (p < 64 ? lo : hi) |= std::uint64_t (1) << (p & 63);
    chordBits[0].store (lo, std::memory_order_relaxed);
    chordBits[1].store (hi, std::memory_order_relaxed);
}

bool KeyAttacks::expected (int p) const noexcept
{
    if (p < 0 || p > 127) return false;
    return ((chordBits[p < 64 ? 0 : 1].load (std::memory_order_relaxed) >> (p & 63)) & 1u) != 0;
}

ListenerState KeyAttacks::state() const noexcept
{
    ListenerState s;
    s.levelDb = levelDb;
    if (lastPressed >= 0 && keys[(std::size_t) lastPressed].sounding)
    {
        s.pitch = lastPressed;
        s.frequency = (float) pitchToFrequency (lastPressed);
        s.clarity = 1.0f;
    }
    return s;
}

double KeyAttacks::latencySeconds() const noexcept
{
    return decideAfter + 2.0 * hopSize / rate;
}

bool KeyAttacks::isSounding (int p) const noexcept
{
    return p >= 0 && p < 128 && keys[(std::size_t) p].sounding;
}

int KeyAttacks::partialCount (int pitch) const
{
    int k = 1;
    while (k < kSaliencePartials && partialHz (pitch, k + 1) < std::min (kTopHz, rate * 0.45)) ++k;
    return k;
}

float KeyAttacks::partialAmp (const std::vector<float>& power, int pitch, int k) const
{
    return fftFor (pitch).bandAmplitude (power, partialHz (pitch, k));
}

float KeyAttacks::salience (const std::vector<float>& power, int pitch) const
{
    float sum = 0.0f;
    const int count = partialCount (pitch);
    for (int k = 1; k <= count; ++k)
        sum += partialAmp (power, pitch, k) / std::sqrt ((float) k);
    return sum;
}

float KeyAttacks::partialAmp (const std::vector<Spectrum::Peak>& peaks, int pitch, int k) const
{
    return fftFor (pitch).amplitudeAt (peaks, partialHz (pitch, k));
}

float KeyAttacks::salience (const std::vector<Spectrum::Peak>& peaks, int pitch) const
{
    float sum = 0.0f;
    const int count = partialCount (pitch);
    for (int k = 1; k <= count; ++k)
        sum += partialAmp (peaks, pitch, k) / std::sqrt ((float) k);
    return sum;
}

float KeyAttacks::noiseSalience (int pitch, bool bands) const
{
    const Spectrum& f = fftFor (pitch);
    const float bin = usesLong (pitch) ? noiseLong : noiseShort;
    float sum = 0.0f;
    for (int k = 1; k <= partialCount (pitch); ++k)
    {
        int b0 = 0, b1 = 0;
        f.bandBins (partialHz (pitch, k), b0, b1);
        sum += bin * (bands ? std::sqrt ((float) (b1 - b0 + 1)) : 1.0f) / std::sqrt ((float) k);
    }
    return toDb (sum);
}

void KeyAttacks::process (const float* samples, int numSamples, std::vector<NoteEvent>& out,
                          std::vector<NoteEvent>* rejected)
{
    const int n = (int) ring.size();
    if (n == 0) return;

    for (int i = 0; i < numSamples; ++i)
    {
        ring[(std::size_t) writePos] = samples[i];
        writePos = (writePos + 1) % n;
        filled = std::min (filled + 1, n);
        ++samplesSeen;
        if (++sinceHop < hopSize) continue;
        sinceHop = 0;
        if (filled >= fluxFft.size)
            hop (out, rejected);
    }
}

void KeyAttacks::hop (std::vector<NoteEvent>& out, std::vector<NoteEvent>* rejected)
{
    const int n = (int) ring.size();
    const double now = (double) samplesSeen / rate;
    const double hopSec = hopSize / rate;
    ++hops;

    if (fingerprintsChanged.exchange (false))
    {
        std::shared_ptr<const KeyFingerprints> f;
        {
            const std::lock_guard<std::mutex> lock (fingerprintLock);
            f = givenFingerprints;
        }
        const bool fits = f != nullptr && f->suits (rate, longFft.size) && f->bin0 == fpBin0 && f->bins == fpBins;
        if (f != nullptr && ! fits)
            f = f->convertedTo (rate, longFft.size, fpBin0, fpBins);
        if (f != nullptr && ! f->complete())
            f = nullptr;
        matcher.setFingerprints (f);
    }

    std::copy (ring.begin() + writePos, ring.end(), linear.begin());
    std::copy (ring.begin(), ring.begin() + writePos, linear.begin() + (n - writePos));

    double sumSq = 0.0;
    for (int j = n - fluxFft.size; j < n; ++j) sumSq += (double) linear[(std::size_t) j] * linear[(std::size_t) j];
    levelDb = (float) (10.0 * std::log10 (sumSq / fluxFft.size + 1.0e-12));

    fluxFft.run (linear.data(), n);
    float maxDb = -200.0f;
    for (int b = fluxBin0; b <= fluxBin1; ++b)
    {
        db[(std::size_t) b] = 10.0f * std::log10 (fluxFft.power[(std::size_t) b] + 1.0e-20f);
        maxDb = std::max (maxDb, db[(std::size_t) b]);
    }
    std::vector<float> band (db.begin() + fluxBin0, db.begin() + fluxBin1 + 1);
    std::nth_element (band.begin(), band.begin() + (long) band.size() / 2, band.end());
    const float floorDb = std::max (maxDb - kFloorBelowMax, band[band.size() / 2] + 10.0f);

    float flux = 0.0f;
    for (int b = fluxBin0; b <= fluxBin1; ++b)
    {
        const float v = db[(std::size_t) b];
        if (v <= floorDb) continue;
        const float prior = std::max ({ floorDb, dbPrev2[(std::size_t) b - 1], dbPrev2[(std::size_t) b], dbPrev2[(std::size_t) b + 1] });
        flux += std::max (0.0f, v - prior);
    }
    flux /= (float) (fluxBin1 - fluxBin0 + 1);
    dbPrev2.swap (dbPrev1);
    dbPrev1 = db;

    const int hsize = (int) fluxHistory.size();
    fluxHistory[(std::size_t) fluxPos] = flux;
    fluxPos = (fluxPos + 1) % hsize;
    auto fluxAt = [&] (int age) { return fluxHistory[(std::size_t) ((fluxPos - 1 - age + 2 * hsize) % hsize)]; };

    if (hops > 6)
    {
        const float c = fluxAt (2);
        std::vector<float> older;
        for (int age = 6; age < hsize - 1; ++age) older.push_back (fluxAt (age));
        std::nth_element (older.begin(), older.begin() + (long) older.size() / 2, older.end());
        const float threshold = std::max (kFluxMin, kFluxOverMedian * older[older.size() / 2]);
        const double t0 = now - 2 * hopSec;
        const bool peak = c >= fluxAt (3) && c >= fluxAt (4) && c > fluxAt (1) && c > fluxAt (0);
        if (peak && c >= threshold && levelDb > opts.gateDb && t0 - lastAttack >= kMinGap)
        {
            lastAttack = t0;
            if (pending && t0 - attackStart >= kChordJoin)
            {
                pending = false;
                decide (out, rejected, t0);
            }
            if (! pending)
            {
                auto latestBefore = [&] (double gap) -> const Snapshot*
                {
                    const Snapshot* best = nullptr;
                    for (const auto& s : snapshots)
                        if (s.time >= 0.0 && s.time <= t0 - gap && (best == nullptr || s.time > best->time))
                            best = &s;
                    return best;
                };
                const auto* b = latestBefore (kBeforeGap);
                before = b != nullptr ? *b : Snapshot {};
                const auto* jb = latestBefore (kJustBeforeGap);
                before.fluxPower = jb != nullptr ? jb->fluxPower : std::vector<float> {};
                pending = true;
                attackStart = t0;
                attackStrength = c;
                strikeLevelDb = levelDb;
                decideAt = t0 + decideAfter;
                if (captureWanted.load())
                {
                    capturing = CapturedStrike {};
                    capturing.time = t0;
                    capturing.strength = c;
                    capturing.rate = rate;
                    capturing.fftSize = longFft.size;
                    capturing.bin0 = fpBin0;
                    capturing.bins = fpBins;
                    capturing.offsets = fpOffsets;
                    captureBefore = before.longPower;
                    captureNext = 0;
                }
            }
            else
            {
                decideAt = std::max (decideAt, t0 + decideAfter);
                attackStrength = std::max (attackStrength, c);
                strikeLevelDb = std::max (strikeLevelDb, levelDb);
            }
        }
    }

    if (hops % 2 != 0 || filled < n)
        return;

    shortFft.run (linear.data(), n);
    longFft.run (linear.data(), n);
    noiseShort = shortFft.medianAmplitude (60.0, 5000.0);
    noiseLong  = longFft.medianAmplitude (60.0, 5000.0);

    auto& snap = snapshots[(std::size_t) snapPos];
    snapPos = (snapPos + 1) % (int) snapshots.size();
    snap.time = now;
    snap.shortPower = shortFft.power;
    snap.longPower = longFft.power;
    snap.fluxPower = fluxFft.power;

    if (! captureWanted.load())
        captureNext = -1;
    if (captureNext >= 0)
        captureFrom (snap);

    if (pending && now >= decideAt)
    {
        pending = false;
        decide (out, rejected, now);
    }
    trackReleases (out);
}

void KeyAttacks::captureFrom (const Snapshot& snap)
{
    while (captureNext >= 0 && captureNext < (int) capturing.offsets.size()
           && snap.time - capturing.time >= capturing.offsets[(std::size_t) captureNext] - 1.0e-9)
    {
        std::vector<float> sound ((std::size_t) capturing.bins, 0.0f);
        for (int b = 0; b < capturing.bins; ++b)
        {
            const auto i = (std::size_t) (capturing.bin0 + b);
            const float was = i < captureBefore.size() ? captureBefore[i] : 0.0f;
            sound[(std::size_t) b] = i < snap.longPower.size() ? std::sqrt (std::max (0.0f, snap.longPower[i] - was)) : 0.0f;
        }
        capturing.spectra.push_back (std::move (sound));
        if (++captureNext == (int) capturing.offsets.size())
        {
            captured.push_back (std::move (capturing));
            capturing = CapturedStrike {};
            captureNext = -1;
        }
    }
}

void KeyAttacks::press (int p, std::vector<NoteEvent>& out, NoteEvent::Source source, float riseDb, float fitCost)
{
    auto& k = keys[(std::size_t) p];
    const double t = attackStart;
    if (k.sounding)
        out.push_back ({ NoteEvent::Type::Off, p, t, 0.0f, levelDb });
    k.sounding = true;
    k.onTime = t;
    k.refractory = kRefractoryUpdates;
    k.quietUpdates = 0;
    k.history.fill (-200.0f);
    k.raw = { -200.0f, -200.0f, -200.0f };
    lastPressed = p;
    NoteEvent e { NoteEvent::Type::On, p, t, 0.0f, levelDb };
    e.source = source;
    e.riseDb = riseDb;
    e.attack = attackStrength;
    e.fitCost = fitCost;
    out.push_back (e);
}

bool KeyAttacks::decideByFingerprints (std::vector<NoteEvent>& out, std::vector<NoteEvent>* rejected, double until)
{
    const auto* f = matcher.fingerprints();
    if (f == nullptr)
        return false;

    const Snapshot* snap = nullptr;
    for (const auto& s : snapshots)
        if (s.time > attackStart && s.time <= until + 1.0e-9 && (snap == nullptr || s.time > snap->time))
            snap = &s;
    if (snap == nullptr || snap->longPower.size() < (std::size_t) (f->bin0 + f->bins))
        return false;

    const int o = f->nearestOffset (snap->time - attackStart);
    std::vector<float> sound ((std::size_t) f->bins, 0.0f);
    for (int b = 0; b < f->bins; ++b)
    {
        const auto i = (std::size_t) (f->bin0 + b);
        const float was = i < before.longPower.size() ? before.longPower[i] : 0.0f;
        sound[(std::size_t) b] = std::sqrt (std::max (0.0f, snap->longPower[i] - was));
    }
    const auto found = matcher.match (sound, o, loudness);

    const bool loudEnough = levelDb > opts.gateDb && levelDb > strikeLevelDb - kLastsDb;
    std::vector<int> picks;
    for (std::size_t i = 0; i < found.keys.size(); ++i)
    {
        const int p = found.keys[i];
        if (p < 0 || p > 127) continue;
        const auto& k = keys[(std::size_t) p];
        const bool tooSoon = k.onTime >= 0.0 && attackStart - k.onTime < kRepressGap;
        if (loudEnough && ! tooSoon)
            picks.push_back (p);
    }

    if (found.keys.size() == 1 && found.cost < 0.15f && picks.size() == 1 && ! keys[(std::size_t) picks[0]].sounding)
    {
        const float g = matcher.loudness (sound, picks[0], o);
        if (g > 0.0f)
            loudness = std::clamp (0.9f * loudness + 0.1f * g, 0.2f, 5.0f);
    }

    for (std::size_t i = 0; i < found.keys.size(); ++i)
    {
        const int p = found.keys[i];
        if (std::find (picks.begin(), picks.end(), p) != picks.end())
            press (p, out, NoteEvent::Source::Fingerprint, 20.0f * std::log10 (std::max (found.gains[i], 1.0e-3f)), found.cost);
    }

    if (rejected == nullptr)
        return true;

    for (int p = lowPitch; p <= highPitch; ++p)
    {
        if (! expected (p) || std::find (picks.begin(), picks.end(), p) != picks.end()) continue;
        NoteEvent r { NoteEvent::Type::On, p, attackStart, 0.0f, levelDb };
        r.source = NoteEvent::Source::Fingerprint;
        r.attack = attackStrength;
        r.fitCost = found.cost;
        const bool inSound = std::find (found.keys.begin(), found.keys.end(), p) != found.keys.end();
        r.rejected = ! loudEnough ? NoteEvent::Rejected::TooQuiet : inSound ? NoteEvent::Rejected::TooSoon : NoteEvent::Rejected::NotInSound;
        rejected->push_back (r);
    }
    if (picks.empty() && found.keys.empty())
    {
        NoteEvent r { NoteEvent::Type::On, std::max (0, found.closest), attackStart, 0.0f, levelDb };
        r.source = NoteEvent::Source::Fingerprint;
        r.attack = attackStrength;
        r.fitCost = found.cost;
        r.rejected = loudEnough ? NoteEvent::Rejected::NoKey : NoteEvent::Rejected::TooQuiet;
        rejected->push_back (r);
    }
    return true;
}

void KeyAttacks::decide (std::vector<NoteEvent>& out, std::vector<NoteEvent>* rejected, double until)
{
    ++attacks;
    if (decideByFingerprints (out, rejected, until))
        return;
    constexpr double kBand = 1.5;
    constexpr double kGate = 1.0;

    auto newEnergy = [] (const Spectrum& f, const std::vector<float>& was)
    {
        std::vector<float> fresh (f.power.size(), 0.0f);
        for (std::size_t b = 0; b < fresh.size(); ++b)
            fresh[b] = std::max (0.0f, f.power[b] - (b < was.size() ? was[b] : 0.0f));
        return fresh;
    };
    auto aboveFloor = [] (std::vector<float> fresh)
    {
        const int n = (int) fresh.size();
        std::vector<float> kept (fresh.size(), 0.0f), window;
        for (int b = 0; b < n; ++b)
        {
            window.clear();
            for (int j = std::max (0, b - 12); j <= std::min (n - 1, b + 12); ++j) window.push_back (fresh[(std::size_t) j]);
            std::nth_element (window.begin(), window.begin() + (long) window.size() / 2, window.end());
            kept[(std::size_t) b] = std::max (0.0f, fresh[(std::size_t) b] - window[window.size() / 2]);
        }
        return kept;
    };
    std::vector<float> residual[2] = { aboveFloor (newEnergy (shortFft, before.shortPower)), aboveFloor (newEnergy (longFft, before.longPower)) };
    const std::vector<float> newPower[2] = { residual[0], residual[1] };
    auto which = [&] (int p) { return usesLong (p) ? 1 : 0; };

    auto gated = [&] (int p, int k)
    {
        const Spectrum& f = fftFor (p);
        return f.amplitudeNear (f.peaks, partialHz (p, k), kGate) > 0.0f;
    };
    auto amp = [&] (const std::vector<float>& power, int p, int k)
    {
        return gated (p, k) ? fftFor (p).bandAmplitude (power, partialHz (p, k), kBand) : 0.0f;
    };
    auto meter = [&] (const std::vector<float>& power, int p)
    {
        float sum = 0.0f;
        for (int k = 1; k <= partialCount (p); ++k) sum += amp (power, p, k) / std::sqrt ((float) k);
        return sum;
    };
    auto residualDb = [&] (int p) { return toDb (meter (residual[which (p)], p)); };

    std::array<float, 128> fresh {}, rise {}, noise {}, share {}, rootShare {};
    float strongestNew = -1000.0f;
    int strongestKey = -1;
    for (int p = lowPitch; p <= highPitch; ++p)
    {
        fresh[(std::size_t) p] = residualDb (p);
        float w = 0.0f;
        for (int k = 1; k <= partialCount (p); ++k)
        {
            int b0 = 0, b1 = 0;
            fftFor (p).bandBins (partialHz (p, k), b0, b1, kBand);
            w += std::sqrt ((float) (b1 - b0 + 1)) / std::sqrt ((float) k);
        }
        noise[(std::size_t) p] = toDb ((usesLong (p) ? noiseLong : noiseShort) * w);
        if (fresh[(std::size_t) p] > strongestNew) { strongestNew = fresh[(std::size_t) p]; strongestKey = p; }
    }

    struct View { float rise = -100.0f, share = -100.0f, root = -100.0f; };
    auto look = [&] (int p, const Spectrum& g, const std::vector<float>& was, const std::vector<float>& newP, double band)
    {
        View v;
        const float floorAmp = g.medianAmplitude (60.0, 5000.0) * 3.0f;
        std::array<float, kSaliencePartials> rises {};
        float sa = 0.0f, sb = 0.0f, sn = 0.0f;
        int counted = 0;
        for (int k = 1; k <= partialCount (p); ++k)
        {
            const double hz = partialHz (p, k);
            if (! gated (p, k)) continue;
            const float x = g.bandAmplitude (hz, band), y = g.bandAmplitude (was, hz, band), z = g.bandAmplitude (newP, hz, band);
            sa += x / std::sqrt ((float) k);
            sb += y / std::sqrt ((float) k);
            sn += z / std::sqrt ((float) k);
            if (x > floorAmp)
                rises[(std::size_t) counted++] = std::clamp (toDb (x) - toDb (std::max (y, floorAmp)), -20.0f, 40.0f);
            if ((k == 1 || (k == 2 && p < kWeakFundamentalBelow)) && x > 0.0f)
                v.root = std::max (v.root, toDb (z) - toDb (x));
        }
        v.rise = std::max (toDb (sa) - toDb (std::max (sb, 1.0e-9f)), median (rises.data(), counted));
        v.share = toDb (sn) - toDb (sa);
        return v;
    };
    auto newIn = [&] (const Spectrum& g, const std::vector<float>& was)
    {
        std::vector<float> n (g.power.size(), 0.0f);
        for (std::size_t b = 0; b < n.size(); ++b) n[b] = std::max (0.0f, g.power[b] - (b < was.size() ? was[b] : 0.0f));
        return aboveFloor (n);
    };
    const auto newShort = newPower[0];
    const auto newFlux = before.fluxPower.empty() ? std::vector<float> {} : newIn (fluxFft, before.fluxPower);
    const double shortBinHz = rate / shortFft.size;

    for (int p = lowPitch; p <= highPitch; ++p)
    {
        if (! (fresh[(std::size_t) p] > -200.0f)) continue;
        bool crowded = false;
        for (int d : { -2, -1, 1, 2 })
        {
            const int n = p + d;
            if (n >= lowPitch && n <= highPitch && fresh[(std::size_t) n] > fresh[(std::size_t) p] + 3.0f
                && std::abs (pitchToFrequency (n) - pitchToFrequency (p)) < 2.0 * shortBinHz
                && longFft.amplitudeAt (partialHz (n, 1)) > 0.0f)
                crowded = true;
        }
        View v = crowded ? look (p, longFft, before.longPower, newPower[1], kBand)
                         : look (p, shortFft, before.shortPower, newShort, 0.5);
        if (! crowded && ! newFlux.empty() && p >= kFluxRiseFrom && fresh[(std::size_t) p] >= strongestNew - 3.0f)
        {
            const View f = look (p, fluxFft, before.fluxPower, newFlux, 0.6);
            v.rise = std::max (v.rise, f.rise);
            v.share = std::max (v.share, f.share);
            v.root = std::max (v.root, f.root);
        }
        rise[(std::size_t) p] = v.rise;
        share[(std::size_t) p] = v.share;
        rootShare[(std::size_t) p] = v.root;
    }

    std::vector<std::pair<int, double>> expectedPartials;
    for (int e = lowPitch; e <= highPitch; ++e)
        if (expected (e))
            for (int k = 1; k <= kCancelPartials && partialHz (e, k) < 8000.0; ++k)
                expectedPartials.push_back ({ e, partialHz (e, k) });
    auto sharedWithExpected = [&] (int p, double hz)
    {
        for (const auto& [e, f] : expectedPartials)
            if (e != p && std::abs (1200.0 * std::log2 (f / hz)) < 35.0)
                return true;
        return false;
    };

    auto cancel = [&] (int p, bool fully)
    {
        const bool inChord = expected (p) && ! fully;
        for (int w = 0; w < 2; ++w)
        {
            auto& list = residual[w];
            const Spectrum& f = w == 1 ? longFft : shortFft;
            std::array<float, kCancelPartials + 2> a {};
            std::array<bool, kCancelPartials + 2> shared {};
            int count = 0;
            for (int k = 1; k <= kCancelPartials && partialHz (p, k) < std::min (8000.0, rate * 0.45); ++k)
            {
                a[(std::size_t) k] = f.bandAmplitude (list, partialHz (p, k), kBand);
                shared[(std::size_t) k] = k >= 2 && inChord && sharedWithExpected (p, partialHz (p, k));
                count = k;
            }
            for (int k = 1; k <= count; ++k)
            {
                const float ak = a[(std::size_t) k];
                if (ak <= 0.0f) continue;
                float est = ak;
                if (shared[(std::size_t) k])
                {
                    int lo = k - 1, hi = k + 1;
                    while (lo >= 1 && shared[(std::size_t) lo]) --lo;
                    while (hi <= count && shared[(std::size_t) hi]) ++hi;
                    float g;
                    if (lo >= 1 && hi <= count)
                    {
                        const float t = (float) (k - lo) / (float) (hi - lo);
                        g = std::pow (a[(std::size_t) lo] + 1.0e-12f, 1.0f - t) * std::pow (a[(std::size_t) hi] + 1.0e-12f, t);
                    }
                    else if (lo >= 1) g = a[(std::size_t) lo] * (float) lo / (float) k;
                    else if (hi <= count) g = a[(std::size_t) hi] * (float) hi / (float) k;
                    else g = 0.5f * ak;
                    if (p < kWeakFundamentalBelow && lo == 1 && hi <= count && a[1] < 0.3f * a[(std::size_t) hi])
                        g = std::max (g, a[(std::size_t) hi] * (float) hi / (float) k);
                    est = std::min (est, g);
                }
                const float keep = std::max (0.0f, 1.0f - (est * est) / (ak * ak));
                int b0 = 0, b1 = 0;
                f.bandBins (partialHz (p, k), b0, b1, kBand);
                for (int b = b0; b <= std::min (b1, (int) list.size() - 1); ++b) list[(std::size_t) b] *= keep;
            }
        }
    };

    auto hasLowPartial = [&] (int q)
    {
        const Spectrum& f = fftFor (q);
        float strongest = 0.0f, any = 0.0f;
        for (int k = 1; k <= partialCount (q); ++k)
        {
            const float a = f.amplitudeNear (f.peaks, partialHz (q, k), kGate);
            any = std::max (any, a);
            if (k == 1 || ! sharedWithExpected (q, partialHz (q, k))) strongest = std::max (strongest, a);
        }
        if (strongest <= 0.0f) strongest = any;
        const float a1 = f.amplitudeNear (f.peaks, partialHz (q, 1), kGate);
        const float lowest = q >= kWeakFundamentalBelow ? a1 : std::max (a1, f.amplitudeNear (f.peaks, partialHz (q, 2), kGate));
        return strongest > 0.0f && lowest >= 0.1f * strongest;
    };

    auto tonal = [&] (int q)
    {
        if (q < kWeakFundamentalBelow) return true;
        const auto& pw = longFft.power;
        const double b = partialHz (q, 1) * longFft.size / rate;
        const int c = (int) std::lround (b);
        const int lo = std::max (1, std::min (c - 4, (int) (b / std::pow (2.0, 0.25))));
        const int hi = std::min ((int) pw.size() - 1, std::max (c + 4, (int) (b * std::pow (2.0, 0.25))));
        if (c < 2 || c + 1 >= (int) pw.size()) return false;
        const float peak = std::max ({ pw[(std::size_t) c - 1], pw[(std::size_t) c], pw[(std::size_t) c + 1] });
        std::vector<float> around (pw.begin() + lo, pw.begin() + hi + 1);
        std::nth_element (around.begin(), around.begin() + (long) around.size() / 2, around.end());
        const float med = around[around.size() / 2];
        return 10.0f * std::log10 ((peak + 1.0e-20f) / (med + 1.0e-20f)) >= kTonalDb;
    };

    auto ownEvidence = [&] (int q, int ratio, float againstDb)
    {
        const auto& power = residual[which (q)];
        float sum = 0.0f, strongest = 0.0f;
        for (int k = 1; k <= partialCount (q); ++k)
            if (k % ratio != 0)
            {
                const float a = amp (power, q, k);
                sum += a / std::sqrt ((float) k);
                strongest = std::max (strongest, a);
            }
        if (strongest <= 0.0f || toDb (sum) < againstDb - kLowerKeyDb) return false;
        if (amp (power, q, 1) >= 0.1f * strongest) return true;
        if (q >= kWeakFundamentalBelow)
        {
            if (! hasLowPartial (q)) return false;
            int renewed = 0;
            for (int k = 2; k <= partialCount (q); ++k)
                if (k % ratio != 0 && amp (power, q, k) >= 0.1f * strongest) ++renewed;
            return renewed >= 2;
        }
        for (int k : { 3, 5, 7, 9 })
            if (k % ratio != 0 && k <= partialCount (q) && amp (power, q, k) < 0.1f * strongest)
                return false;
        return true;
    };

    std::vector<int> picks;
    std::array<bool, 128> considered {};
    bool chordCleared = false;
    float first = -1000.0f;
    const bool loudEnough = levelDb > opts.gateDb && levelDb > strikeLevelDb - kLastsDb;

    for (int round = 0; round < 3 * kMaxKeys && (int) picks.size() < kMaxKeys && loudEnough; ++round)
    {
        int best = -1;
        float bestScore = 0.0f;
        for (int pass = 0; pass < 2 && best < 0; ++pass)
            for (int p = lowPitch; p <= highPitch; ++p)
            {
                if (considered[(std::size_t) p] || (pass == 0 && ! expected (p))) continue;
                const auto& power = residual[which (p)];
                const float m = meter (power, p);
                if (toDb (m) - noise[(std::size_t) p] < kPresentDb) continue;
                const float score = p < kWeakFundamentalBelow ? m : amp (power, p, 1) + 0.5f * amp (power, p, 2) + 0.5f * m;
                if (score > bestScore) { best = p; bestScore = score; }
            }
        if (best < 0)
            break;
        float bestDb = residualDb (best);

        for (int ratio = 8; ratio >= 2; --ratio)
        {
            const int q = (int) std::lround (best - 12.0 * std::log2 ((double) ratio));
            if (q < lowPitch || considered[(std::size_t) q]) continue;
            if (residualDb (q) >= bestDb - kLowerKeyDb && hasLowPartial (q) && tonal (q) && ownEvidence (q, ratio, bestDb))
            {
                best = q;
                bestDb = residualDb (q);
                break;
            }
        }
        considered[(std::size_t) best] = true;

        const bool exp = expected (best);
        if (! exp && ! chordCleared)
        {
            chordCleared = true;
            for (int q : picks) cancel (q, true);
            bestDb = residualDb (best);
        }
        const float s = bestDb;
        if (picks.empty()) first = s;
        const bool present = s - noise[(std::size_t) best] >= kPresentDb;
        const bool inRange = (picks.empty() || s >= first - (exp ? kChordRangeExpectedDb : kChordRangeDb))
                          && fresh[(std::size_t) best] >= strongestNew - (exp && expected (strongestKey) ? kChordRangeDb : kStrongestRangeDb);

        bool onPartial = false;
        for (int q : picks)
            for (int k = 2; k <= kCancelPartials && ! onPartial; ++k)
                onPartial = std::abs (1200.0 * std::log2 (partialHz (q, k) / pitchToFrequency (best))) < 35.0;
        const bool own = ! onPartial || s - fresh[(std::size_t) best] >= (exp ? kOwnExpectedDb : kOwnDb);

        const bool jumped = rise[(std::size_t) best] >= kRiseDb && share[(std::size_t) best] >= kNewShareDb
                         && rootShare[(std::size_t) best] >= kRootShareDb;
        bool low = hasLowPartial (best) && (exp || tonal (best));
        const bool tooSoon = keys[(std::size_t) best].onTime >= 0.0 && attackStart - keys[(std::size_t) best].onTime < kRepressGap;

        int louderNeighbour = -1;
        for (int d : { -2, -1, 1, 2 })
        {
            const int n = best + d;
            if (n >= lowPitch && n <= highPitch && fresh[(std::size_t) n] > fresh[(std::size_t) best]
                && (louderNeighbour < 0 || fresh[(std::size_t) n] > fresh[(std::size_t) louderNeighbour]))
                louderNeighbour = n;
        }
        if (louderNeighbour >= 0 && low)
        {
            const float own1 = longFft.amplitudeAt (partialHz (best, 1));
            const float own2 = best < kWeakFundamentalBelow ? longFft.amplitudeAt (partialHz (best, 2)) : 0.0f;
            const bool semitone = std::abs (louderNeighbour - best) == 1;
            if ((own1 <= 0.0f && own2 <= 0.0f) || rise[(std::size_t) best] < (semitone ? kCrowdedRiseDb : kToneRiseDb)
                || fresh[(std::size_t) best] < fresh[(std::size_t) louderNeighbour] - (semitone ? kNeighbourDb : kToneNeighbourDb))
                low = false;
        }

        if (present && inRange && own && jumped && low && ! tooSoon)
        {
            picks.push_back (best);
            cancel (best, false);
        }
        else
        {
            if ((! jumped || tooSoon) && own && present)
                cancel (best, tooSoon);
            if (picks.empty())
                first = -1000.0f;
        }
        if (! picks.empty() && s < first - kChordRangeExpectedDb)
            break;
    }

    if (picks.size() > 1)
    {
        for (auto it = picks.begin(); it != picks.end();)
        {
            const int p = *it;
            bool overtone = false;
            for (int q : picks)
                for (int k = 2; k <= kCancelPartials && q < p && ! overtone; ++k)
                    overtone = std::abs (1200.0 * std::log2 (partialHz (q, k) / pitchToFrequency (p))) < 35.0;
            if (expected (p) || ! overtone) { ++it; continue; }

            residual[0] = newPower[0];
            residual[1] = newPower[1];
            for (int q : picks)
                if (q != p) cancel (q, true);
            if (residualDb (p) - fresh[(std::size_t) p] < kOwnDb)
                it = picks.erase (it);
            else
                ++it;
        }
    }

    std::sort (picks.begin(), picks.end());
    const double t = attackStart;
    for (int p : picks)
    {
        auto& k = keys[(std::size_t) p];
        if (k.sounding)
            out.push_back ({ NoteEvent::Type::Off, p, t, 0.0f, levelDb });
        k.sounding = true;
        k.onTime = t;
        k.refractory = kRefractoryUpdates;
        k.quietUpdates = 0;
        k.history.fill (-200.0f);
        k.raw = { -200.0f, -200.0f, -200.0f };
        lastPressed = p;
        NoteEvent e { NoteEvent::Type::On, p, t, 0.0f, levelDb };
        e.source = NoteEvent::Source::Attack;
        e.riseDb = rise[(std::size_t) p];
        e.attack = attackStrength;
        out.push_back (e);
    }

    if (rejected == nullptr)
        return;

    for (int p = lowPitch; p <= highPitch; ++p)
    {
        if (! expected (p) || std::find (picks.begin(), picks.end(), p) != picks.end()) continue;
        if (fresh[(std::size_t) p] - noise[(std::size_t) p] < kPresentDb) continue;
        NoteEvent r { NoteEvent::Type::On, p, t, 0.0f, levelDb };
        r.source = NoteEvent::Source::Attack;
        r.riseDb = rise[(std::size_t) p];
        r.attack = attackStrength;
        if (! loudEnough)
            r.rejected = NoteEvent::Rejected::TooQuiet;
        else if (! picks.empty() && residualDb (p) - fresh[(std::size_t) p] < kOwnExpectedDb)
        {
            r.rejected = NoteEvent::Rejected::Overtones;
            r.otherPitch = picks.front();
            for (int q : picks)
                for (int interval : { 12, 19, 24, 28, 31, 34, 36 })
                    if (p - q == interval) r.otherPitch = q;
        }
        else if (rise[(std::size_t) p] < kRiseDb || share[(std::size_t) p] < kNewShareDb || rootShare[(std::size_t) p] < kRootShareDb)
            r.rejected = NoteEvent::Rejected::NoStrike;
        else
            r.rejected = NoteEvent::Rejected::TooQuiet;
        rejected->push_back (r);
    }
    if (picks.empty())
    {
        int loudest = -1;
        for (int p = lowPitch; p <= highPitch; ++p)
            if (loudest < 0 || fresh[(std::size_t) p] > fresh[(std::size_t) loudest]) loudest = p;
        if (loudest >= 0 && ! expected (loudest))
        {
            NoteEvent r { NoteEvent::Type::On, loudest, t, 0.0f, levelDb };
            r.source = NoteEvent::Source::Attack;
            r.riseDb = rise[(std::size_t) loudest];
            r.attack = attackStrength;
            r.rejected = loudEnough ? NoteEvent::Rejected::NoKey : NoteEvent::Rejected::TooQuiet;
            rejected->push_back (r);
        }
    }
}

void KeyAttacks::trackReleases (std::vector<NoteEvent>& out)
{
    const double now = (double) samplesSeen / rate;
    const double updateSec = 2.0 * hopSize / rate;
    for (int p = lowPitch; p <= highPitch; ++p)
    {
        auto& k = keys[(std::size_t) p];
        if (! k.sounding) continue;
        float s = toDb (salience (fftFor (p).power, p));

        k.raw = { k.raw[1], k.raw[2], s };
        s = std::max (std::min (k.raw[0], k.raw[1]), std::min (std::max (k.raw[0], k.raw[1]), k.raw[2]));

        float recentMax = -1000.0f;
        for (int a = 1; a <= kDropLookback; ++a)
            recentMax = std::max (recentMax, k.history[(std::size_t) ((k.historyPos - a + 16) % 16)]);
        k.history[(std::size_t) k.historyPos] = s;
        k.historyPos = (k.historyPos + 1) % 16;
        if (k.refractory > 0) { --k.refractory; continue; }

        k.quietUpdates = (s - noiseSalience (p, true) < 6.0f || levelDb < opts.gateDb - 20.0f) ? k.quietUpdates + 1 : 0;

        if (s < recentMax - kReleaseDropDb || k.quietUpdates * updateSec > kFadeOutSeconds)
        {
            k.sounding = false;
            out.push_back ({ NoteEvent::Type::Off, p, now, 0.0f, levelDb });
        }
    }
}

}
