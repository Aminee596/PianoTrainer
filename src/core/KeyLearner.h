#pragma once

#include "core/KeyFingerprints.h"

#include <memory>
#include <vector>

namespace piano
{
struct HeardNote
{
    int pitch = 0;
    double onset = 0.0;
};

class KeyLearner
{
public:
    void start (std::shared_ptr<const KeyFingerprints> existing, int lowKey, int keyCount);
    void stop() { running = false; pending.clear(); }
    bool isRunning() const noexcept { return running; }
    bool complete() const noexcept { return fp != nullptr && fp->complete(); }

    void addStrike (const CapturedStrike& strike);
    bool hasStrikesIn (double from, double to) const;
    std::vector<int> label (const std::vector<HeardNote>& notes, double from, double to);
    void clockRestarted() { pending.clear(); }

    std::shared_ptr<const KeyFingerprints> fingerprints() const { return fp; }
    std::vector<int> missingKeys() const;

private:
    void adoptLayout (const CapturedStrike& strike);

    bool running = false;
    int lowKey = 0, keyCount = 0;
    std::shared_ptr<const KeyFingerprints> given;
    std::shared_ptr<KeyFingerprints> fp;
    std::vector<CapturedStrike> pending;
    std::vector<std::vector<CapturedStrike>> candidates;
};

}
