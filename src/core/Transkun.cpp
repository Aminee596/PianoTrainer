#include "core/Transkun.h"

#include <cmath>
#include <complex>
#include <utility>

namespace piano::transkun
{
namespace
{
    constexpr double kEps = 1.0e-5;
    constexpr double kPi = 3.14159265358979323846;

    void fft (std::vector<std::complex<double>>& a)
    {
        const std::size_t n = a.size();
        for (std::size_t i = 1, j = 0; i < n; ++i)
        {
            std::size_t bit = n >> 1;
            for (; j & bit; bit >>= 1) j ^= bit;
            j ^= bit;
            if (i < j) std::swap (a[i], a[j]);
        }
        for (std::size_t len = 2; len <= n; len <<= 1)
        {
            const double angle = -2.0 * kPi / (double) len;
            const std::complex<double> step (std::cos (angle), std::sin (angle));
            for (std::size_t i = 0; i < n; i += len)
            {
                std::complex<double> w (1.0, 0.0);
                for (std::size_t k = 0; k < len / 2; ++k)
                {
                    const auto u = a[i + k], v = a[i + k + len / 2] * w;
                    a[i + k] = u + v;
                    a[i + k + len / 2] = u - v;
                    w *= step;
                }
            }
        }
    }
}

bool Features::load (std::vector<float> freq2mels, std::vector<float> windows)
{
    if (freq2mels.size() != (std::size_t) kBins * kMels || windows.size() != (std::size_t) kWindows * kWindow)
        return false;
    melFilter = std::move (freq2mels);
    analysisWindows = std::move (windows);
    melFirst.assign (kMels, 0);
    melLast.assign (kMels, -1);
    for (int m = 0; m < kMels; ++m)
    {
        int first = -1, last = -1;
        for (int k = 0; k < kBins; ++k)
            if (melFilter[(std::size_t) (k * kMels + m)] != 0.0f)
            {
                if (first < 0) first = k;
                last = k;
            }
        melFirst[(std::size_t) m] = first < 0 ? 0 : first;
        melLast[(std::size_t) m] = last;
    }
    return true;
}

std::vector<float> Features::compute (const float* audio, int n, int& frames) const
{
    frames = (int) std::ceil ((double) n / kHop) + 1;
    const int lPad = kWindow / 2;
    const int rPad = (frames - 1) * kHop + kWindow / 2 - n;
    std::vector<double> padded ((std::size_t) (lPad + n + std::max (rPad, 0)), 0.0);
    for (int i = 0; i < n; ++i) padded[(std::size_t) (lPad + i)] = audio[i];

    const double count = (double) frames * kWindow;
    double sum = 0.0;
    for (int f = 0; f < frames; ++f)
        for (int j = 0; j < kWindow; ++j) sum += padded[(std::size_t) (f * kHop + j)];
    const double mean = sum / count;
    double sq = 0.0;
    for (int f = 0; f < frames; ++f)
        for (int j = 0; j < kWindow; ++j)
        {
            const double d = padded[(std::size_t) (f * kHop + j)] - mean;
            sq += d * d;
        }
    const double invStd = 1.0 / (std::sqrt (sq / (count - 1.0)) + 1.0e-8);

    std::vector<float> out ((std::size_t) frames * kMels * kWindows, 0.0f);
    std::vector<std::complex<double>> buf (kWindow);
    std::vector<double> power (kBins);
    const double logEps = std::log (kEps);
    for (int f = 0; f < frames; ++f)
        for (int w = 0; w < kWindows; ++w)
        {
            const float* win = analysisWindows.data() + (std::size_t) w * kWindow;
            for (int j = 0; j < kWindow; ++j)
                buf[(std::size_t) j] = { (padded[(std::size_t) (f * kHop + j)] - mean) * invStd * win[j], 0.0 };
            fft (buf);
            for (int k = 0; k < kBins; ++k) power[(std::size_t) k] = std::norm (buf[(std::size_t) k]) / kWindow;
            for (int m = 0; m < kMels; ++m)
            {
                double acc = 0.0;
                for (int k = melFirst[(std::size_t) m]; k <= melLast[(std::size_t) m]; ++k)
                    acc += power[(std::size_t) k] * melFilter[(std::size_t) (k * kMels + m)];
                out[((std::size_t) f * kMels + (std::size_t) m) * kWindows + (std::size_t) w] = (float) ((std::log (acc + kEps) - logEps) / -logEps);
            }
        }
    return out;
}

std::vector<std::vector<Interval>> decode (const float* S, int T, int tracks)
{
    std::vector<std::vector<Interval>> result ((std::size_t) tracks);
    if (T <= 0) return result;
    auto at = [&] (int e, int b, int k) { return S[((std::size_t) e * (std::size_t) T + (std::size_t) b) * (std::size_t) tracks + (std::size_t) k]; };

    std::vector<float> q ((std::size_t) T * (std::size_t) tracks, 0.0f);
    std::vector<int> ptr ((std::size_t) std::max (T - 1, 0) * (std::size_t) tracks, -1);
    for (int k = 0; k < tracks; ++k)
        q[(std::size_t) ((T - 1) * tracks + k)] = std::max (at (T - 1, T - 1, k), 0.0f);
    for (int i = 1; i < T; ++i)
    {
        const int begin = T - i - 1;
        for (int k = 0; k < tracks; ++k)
        {
            float best = q[(std::size_t) ((T - i) * tracks + k)];
            int sel = 0;
            for (int e = T - i; e < T; ++e)
            {
                const float cand = q[(std::size_t) (e * tracks + k)] + at (e, begin, k);
                if (cand > best) { best = cand; sel = e - (T - i) + 1; }
            }
            ptr[(std::size_t) ((i - 1) * tracks + k)] = sel - 1;
            q[(std::size_t) (begin * tracks + k)] = best + std::max (at (begin, begin, k), 0.0f);
        }
    }

    for (int k = 0; k < tracks; ++k)
    {
        auto& notes = result[(std::size_t) k];
        int j = 0;
        while (j < T - 1)
        {
            const int sel = ptr[(std::size_t) ((T - j - 2) * tracks + k)];
            if (at (j, j, k) > 0.0f) notes.push_back ({ j, j });
            if (sel < 0) ++j;
            else
            {
                const int e = sel + j + 1;
                notes.push_back ({ j, e });
                j = e;
            }
        }
        if (at (T - 1, T - 1, k) > 0.0f) notes.push_back ({ T - 1, T - 1 });
    }
    return result;
}

}
