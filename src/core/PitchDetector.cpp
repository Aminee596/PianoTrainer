#include "core/PitchDetector.h"

#include <algorithm>
#include <cmath>

namespace piano
{
void YinPitchDetector::prepare (double sr, float minFrequency, float maxFrequency, float thresh)
{
    sampleRate  = sr;
    threshold   = thresh;
    minLag      = std::max (2, (int) std::floor (sr / maxFrequency));
    maxLag      = std::max (minLag + 2, (int) std::ceil (sr / minFrequency));
    integration = maxLag;
    diff.assign ((size_t) maxLag + 2, 0.0f);
}

PitchEstimate YinPitchDetector::process (const float* x) noexcept
{
    const int W = integration;

    diff[0] = 1.0f;
    float runningSum = 0.0f;

    for (int tau = 1; tau <= maxLag; ++tau)
    {
        float d = 0.0f;
        const float* a = x;
        const float* b = x + tau;
        for (int j = 0; j < W; ++j)
        {
            const float delta = a[j] - b[j];
            d += delta * delta;
        }
        runningSum += d;
        diff[(size_t) tau] = runningSum > 0.0f ? d * (float) tau / runningSum : 1.0f;
    }

    int best = -1;
    for (int tau = minLag; tau < maxLag; ++tau)
    {
        if (diff[(size_t) tau] < threshold)
        {
            while (tau + 1 < maxLag && diff[(size_t) tau + 1] < diff[(size_t) tau])
                ++tau;
            best = tau;
            break;
        }
    }

    PitchEstimate result;

    if (best < 0)
    {
        float m = 1.0f;
        for (int tau = minLag; tau < maxLag; ++tau)
            m = std::min (m, diff[(size_t) tau]);
        result.clarity = std::clamp (1.0f - m, 0.0f, 1.0f);
        return result;
    }

    float refined = (float) best;
    if (best > 1 && best < maxLag)
    {
        const float s0 = diff[(size_t) best - 1];
        const float s1 = diff[(size_t) best];
        const float s2 = diff[(size_t) best + 1];
        const float denom = s0 - 2.0f * s1 + s2;
        if (std::abs (denom) > 1e-9f)
            refined += 0.5f * (s0 - s2) / denom;
    }

    result.frequency = (float) (sampleRate / refined);
    result.clarity   = std::clamp (1.0f - diff[(size_t) best], 0.0f, 1.0f);
    result.voiced    = true;
    return result;
}

}
