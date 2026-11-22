#include "core/ChordDetector.h"
#include "core/Score.h"

#include <algorithm>
#include <cmath>

namespace piano
{
namespace
{
    constexpr double kPi = 3.14159265358979323846;

    constexpr int   kPartials        = 8;
    constexpr float kInharmonicity   = 0.0004f;
    constexpr double kCents          = 35.0;
    constexpr float kPresentDb       = 16.0f;
    constexpr float kNeighbourDb     = 3.0f;
    constexpr float kRiseDb          = 8.0f;
    constexpr float kReleaseDropDb   = 14.0f;
    constexpr float kGateDb          = -70.0f;
    constexpr int   kRiseLookback    = 10;
    constexpr int   kDropLookback    = 12;
    constexpr int   kRefractoryHops  = 12;
    constexpr float kRestrikeBelowPeakDb = 10.0f;
    constexpr int   kMaxPendingHops  = 8;
    constexpr float kLowPartialDb    = 14.0f;
    constexpr int   kFadeOutHops     = 280;
    constexpr double kLateCreditSeconds = 0.15;
    constexpr int   kShortFftMinPitch = 67;

    float toDb (float amplitude) { return 20.0f * std::log10 (amplitude + 1.0e-9f); }
}

void ChordDetector::Fft::prepare (int n)
{
    size = n;
    window.resize ((std::size_t) n);
    for (int i = 0; i < n; ++i)
        window[(std::size_t) i] = (float) (0.5 - 0.5 * std::cos (2.0 * kPi * i / (n - 1)));
    re.assign ((std::size_t) n, 0.0f);
    im.assign ((std::size_t) n, 0.0f);
    power.assign ((std::size_t) n / 2 + 1, 0.0f);

    int bits = 0;
    while ((1 << bits) < n) ++bits;
    bitrev.resize ((std::size_t) n);
    for (int i = 0; i < n; ++i)
    {
        int r = 0;
        for (int b = 0; b < bits; ++b)
            if (i & (1 << b)) r |= 1 << (bits - 1 - b);
        bitrev[(std::size_t) i] = r;
    }
    cosTable.resize ((std::size_t) n / 2);
    sinTable.resize ((std::size_t) n / 2);
    for (int i = 0; i < n / 2; ++i)
    {
        cosTable[(std::size_t) i] = (float) std::cos (2.0 * kPi * i / n);
        sinTable[(std::size_t) i] = (float) -std::sin (2.0 * kPi * i / n);
    }
}

void ChordDetector::Fft::run (const float* newestLast, int available, double sampleRate)
{
    const int n = size;
    const float* src = newestLast + (available - n);
    for (int i = 0; i < n; ++i)
    {
        re[(std::size_t) bitrev[(std::size_t) i]] = src[i] * window[(std::size_t) i];
        im[(std::size_t) bitrev[(std::size_t) i]] = 0.0f;
    }

    for (int len = 2; len <= n; len <<= 1)
    {
        const int half = len >> 1, step = n / len;
        for (int start = 0; start < n; start += len)
            for (int k = 0; k < half; ++k)
            {
                const float wr = cosTable[(std::size_t) (k * step)], wi = sinTable[(std::size_t) (k * step)];
                const auto a = (std::size_t) (start + k), b = (std::size_t) (start + k + half);
                const float tr = re[b] * wr - im[b] * wi;
                const float ti = re[b] * wi + im[b] * wr;
                re[b] = re[a] - tr; im[b] = im[a] - ti;
                re[a] += tr;        im[a] += ti;
            }
    }

    for (int k = 0; k <= n / 2; ++k)
        power[(std::size_t) k] = re[(std::size_t) k] * re[(std::size_t) k] + im[(std::size_t) k] * im[(std::size_t) k];

    peaks.clear();
    const double binHz = sampleRate / n;
    for (int k = 2; k < n / 2 - 1; ++k)
    {
        const float c = power[(std::size_t) k];
        if (c <= power[(std::size_t) k - 1] || c < power[(std::size_t) k + 1] || c <= 0.0f) continue;
        const float a = std::log (power[(std::size_t) k - 1] + 1.0e-20f), b = std::log (c), d = std::log (power[(std::size_t) k + 1] + 1.0e-20f);
        const float denom = a - 2.0f * b + d;
        const float offset = std::abs (denom) > 1.0e-9f ? 0.5f * (a - d) / denom : 0.0f;
        const float logPeak = b - 0.25f * (a - d) * offset;
        peaks.push_back ({ (float) ((k + offset) * binHz), std::sqrt (std::exp (logPeak)) / (float) n });
    }
}

void ChordDetector::prepare (double sampleRate)
{
    rate = sampleRate;
    hop = std::max (64, (int) std::lround (256.0 * sampleRate / 48000.0));
    fftLong.prepare (sampleRate > 60000.0 ? 8192 : 4096);
    fftShort.prepare (sampleRate > 60000.0 ? 4096 : 2048);
    ring.assign ((std::size_t) fftLong.size, 0.0f);
    linear.assign ((std::size_t) fftLong.size, 0.0f);
    reset();
}

void ChordDetector::reset()
{
    std::fill (ring.begin(), ring.end(), 0.0f);
    writePos = filled = sinceHop = 0;
    samplesSeen = 0;
    for (auto& s : states) s = PitchState {};
    for (auto& s : states) s.history.fill (-200.0f);
    lastSalience.fill (-200.0f);
}

void ChordDetector::setWatched (const std::vector<int>& chord, const std::vector<int>& held)
{
    auto bits = [] (const std::vector<int>& pitches, std::uint64_t& lo, std::uint64_t& hi)
    {
        for (int p : pitches)
        {
            if (p < 0 || p > 127) continue;
            if (p < 64) lo |= std::uint64_t (1) << p;
            else        hi |= std::uint64_t (1) << (p - 64);
        }
    };
    std::uint64_t clo = 0, chi = 0;
    bits (chord, clo, chi);
    std::uint64_t lo = clo, hi = chi;
    bits (held, lo, hi);
    chordBits[0].store (clo, std::memory_order_relaxed);
    chordBits[1].store (chi, std::memory_order_relaxed);
    watchedBits[0].store (lo, std::memory_order_relaxed);
    watchedBits[1].store (hi, std::memory_order_relaxed);
}

float ChordDetector::salienceDb (int pitch) const
{
    return pitch >= 0 && pitch < 128 ? lastSalience[(std::size_t) pitch] : -200.0f;
}

bool ChordDetector::isSounding (int pitch) const
{
    return pitch >= 0 && pitch < 128 && states[(std::size_t) pitch].sounding;
}

bool ChordDetector::hasFundamental (int pitch) const
{
    if (fftLong.peaks.empty())
        return false;
    float loudest = 0.0f;
    for (const auto& p : fftLong.peaks)
        loudest = std::max (loudest, p.amp);
    const float fundamental = peakAmplitude (fftLong, pitchToFrequency (pitch), kCents);
    return fundamental > 0.0f && fundamental >= loudest * 0.02f;
}

float ChordDetector::peakAmplitude (const Fft& f, double hz, double cents)
{
    const auto it = std::lower_bound (f.peaks.begin(), f.peaks.end(), (float) hz, [] (const Fft::Peak& pk, float x) { return pk.hz < x; });
    float best = 0.0f;
    double bestDist = cents;
    for (auto c = it == f.peaks.begin() ? it : it - 1; c != f.peaks.end() && c <= it; ++c)
    {
        const double dist = std::abs (1200.0 * std::log2 (c->hz / hz));
        if (dist < bestDist) { bestDist = dist; best = c->amp; }
    }
    return best;
}

float ChordDetector::unsharedEvidence (const Fft& f, int pitch, int ratio) const
{
    const double f0 = pitchToFrequency (pitch);
    float sum = 0.0f;
    for (int k = 1; k <= kPartials; ++k)
    {
        if (k % ratio == 0) continue;
        const double fk = k * f0 * std::sqrt (1.0 + kInharmonicity * k * k);
        if (fk > std::min (8000.0, rate * 0.45)) break;
        sum += peakAmplitude (f, fk, kCents) / std::sqrt ((float) k);
    }
    return sum;
}

float ChordDetector::measure (int pitch) const
{
    const Fft& f = pitch >= kShortFftMinPitch ? fftShort : fftLong;
    const double f0 = pitchToFrequency (pitch);
    float sum = 0.0f;
    for (int k = 1; k <= kPartials; ++k)
    {
        const double fk = k * f0 * std::sqrt (1.0 + kInharmonicity * k * k);
        if (fk > std::min (8000.0, rate * 0.45)) break;
        sum += peakAmplitude (f, fk, kCents) / std::sqrt ((float) k);
    }
    return toDb (sum);
}

void ChordDetector::process (const float* samples, int numSamples, std::vector<NoteEvent>& out)
{
    const int n = (int) ring.size();
    if (n == 0) return;

    for (int i = 0; i < numSamples; ++i)
    {
        ring[(std::size_t) writePos] = samples[i];
        writePos = (writePos + 1) % n;
        filled = std::min (filled + 1, n);
        ++samplesSeen;
        if (++sinceHop < hop || filled < n) continue;
        sinceHop = 0;

        std::copy (ring.begin() + writePos, ring.end(), linear.begin());
        std::copy (ring.begin(), ring.begin() + writePos, linear.begin() + (n - writePos));

        double sumSq = 0.0;
        for (int j = n - fftShort.size; j < n; ++j) sumSq += (double) linear[(std::size_t) j] * linear[(std::size_t) j];
        const float levelDb = (float) (10.0 * std::log10 (sumSq / fftShort.size + 1.0e-12));

        fftLong.run (linear.data(), n, rate);
        fftShort.run (linear.data(), n, rate);

        auto noiseDb = [this] (const Fft& f)
        {
            const int b0 = std::max (1, (int) (60.0 * f.size / rate)), b1 = std::min (f.size / 2, (int) (5000.0 * f.size / rate));
            std::vector<float> band (f.power.begin() + b0, f.power.begin() + b1);
            std::nth_element (band.begin(), band.begin() + band.size() / 2, band.end());
            const float amp = std::sqrt (band[band.size() / 2] * 4.0f) / (float) f.size;
            float sum = 0.0f;
            for (int k = 1; k <= kPartials; ++k) sum += amp / std::sqrt ((float) k);
            return toDb (sum);
        };
        const float noiseLong = noiseDb (fftLong), noiseShort = noiseDb (fftShort);

        std::array<float, 128> sal {};
        sal.fill (-200.0f);
        for (int p = lowestPitch - 1; p <= highestPitch + 1; ++p)
            sal[(std::size_t) p] = measure (p);

        const std::uint64_t lo = watchedBits[0].load (std::memory_order_relaxed);
        const std::uint64_t hi = watchedBits[1].load (std::memory_order_relaxed);
        auto watched = [lo, hi] (int p) { return p < 64 ? ((lo >> p) & 1u) != 0 : ((hi >> (p - 64)) & 1u) != 0; };
        const std::uint64_t clo = chordBits[0].load (std::memory_order_relaxed);
        const std::uint64_t chi = chordBits[1].load (std::memory_order_relaxed);
        auto inChord = [clo, chi] (int p) { return p < 64 ? ((clo >> p) & 1u) != 0 : ((chi >> (p - 64)) & 1u) != 0; };
        auto wasWatched = [this] (int p) { return p < 64 ? ((prevWatched[0] >> p) & 1u) != 0 : ((prevWatched[1] >> (p - 64)) & 1u) != 0; };
        const double now = (double) samplesSeen / rate;

        for (int p = lowestPitch; p <= highestPitch; ++p)
        {
            const auto& st = states[(std::size_t) p];
            if (watched (p) && ! wasWatched (p) && st.sounding && now - st.onsetTime < kLateCreditSeconds)
                out.push_back ({ NoteEvent::Type::On, p, st.onsetTime, 0.0f, levelDb });
        }
        prevWatched = { lo, hi };

        for (int p = lowestPitch; p <= highestPitch; ++p)
        {
            auto& st = states[(std::size_t) p];

            float s = sal[(std::size_t) p];
            const float noise = p >= kShortFftMinPitch ? noiseShort : noiseLong;

            const Fft& mine = p >= kShortFftMinPitch ? fftShort : fftLong;
            bool pending = false, explained = false;
            for (int ratio : { 2, 3, 4 })
            {
                const int q = p - (ratio == 2 ? 12 : ratio == 3 ? 19 : 24);
                if (q < lowestPitch || (inChord (q) && inChord (p))) continue;
                const float own = toDb (unsharedEvidence (mine, q, ratio));
                if (! (own > s - 6.0f && own - noise > kPresentDb)) continue;
                if (toDb (peakAmplitude (mine, pitchToFrequency (q), kCents)) > s - 14.0f)
                {
                    s -= 40.0f;
                    explained = true;
                    pending = false;
                    break;
                }
                pending = true;
            }

            st.raw = { st.raw[1], st.raw[2], s };
            s = std::max (std::min (st.raw[0], st.raw[1]), std::min (std::max (st.raw[0], st.raw[1]), st.raw[2]));

            const bool loud = levelDb > kGateDb && s - noise > kPresentDb;
            const double f0 = pitchToFrequency (p);
            const float lowPartial = toDb (std::max (peakAmplitude (mine, f0, kCents),
                                                     peakAmplitude (mine, 2.0 * f0 * std::sqrt (1.0 + 4.0 * kInharmonicity), kCents)));
            const bool present = loud && ! explained && lowPartial > s - kLowPartialDb
                              && s - std::max (sal[(std::size_t) p - 1], sal[(std::size_t) p + 1]) > kNeighbourDb;

            float recentMin = 1000.0f, recentMax = -1000.0f;
            for (int a = 1; a <= kDropLookback; ++a)
            {
                const float h = st.history[(std::size_t) ((st.historyPos - a + 16) % 16)];
                if (a <= kRiseLookback) recentMin = std::min (recentMin, h);
                recentMax = std::max (recentMax, h);
            }
            st.history[(std::size_t) st.historyPos] = s;
            st.historyPos = (st.historyPos + 1) % 16;
            if (st.refractory > 0) --st.refractory;

            if (st.sounding)
            {
                if (s >= st.peakSinceOnset) { st.peakSinceOnset = s; st.troughSincePeak = s; }
                else st.troughSincePeak = std::min (st.troughSincePeak, s);
            }

            const float reference = st.sounding ? st.troughSincePeak : recentMin;
            st.uncertainHops = present && pending ? st.uncertainHops + 1 : 0;
            const bool strike = present && st.refractory == 0 && s - reference > kRiseDb
                             && (! st.sounding || s > st.peakSinceOnset - kRestrikeBelowPeakDb)
                             && (! pending || st.uncertainHops > kMaxPendingHops);

            st.quietHops = st.sounding && ! loud ? st.quietHops + 1 : 0;
            const bool released = st.sounding && (s < recentMax - kReleaseDropDb || st.quietHops > kFadeOutHops);

            if (strike)
            {
                if (st.sounding && watched (p))
                    out.push_back ({ NoteEvent::Type::Off, p, now, 0.0f, levelDb });
                st.sounding = true;
                st.onsetTime = now;
                st.peakSinceOnset = st.troughSincePeak = s;
                st.refractory = kRefractoryHops;
                if (watched (p))
                    out.push_back ({ NoteEvent::Type::On, p, now, 0.0f, levelDb });
            }
            else if (released && st.refractory == 0)
            {
                st.sounding = false;
                st.refractory = kRefractoryHops;
                if (watched (p))
                    out.push_back ({ NoteEvent::Type::Off, p, now, 0.0f, levelDb });
            }
        }

        lastSalience = sal;
    }
}

}
