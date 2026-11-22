#include "core/NoteListener.h"
#include "core/Score.h"

#include <algorithm>
#include <cmath>

namespace piano
{
namespace
{
    constexpr double kRefractorySec   = 0.05;
    constexpr double kOnsetExpirySec  = 0.20;
    constexpr double kOnsetLookbackSec = 0.012;
    constexpr double kPeakLookbackSec  = 0.050;
    constexpr double kRecentSec       = 1.0;
    constexpr float  kFastMinHz       = 260.0f;
    constexpr float  kFastAcceptHz    = 320.0f;

    bool isHarmonicOf (float high, float low)
    {
        if (low <= 0.0f) return false;
        const float ratio = high / low;
        const float k = std::round (ratio);
        return k >= 2.0f && std::abs (1200.0f * std::log2 (ratio / k)) < 40.0f;
    }
}

void NoteListener::prepare (double sampleRate, const ListenerOptions& options)
{
    rate = sampleRate;
    opts = options;
    opts.hopSize      = std::max (16, opts.hopSize);
    opts.stableFrames = std::max (1, opts.stableFrames);

    yin.prepare (rate, opts.minFrequency, opts.maxFrequency, opts.yinThreshold);

    useFast = opts.fastHighNotes && opts.maxFrequency > kFastAcceptHz && opts.minFrequency < kFastMinHz;
    if (useFast)
        yinFast.prepare (rate, kFastMinHz, opts.maxFrequency, opts.yinThreshold);

    ring.assign ((size_t) yin.frameSize(), 0.0f);
    frame.assign ((size_t) yin.frameSize(), 0.0f);
    reset();
}

void NoteListener::reset()
{
    std::fill (ring.begin(), ring.end(), 0.0f);
    writePos = filled = sinceLastHop = 0;
    samplesSeen = 0;
    levelWindow = std::min ((int) ring.size(), std::max (opts.hopSize, (int) std::ceil (rate / opts.minFrequency)));
    recentFrames = std::max (2, (int) std::ceil ((levelWindow + kOnsetLookbackSec * rate) / opts.hopSize));
    const int olderFrames = std::max (2, (int) std::ceil (kPeakLookbackSec * rate / opts.hopSize));
    levelHistory.assign ((size_t) (recentFrames + olderFrames), -120.0f);
    levelPos = 0;
    candidate = -1;
    candidateRun = 0;
    runAfterOnset = 0;
    candidateCents = 0.0f;
    onsetPending = false;
    onsetSample = 0;
    lastStrike = -1.0e9;
    refractory = 0;
    silentRun = 0;
    recentOns.fill ({ -1, -1.0e9 });
    recentPos = 0;
    current = {};
}

double NoteListener::latencySeconds() const noexcept
{
    return (double) yin.frameSize() / rate;
}

void NoteListener::process (const float* samples, int numSamples, std::vector<NoteEvent>& out)
{
    const int n = (int) ring.size();
    if (n == 0) return;

    for (int i = 0; i < numSamples; ++i)
    {
        ring[(size_t) writePos] = samples[i];
        writePos = (writePos + 1) % n;
        filled = std::min (filled + 1, n);
        ++samplesSeen;

        if (++sinceLastHop >= opts.hopSize)
        {
            sinceLastHop = 0;
            if (filled == n)
                analyse (out);
        }
    }
}

bool NoteListener::playedRecently (int pitch, double now) const noexcept
{
    for (const auto& r : recentOns)
        if (r.pitch == pitch && now - r.time < kRecentSec)
            return true;
    return false;
}

void NoteListener::emitOff (std::vector<NoteEvent>& out, double time)
{
    if (current.pitch < 0) return;
    NoteEvent e;
    e.type = NoteEvent::Type::Off;
    e.pitch = current.pitch;
    e.time = time;
    e.levelDb = current.levelDb;
    out.push_back (e);
    current.pitch = -1;
}

void NoteListener::analyse (std::vector<NoteEvent>& out)
{
    const int n = (int) ring.size();
    const double now = (double) samplesSeen / rate;
    const double hopSec = (double) opts.hopSize / rate;

    const int tail = n - writePos;
    std::copy (ring.begin() + writePos, ring.end(), frame.begin());
    std::copy (ring.begin(), ring.begin() + writePos, frame.begin() + tail);

    double sumSq = 0.0;
    for (int i = n - levelWindow; i < n; ++i)
        sumSq += (double) frame[(size_t) i] * frame[(size_t) i];
    const float levelDb = (float) (10.0 * std::log10 (sumSq / levelWindow + 1e-12));

    const int histSize = (int) levelHistory.size();
    float recentMin = 1000.0f, olderPeak = -1000.0f;
    for (int age = 1; age <= histSize; ++age)
    {
        const float v = levelHistory[(size_t) ((levelPos - age + histSize) % histSize)];
        if (age <= recentFrames) recentMin = std::min (recentMin, v);
        else                     olderPeak = std::max (olderPeak, v);
    }
    levelHistory[(size_t) levelPos] = levelDb;
    levelPos = (levelPos + 1) % histSize;

    if (refractory > 0) --refractory;

    if (! onsetPending && refractory == 0
        && levelDb > opts.gateDb && levelDb - recentMin > opts.onsetRiseDb
        && levelDb > olderPeak + opts.onsetOverPeakDb)
    {
        onsetPending  = true;
        onsetSample   = samplesSeen;
        runAfterOnset = 0;
        lastStrike    = (double) samplesSeen / rate;
    }

    if (onsetPending && (double) (samplesSeen - onsetSample) / rate > kOnsetExpirySec)
        onsetPending = false;

    current.levelDb = levelDb;

    auto unvoiced = [&]
    {
        current.frequency = 0.0f;
        candidateRun = 0;
        runAfterOnset = 0;
        if (++silentRun >= opts.releaseFrames)
            emitOff (out, now);
    };

    if (levelDb < opts.gateDb)
    {
        current.clarity = 0.0f;
        unvoiced();
        return;
    }

    auto est = yin.process (frame.data());
    bool fromFast = false;

    if (useFast)
    {
        const auto fast = yinFast.process (frame.data() + (n - yinFast.frameSize()));
        const bool longOK = est.voiced && est.clarity >= opts.minClarity;
        const bool fastOK = fast.voiced && fast.clarity >= opts.minClarity && fast.frequency >= kFastAcceptHz;

        if (fastOK)
        {
            const int fastPitch = (int) std::lround (frequencyToPitch (fast.frequency));
            const int longPitch = longOK ? (int) std::lround (frequencyToPitch (est.frequency)) : -1;

            bool takeFast;
            if (! longOK)
                takeFast = true;
            else if (fastPitch == longPitch)
                takeFast = false;
            else if (isHarmonicOf (fast.frequency, est.frequency))
                takeFast = onsetPending && longPitch == current.pitch;
            else
                takeFast = fastPitch != current.pitch;

            if (takeFast)
            {
                est = fast;
                fromFast = true;
            }
        }
    }

    current.clarity = est.clarity;

    if (! est.voiced || est.clarity < opts.minClarity)
    {
        unvoiced();
        return;
    }

    const double fractional = frequencyToPitch (est.frequency);
    const int    midi       = (int) std::lround (fractional);
    const float  cents      = (float) ((fractional - midi) * 100.0);

    current.frequency = est.frequency;

    if (std::abs (cents) > opts.maxCentsOff || midi < 0 || midi > 127)
    {
        unvoiced();
        return;
    }

    silentRun = 0;

    if (midi == candidate)
    {
        ++candidateRun;
        ++runAfterOnset;
    }
    else
    {
        candidate = midi;
        candidateRun = 1;
        runAfterOnset = 1;
    }
    candidateCents = cents;

    const bool isNew = midi != current.pitch;
    bool accept = false;

    if (isNew)
    {
        int needed = opts.stableFrames;
        if (! onsetPending)
            needed *= (current.pitch >= 0 && ! fromFast) ? 4 : 2;
        accept = candidateRun >= needed;

        if (accept && ! onsetPending && playedRecently (midi, now))
            accept = false;
    }
    else if (onsetPending)
    {
        const int window = fromFast ? yinFast.frameSize() : n;
        accept = runAfterOnset >= opts.stableFrames
              && (samplesSeen - onsetSample) >= (int64_t) (window * 3 / 4);
    }

    if (! accept)
        return;

    if (isNew)
        emitOff (out, now);

    NoteEvent e;
    e.type    = NoteEvent::Type::On;
    e.pitch   = midi;
    e.time    = now;
    e.cents   = candidateCents;
    e.levelDb = levelDb;
    out.push_back (e);

    recentOns[(size_t) recentPos] = { midi, now };
    recentPos = (recentPos + 1) % (int) recentOns.size();

    current.pitch = midi;
    onsetPending  = false;
    refractory    = (int) std::ceil (kRefractorySec / hopSec);
}

}
