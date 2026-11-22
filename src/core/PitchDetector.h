#pragma once

#include <vector>

namespace piano
{
struct PitchEstimate
{
    float frequency = 0.0f;
    float clarity   = 0.0f;
    bool  voiced    = false;
};

class YinPitchDetector
{
public:
    void prepare (double sampleRate, float minFrequency, float maxFrequency, float threshold = 0.15f);

    int frameSize() const noexcept { return integration + maxLag; }

    PitchEstimate process (const float* frame) noexcept;

private:
    double sampleRate  = 48000.0;
    int    minLag      = 1;
    int    maxLag      = 1;
    int    integration = 1;
    float  threshold   = 0.15f;
    std::vector<float> diff;
};

}
