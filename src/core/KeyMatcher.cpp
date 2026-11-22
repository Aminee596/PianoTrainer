#include "core/KeyMatcher.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace piano
{
namespace
{
    constexpr int    kPrefilter  = 24;
    constexpr int    kPool       = 12;
    constexpr double kPoolMin    = 0.08;
    constexpr int    kIterations = 8;
    constexpr double kFloor      = 0.02;
    constexpr double kQuietest   = 0.7;
    constexpr double kLoudest    = 1.3;
    constexpr double kKeyCost    = 0.09;
    constexpr int    kMaxKeys    = 10;

    bool solve (std::vector<double>& a, std::vector<double>& b, int n)
    {
        for (int c = 0; c < n; ++c)
        {
            int pivot = c;
            for (int r = c + 1; r < n; ++r)
                if (std::abs (a[(std::size_t) (r * n + c)]) > std::abs (a[(std::size_t) (pivot * n + c)])) pivot = r;
            if (std::abs (a[(std::size_t) (pivot * n + c)]) < 1.0e-30) return false;
            if (pivot != c)
            {
                for (int k = 0; k < n; ++k) std::swap (a[(std::size_t) (c * n + k)], a[(std::size_t) (pivot * n + k)]);
                std::swap (b[(std::size_t) c], b[(std::size_t) pivot]);
            }
            for (int r = c + 1; r < n; ++r)
            {
                const double f = a[(std::size_t) (r * n + c)] / a[(std::size_t) (c * n + c)];
                if (f == 0.0) continue;
                for (int k = c; k < n; ++k) a[(std::size_t) (r * n + k)] -= f * a[(std::size_t) (c * n + k)];
                b[(std::size_t) r] -= f * b[(std::size_t) c];
            }
        }
        for (int r = n - 1; r >= 0; --r)
        {
            double s = b[(std::size_t) r];
            for (int k = r + 1; k < n; ++k) s -= a[(std::size_t) (r * n + k)] * b[(std::size_t) k];
            b[(std::size_t) r] = s / a[(std::size_t) (r * n + r)];
        }
        return true;
    }

    std::vector<double> nnls (const std::vector<double>& G, const std::vector<double>& h, int n)
    {
        std::vector<double> x ((std::size_t) n, 0.0), w ((std::size_t) n), s ((std::size_t) n);
        std::vector<bool> passive ((std::size_t) n, false);
        double scale = 0.0;
        for (int i = 0; i < n; ++i) scale = std::max (scale, G[(std::size_t) (i * n + i)]);
        const double tol = 1.0e-10 * std::max (scale, 1.0e-30);
        const double ridge = 1.0e-9 * std::max (scale, 1.0e-30);

        auto solvePassive = [&]
        {
            std::vector<int> idx;
            for (int i = 0; i < n; ++i) if (passive[(std::size_t) i]) idx.push_back (i);
            const int m = (int) idx.size();
            std::vector<double> a ((std::size_t) (m * m)), b ((std::size_t) m);
            for (int r = 0; r < m; ++r)
            {
                b[(std::size_t) r] = h[(std::size_t) idx[(std::size_t) r]];
                for (int c = 0; c < m; ++c)
                    a[(std::size_t) (r * m + c)] = G[(std::size_t) (idx[(std::size_t) r] * n + idx[(std::size_t) c])] + (r == c ? ridge : 0.0);
            }
            std::fill (s.begin(), s.end(), 0.0);
            if (m > 0 && solve (a, b, m))
                for (int r = 0; r < m; ++r) s[(std::size_t) idx[(std::size_t) r]] = b[(std::size_t) r];
        };

        for (int outer = 0; outer < 3 * n + 3; ++outer)
        {
            for (int i = 0; i < n; ++i)
            {
                double gx = 0.0;
                for (int k = 0; k < n; ++k) gx += G[(std::size_t) (i * n + k)] * x[(std::size_t) k];
                w[(std::size_t) i] = h[(std::size_t) i] - gx;
            }
            int j = -1;
            for (int i = 0; i < n; ++i)
                if (! passive[(std::size_t) i] && w[(std::size_t) i] > tol && (j < 0 || w[(std::size_t) i] > w[(std::size_t) j])) j = i;
            if (j < 0) break;
            passive[(std::size_t) j] = true;

            for (int inner = 0; inner < 3 * n + 3; ++inner)
            {
                solvePassive();
                bool allPositive = true;
                for (int i = 0; i < n; ++i)
                    if (passive[(std::size_t) i] && s[(std::size_t) i] <= 0.0) allPositive = false;
                if (allPositive) { x = s; break; }
                double alpha = 1.0;
                for (int i = 0; i < n; ++i)
                    if (passive[(std::size_t) i] && s[(std::size_t) i] <= 0.0)
                    {
                        const double d = x[(std::size_t) i] - s[(std::size_t) i];
                        if (d > 0.0) alpha = std::min (alpha, x[(std::size_t) i] / d);
                    }
                for (int i = 0; i < n; ++i)
                {
                    x[(std::size_t) i] += alpha * (s[(std::size_t) i] - x[(std::size_t) i]);
                    if (passive[(std::size_t) i] && x[(std::size_t) i] <= tol) { passive[(std::size_t) i] = false; x[(std::size_t) i] = 0.0; }
                }
            }
        }
        return x;
    }
}

float KeyMatcher::loudness (const std::vector<float>& sound, int key, int offsetIndex) const
{
    if (fp == nullptr || key < fp->lowKey || key > fp->highKey() || (int) sound.size() != fp->bins) return 0.0f;
    const float* t = fp->at (key - fp->lowKey, offsetIndex);
    double ty = 0.0, tt = 0.0;
    for (int b = 0; b < fp->bins; ++b) { ty += (double) t[b] * sound[(std::size_t) b]; tt += (double) t[b] * t[b]; }
    return tt > 0.0 ? (float) (ty / tt) : 0.0f;
}

KeyMatch KeyMatcher::match (const std::vector<float>& y, int o, float level) const
{
    KeyMatch result;
    if (fp == nullptr || (int) y.size() != fp->bins || o < 0 || o >= (int) fp->offsets.size())
        return result;
    const int m = fp->bins, keys = fp->keyCount;
    const double total = std::accumulate (y.begin(), y.end(), 0.0);
    const float loudestBin = *std::max_element (y.begin(), y.end());
    if (total <= 0.0 || loudestBin <= 0.0f)
        return result;
    level = std::clamp (level, 0.05f, 20.0f);

    std::vector<std::pair<double, int>> match;
    for (int k = 0; k < keys; ++k)
    {
        if (fp->known[(std::size_t) k] == 0) continue;
        const float* t = fp->at (k, o);
        double ty = 0.0, tt = 0.0;
        for (int b = 0; b < m; ++b) { ty += (double) t[b] * y[(std::size_t) b]; tt += (double) t[b] * t[b]; }
        if (tt > 0.0) match.push_back ({ ty / std::sqrt (tt), k });
    }
    std::sort (match.begin(), match.end(), [] (auto& a, auto& b) { return a.first > b.first; });
    if (match.size() > (std::size_t) kPrefilter) match.resize ((std::size_t) kPrefilter);
    const int n = (int) match.size();
    if (n == 0) return result;

    std::vector<const float*> cols;
    for (const auto& c : match) cols.push_back (fp->at (c.second, o));

    std::vector<double> weight ((std::size_t) m, 1.0), G ((std::size_t) (n * n)), h ((std::size_t) n), g;
    const double floor = kFloor * loudestBin;
    for (int it = 0; it < kIterations; ++it)
    {
        for (int i = 0; i < n; ++i)
        {
            double hi = 0.0;
            for (int b = 0; b < m; ++b) hi += weight[(std::size_t) b] * cols[(std::size_t) i][b] * y[(std::size_t) b];
            h[(std::size_t) i] = hi;
            for (int j = i; j < n; ++j)
            {
                double gij = 0.0;
                for (int b = 0; b < m; ++b) gij += weight[(std::size_t) b] * cols[(std::size_t) i][b] * cols[(std::size_t) j][b];
                G[(std::size_t) (i * n + j)] = G[(std::size_t) (j * n + i)] = gij;
            }
        }
        g = nnls (G, h, n);
        for (int b = 0; b < m; ++b)
        {
            double fit = 0.0;
            for (int i = 0; i < n; ++i) fit += g[(std::size_t) i] * cols[(std::size_t) i][b];
            weight[(std::size_t) b] = 1.0 / std::max (std::abs (y[(std::size_t) b] - fit), floor);
        }
    }

    for (int i = 0; i < n; ++i)
        if (result.closest < 0 || g[(std::size_t) i] > g[(std::size_t) (result.closest)])
            result.closest = i;
    result.closest = result.closest >= 0 ? fp->lowKey + match[(std::size_t) result.closest].second : -1;

    std::vector<int> pool;
    {
        std::vector<int> order ((std::size_t) n);
        std::iota (order.begin(), order.end(), 0);
        std::sort (order.begin(), order.end(), [&] (int a, int b) { return g[(std::size_t) a] > g[(std::size_t) b]; });
        for (int i = 0; i < std::min (n, kPool); ++i)
            if (g[(std::size_t) order[(std::size_t) i]] > kPoolMin * level)
            {
                const int k = match[(std::size_t) order[(std::size_t) i]].second;
                for (int c : { k, k - 12, k + 12 })
                    if (c >= 0 && c < keys && fp->known[(std::size_t) c] != 0 && std::find (pool.begin(), pool.end(), c) == pool.end())
                        pool.push_back (c);
            }
    }

    std::vector<double> sum ((std::size_t) m), big ((std::size_t) m);
    auto cost = [&] (const std::vector<int>& set)
    {
        if (set.empty()) return 1.0;
        std::fill (sum.begin(), sum.end(), 0.0);
        std::fill (big.begin(), big.end(), 0.0);
        for (int k : set)
        {
            const float* t = fp->at (k, o);
            for (int b = 0; b < m; ++b)
            {
                const double v = level * t[b];
                sum[(std::size_t) b] += v;
                big[(std::size_t) b] = std::max (big[(std::size_t) b], v);
            }
        }
        double off = 0.0;
        for (int b = 0; b < m; ++b)
        {
            const double hiB = kLoudest * sum[(std::size_t) b];
            const double loB = std::max (0.0, kQuietest * big[(std::size_t) b] - kLoudest * (sum[(std::size_t) b] - big[(std::size_t) b]));
            const double v = y[(std::size_t) b];
            off += std::max (loB - v, 0.0) + std::max (v - hiB, 0.0);
        }
        return off / total + kKeyCost * (double) set.size();
    };

    std::vector<int> chosen;
    double best = cost (chosen);
    for (bool improved = true; improved;)
    {
        improved = false;
        std::vector<int> bestSet = chosen;
        auto consider = [&] (const std::vector<int>& set)
        {
            const double c = cost (set);
            if (c < best - 1.0e-9) { best = c; bestSet = set; improved = true; }
        };
        for (int c : pool)
        {
            if (std::find (chosen.begin(), chosen.end(), c) != chosen.end()) continue;
            if ((int) chosen.size() < kMaxKeys)
            {
                auto s = chosen; s.push_back (c); consider (s);
            }
            for (std::size_t r = 0; r < chosen.size(); ++r)
            {
                auto s = chosen; s[r] = c; consider (s);
            }
        }
        for (std::size_t r = 0; r < chosen.size(); ++r)
        {
            auto s = chosen; s.erase (s.begin() + (long) r); consider (s);
        }
        chosen = bestSet;
    }

    std::sort (chosen.begin(), chosen.end());
    result.cost = (float) best;
    for (int k : chosen)
    {
        result.keys.push_back (fp->lowKey + k);
        float gain = 0.0f;
        for (int i = 0; i < n; ++i)
            if (match[(std::size_t) i].second == k) gain = (float) g[(std::size_t) i];
        result.gains.push_back (gain / level);
    }
    return result;
}

}
