#pragma once

#include <vector>

namespace piano::transkun
{
constexpr int    kRate = 44100;
constexpr int    kWindow = 4096;
constexpr int    kHop = 1024;
constexpr int    kMels = 229;
constexpr int    kWindows = 6;
constexpr int    kBins = kWindow / 2 + 1;
constexpr int    kTracks = 90;
constexpr double kSegmentSeconds = 16.0;
constexpr double kStepSeconds = 8.0;

inline int trackPitch (int track) noexcept { return track == 0 ? -64 : track == 1 ? -67 : 19 + track; }

class Features
{
public:
    bool load (std::vector<float> freq2mels, std::vector<float> windows);
    bool ready() const noexcept { return ! melFilter.empty(); }

    std::vector<float> compute (const float* audio, int n, int& frames) const;

private:
    std::vector<float> melFilter, analysisWindows;
    std::vector<int> melFirst, melLast;
};

struct Interval { int begin, end; };

std::vector<std::vector<Interval>> decode (const float* scores, int frames, int tracks = kTracks);

}
