#pragma once

#include "app/TranskunModel.h"
#include "core/KeyLearner.h"

#include <juce_core/juce_core.h>

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

class FingerprintLearner : private juce::Thread
{
public:
    FingerprintLearner();
    ~FingerprintLearner() override;

    bool load (const juce::File& folder, juce::String& error);
    bool available() const noexcept { return modelReady.load(); }

    void start (std::shared_ptr<const piano::KeyFingerprints> known, int lowKey, int keyCount);
    void stop();
    bool isLearning() const noexcept { return learning.load(); }

    bool wantsInput() const noexcept { return learning.load(); }
    void addAudio (const float* samples, int n, double rate, std::int64_t position);
    void addStrikes (const std::vector<piano::CapturedStrike>& strikes);

    std::function<void (std::shared_ptr<const piano::KeyFingerprints>, std::vector<int> missing)> onProgress;

private:
    void run() override;
    void report (std::vector<int> missing);

    piano::TranskunModel model;
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>> (true);
    std::atomic<bool> modelReady { false }, learning { false };

    std::mutex requestLock;
    int requestId = 0, appliedId = 0;
    bool requestStart = false;
    std::shared_ptr<const piano::KeyFingerprints> requestKnown;
    int requestLow = 0, requestCount = 0;

    std::mutex inputLock;
    std::vector<float> ring;
    std::size_t ringWrite = 0;
    std::int64_t ringEnd = 0;
    std::int64_t ringFilled = 0;
    double ringRate = 0.0;
    bool restarted = false;
    std::vector<piano::CapturedStrike> incoming;

    piano::KeyLearner learner;
    double labelledUntil = -1.0;
};
