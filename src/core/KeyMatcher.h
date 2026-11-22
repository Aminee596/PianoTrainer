#pragma once

#include "core/KeyFingerprints.h"

#include <memory>
#include <vector>

namespace piano
{
struct KeyMatch
{
    std::vector<int> keys;
    float cost = 1.0f;
    std::vector<float> gains;
    int closest = -1;
};

class KeyMatcher
{
public:
    void setFingerprints (std::shared_ptr<const KeyFingerprints> f) { fp = std::move (f); }
    const KeyFingerprints* fingerprints() const noexcept { return fp.get(); }

    KeyMatch match (const std::vector<float>& sound, int offsetIndex, float level = 1.0f) const;

    float loudness (const std::vector<float>& sound, int key, int offsetIndex) const;

private:
    std::shared_ptr<const KeyFingerprints> fp;
};

}
