#include "app/FingerprintLearner.h"

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_events/juce_events.h>

namespace
{
    constexpr double kKeep = 20.0;
    constexpr double kAfter = 4.0;
    constexpr double kEvery = 8.0;
}

FingerprintLearner::FingerprintLearner() : juce::Thread ("Key learner") {}

FingerprintLearner::~FingerprintLearner()
{
    *alive = false;
    stopThread (10000);
}

bool FingerprintLearner::load (const juce::File& folder, juce::String& error)
{
    std::string why;
#if JUCE_WINDOWS
    const std::filesystem::path path (folder.getFullPathName().toWideCharPointer());
#else
    const std::filesystem::path path (folder.getFullPathName().toStdString());
#endif
    if (! model.load (path, why))
    {
        error = juce::String (why);
        return false;
    }
    modelReady = true;
    startThread (juce::Thread::Priority::low);
    return true;
}

void FingerprintLearner::start (std::shared_ptr<const piano::KeyFingerprints> known, int lowKey, int keyCount)
{
    if (! available())
        return;
    {
        const std::lock_guard<std::mutex> lock (requestLock);
        requestStart = true;
        requestKnown = std::move (known);
        requestLow = lowKey;
        requestCount = keyCount;
        ++requestId;
    }
    learning = true;
    notify();
}

void FingerprintLearner::stop()
{
    {
        const std::lock_guard<std::mutex> lock (requestLock);
        requestStart = false;
        requestKnown = nullptr;
        ++requestId;
    }
    learning = false;
    notify();
}

void FingerprintLearner::addAudio (const float* samples, int n, double rate, std::int64_t position)
{
    const std::lock_guard<std::mutex> lock (inputLock);
    if (rate != ringRate || position != ringEnd)
    {
        ringRate = rate;
        ring.assign ((std::size_t) (kKeep * rate), 0.0f);
        ringWrite = 0;
        ringFilled = 0;
        restarted = true;
    }
    for (int i = 0; i < n; ++i)
    {
        ring[ringWrite] = samples[i];
        ringWrite = (ringWrite + 1) % ring.size();
    }
    ringEnd = position + n;
    ringFilled = std::min<std::int64_t> (ringFilled + n, (std::int64_t) ring.size());
}

void FingerprintLearner::addStrikes (const std::vector<piano::CapturedStrike>& strikes)
{
    const std::lock_guard<std::mutex> lock (inputLock);
    incoming.insert (incoming.end(), strikes.begin(), strikes.end());
}

void FingerprintLearner::report (std::vector<int> missing)
{
    auto f = learner.fingerprints();
    if (f == nullptr)
        return;
    auto copy = std::make_shared<piano::KeyFingerprints> (*f);
    if (copy->complete() && copy->created.empty())
        copy->created = juce::Time::getCurrentTime().formatted ("%d %b %Y").toStdString();
    juce::MessageManager::callAsync ([this, stillHere = alive, copy, missing = std::move (missing)]
    {
        if (*stillHere && onProgress != nullptr)
            onProgress (copy, missing);
    });
}

void FingerprintLearner::run()
{
    std::vector<float> segment, resampled;
    while (! threadShouldExit())
    {
        wait (250);

        {
            const std::lock_guard<std::mutex> lock (requestLock);
            if (appliedId != requestId)
            {
                appliedId = requestId;
                if (requestStart) learner.start (requestKnown, requestLow, requestCount);
                else learner.stop();
                labelledUntil = -1.0;
            }
        }

        std::vector<piano::CapturedStrike> strikes;
        double rate = 0.0, now = 0.0, have = 0.0;
        {
            const std::lock_guard<std::mutex> lock (inputLock);
            strikes.swap (incoming);
            if (restarted)
            {
                restarted = false;
                learner.clockRestarted();
                labelledUntil = -1.0;
            }
            rate = ringRate;
            if (rate > 0.0)
            {
                now = (double) ringEnd / rate;
                have = (double) ringFilled / rate;
            }
        }
        if (! learner.isRunning() || rate <= 0.0)
            continue;
        for (const auto& s : strikes)
            learner.addStrike (s);
        if (learner.complete())
            continue;

        if (labelledUntil < 0.0)
            labelledUntil = std::max (0.0, now - kAfter);
        const double to = now - kAfter;
        if (to - labelledUntil < kEvery)
            continue;
        const double from = labelledUntil;
        labelledUntil = to;
        if (! learner.hasStrikesIn (from, to))
        {
            learner.label ({}, from, to);
            continue;
        }

        const double seconds = std::min (have, piano::transkun::kSegmentSeconds);
        {
            const std::lock_guard<std::mutex> lock (inputLock);
            if (ringRate != rate) continue;
            const auto n = (std::size_t) (seconds * rate);
            segment.resize (n);
            const std::size_t start = (ringWrite + ring.size() - n) % ring.size();
            for (std::size_t i = 0; i < n; ++i) segment[i] = ring[(start + i) % ring.size()];
            now = (double) ringEnd / rate;
        }
        const double startTime = now - seconds;
        const float* audio = segment.data();
        int count = (int) segment.size();
        if (std::abs (rate - piano::transkun::kRate) > 0.5)
        {
            const double ratio = rate / piano::transkun::kRate;
            resampled.assign ((std::size_t) ((double) segment.size() / ratio), 0.0f);
            juce::LagrangeInterpolator resampler;
            resampler.process (ratio, segment.data(), resampled.data(), (int) resampled.size(), (int) segment.size(), 0);
            audio = resampled.data();
            count = (int) resampled.size();
        }

        const auto notes = model.transcribe (audio, count, startTime);
        if (threadShouldExit())
            break;
        const auto learned = learner.label (notes, from, to);
        if (! learned.empty())
        {
            report (learner.missingKeys());
            if (learner.complete())
                learning = false;
        }
    }
}
