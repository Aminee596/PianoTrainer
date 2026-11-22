#include "core/Spectrum.h"

#include <algorithm>
#include <cmath>

namespace piano
{
namespace
{
    constexpr double kPi = 3.14159265358979323846;
    constexpr double kToleranceCents = 35.0;
}

void Spectrum::prepare (int n, double sampleRate)
{
    size = n;
    rate = sampleRate;
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
    peaks.clear();
}

void Spectrum::run (const float* samples, int available, bool findPeaks)
{
    const int n = size;
    const float* src = samples + (available - n);
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

    const float scale = 1.0f / ((float) n * (float) n);
    for (int k = 0; k <= n / 2; ++k)
        power[(std::size_t) k] = (re[(std::size_t) k] * re[(std::size_t) k] + im[(std::size_t) k] * im[(std::size_t) k]) * scale;

    if (! findPeaks)
        return;

    peaks = peaksOf (power);
}

std::vector<Spectrum::Peak> Spectrum::peaksOf (const std::vector<float>& pw) const
{
    std::vector<Peak> found;
    const double binHz = rate / size;
    const int n = std::min (size / 2, (int) pw.size() - 1);
    for (int k = 2; k < n - 1; ++k)
    {
        const float c = pw[(std::size_t) k];
        if (c <= pw[(std::size_t) k - 1] || c < pw[(std::size_t) k + 1] || c <= 1.0e-16f) continue;
        const float a = std::log (pw[(std::size_t) k - 1] + 1.0e-20f), b = std::log (c), d = std::log (pw[(std::size_t) k + 1] + 1.0e-20f);
        const float denom = a - 2.0f * b + d;
        const float offset = std::abs (denom) > 1.0e-9f ? std::clamp (0.5f * (a - d) / denom, -0.5f, 0.5f) : 0.0f;
        const float logPeak = b - 0.25f * (a - d) * offset;
        found.push_back ({ (float) ((k + offset) * binHz), std::sqrt (std::exp (logPeak)) });
    }
    return found;
}

int Spectrum::peakIndex (const std::vector<Peak>& list, double hz, double minBins) const
{
    if (list.empty() || hz <= 0.0) return -1;
    const double tolHz = std::max (hz * (std::pow (2.0, kToleranceCents / 1200.0) - 1.0), minBins * rate / size);
    const auto it = std::lower_bound (list.begin(), list.end(), (float) hz, [] (const Peak& p, float x) { return p.hz < x; });
    int best = -1;
    double bestDist = tolHz;
    for (auto c = it == list.begin() ? it : it - 1; c != list.end() && c <= it; ++c)
    {
        const double dist = std::abs (c->hz - hz);
        if (dist <= bestDist) { bestDist = dist; best = (int) (c - list.begin()); }
    }
    return best;
}

float Spectrum::amplitudeNear (const std::vector<Peak>& list, double hz, double minBins) const
{
    const int i = peakIndex (list, hz, minBins);
    return i >= 0 ? list[(std::size_t) i].amp : 0.0f;
}

float Spectrum::amplitudeAt (const std::vector<Peak>& list, double hz) const
{
    const int i = peakIndex (list, hz);
    return i >= 0 ? list[(std::size_t) i].amp : 0.0f;
}

void Spectrum::bandBins (double hz, int& first, int& last, double minBins) const
{
    const double binHz = rate / size;
    const double half = std::max (hz * (std::pow (2.0, kToleranceCents / 1200.0) - 1.0), minBins * binHz);
    first = std::max (1, (int) std::lround ((hz - half) / binHz));
    last  = std::min (size / 2, (int) std::lround ((hz + half) / binHz));
}

float Spectrum::bandAmplitude (const std::vector<float>& savedPower, double hz, double minBins) const
{
    if (savedPower.empty() || hz <= 0.0) return 0.0f;
    int b0 = 0, b1 = 0;
    bandBins (hz, b0, b1, minBins);
    b1 = std::min (b1, (int) savedPower.size() - 1);
    float sum = 0.0f;
    for (int b = b0; b <= b1; ++b) sum += savedPower[(std::size_t) b];
    return std::sqrt (sum);
}

float Spectrum::medianAmplitude (double loHz, double hiHz) const
{
    const int b0 = std::max (1, (int) (loHz * size / rate)), b1 = std::min (size / 2, (int) (hiHz * size / rate));
    if (b1 <= b0) return 0.0f;
    std::vector<float> band (power.begin() + b0, power.begin() + b1);
    std::nth_element (band.begin(), band.begin() + (long) band.size() / 2, band.end());
    return std::sqrt (band[band.size() / 2]);
}

}
