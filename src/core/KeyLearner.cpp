#include "core/KeyLearner.h"

#include <algorithm>
#include <cmath>

namespace piano
{
namespace
{
    constexpr double kSameStrike = 0.07;
    constexpr double kAlone = 0.15;
    constexpr double kAlike = 0.85;
    constexpr std::size_t kCandidates = 3;

    double similarity (const std::vector<float>& a, const std::vector<float>& b)
    {
        double ab = 0.0, aa = 0.0, bb = 0.0;
        for (std::size_t i = 0; i < std::min (a.size(), b.size()); ++i)
        {
            ab += (double) a[i] * b[i];
            aa += (double) a[i] * a[i];
            bb += (double) b[i] * b[i];
        }
        return aa > 0.0 && bb > 0.0 ? ab / std::sqrt (aa * bb) : 0.0;
    }

    bool sameLayout (const KeyFingerprints& f, const CapturedStrike& s)
    {
        return std::abs (f.rate - s.rate) < 0.5 && f.fftSize == s.fftSize && f.bin0 == s.bin0 && f.bins == s.bins && f.offsets == s.offsets;
    }
}

void KeyLearner::start (std::shared_ptr<const KeyFingerprints> existing, int low, int count)
{
    lowKey = low;
    keyCount = count;
    given = existing != nullptr && existing->valid() && existing->lowKey == low && existing->keyCount == count ? existing : nullptr;
    fp = nullptr;
    pending.clear();
    candidates.assign ((std::size_t) count, {});
    running = count > 0;
}

void KeyLearner::adoptLayout (const CapturedStrike& strike)
{
    if (fp != nullptr && sameLayout (*fp, strike))
        return;
    auto from = fp != nullptr ? std::shared_ptr<const KeyFingerprints> (fp) : given;
    fp = KeyFingerprints::empty (strike, lowKey, keyCount);
    for (auto& c : candidates) c.clear();
    if (from == nullptr)
        return;
    auto converted = sameLayout (*from, strike) ? std::make_shared<KeyFingerprints> (*from)
                                                : from->convertedTo (strike.rate, strike.fftSize, strike.bin0, strike.bins);
    if (converted == nullptr)
        return;
    for (int k = 0; k < keyCount; ++k)
    {
        if (converted->known[(std::size_t) k] == 0) continue;
        for (int o = 0; o < (int) fp->offsets.size(); ++o)
            std::copy_n (converted->at (k, converted->nearestOffset (fp->offsets[(std::size_t) o])), fp->bins, fp->at (k, o));
        fp->known[(std::size_t) k] = 1;
    }
    fp->created = converted->created;
}

void KeyLearner::addStrike (const CapturedStrike& strike)
{
    if (! running || strike.spectra.size() != strike.offsets.size() || strike.spectra.empty())
        return;
    adoptLayout (strike);
    if (fp->complete())
        return;
    pending.push_back (strike);
}

bool KeyLearner::hasStrikesIn (double from, double to) const
{
    return std::any_of (pending.begin(), pending.end(), [&] (const auto& s) { return s.time >= from && s.time < to; });
}

std::vector<int> KeyLearner::label (const std::vector<HeardNote>& notes, double from, double to)
{
    std::vector<int> learned;
    if (! running || fp == nullptr)
        return learned;

    for (const auto& s : pending)
    {
        if (s.time < from || s.time >= to) continue;
        int pitch = -1, near = 0, around = 0;
        for (const auto& n : notes)
        {
            if (n.pitch < 0) continue;
            const double d = std::abs (n.onset - s.time);
            if (d <= kSameStrike) { ++near; pitch = n.pitch; }
            if (d <= kAlone) ++around;
        }
        if (near != 1 || around != 1) continue;
        const int k = pitch - lowKey;
        if (k < 0 || k >= keyCount || fp->known[(std::size_t) k] != 0) continue;
        if (! sameLayout (*fp, s)) continue;

        auto& mine = candidates[(std::size_t) k];
        bool done = false;
        for (const auto& c : mine)
            if (similarity (c.spectra.back(), s.spectra.back()) >= kAlike)
            {
                for (int o = 0; o < (int) fp->offsets.size(); ++o)
                    for (int b = 0; b < fp->bins; ++b)
                        fp->at (k, o)[b] = 0.5f * (c.spectra[(std::size_t) o][(std::size_t) b] + s.spectra[(std::size_t) o][(std::size_t) b]);
                fp->known[(std::size_t) k] = 1;
                learned.push_back (pitch);
                mine.clear();
                done = true;
                break;
            }
        if (! done)
        {
            mine.push_back (s);
            if (mine.size() > kCandidates) mine.erase (mine.begin());
        }
    }
    pending.erase (std::remove_if (pending.begin(), pending.end(), [&] (const auto& s) { return s.time < to; }), pending.end());
    return learned;
}

std::vector<int> KeyLearner::missingKeys() const
{
    std::vector<int> missing;
    for (int k = 0; k < keyCount; ++k)
        if (fp != nullptr ? fp->known[(std::size_t) k] == 0 : given == nullptr || given->known[(std::size_t) k] == 0)
            missing.push_back (lowKey + k);
    return missing;
}

}
