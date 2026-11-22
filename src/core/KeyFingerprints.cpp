#include "core/KeyFingerprints.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace piano
{
namespace
{
    constexpr char kMagic[4] = { 'P', 'T', 'F', 'P' };
    constexpr std::int32_t kVersion = 2;
    constexpr double kBounceSeconds = 0.25;

    template <typename T>
    void put (std::vector<std::uint8_t>& out, const T& value)
    {
        const auto* p = reinterpret_cast<const std::uint8_t*> (&value);
        out.insert (out.end(), p, p + sizeof (T));
    }

    struct Reader
    {
        const std::uint8_t* p;
        std::size_t left;

        template <typename T>
        bool get (T& value)
        {
            if (left < sizeof (T)) return false;
            std::memcpy (&value, p, sizeof (T));
            p += sizeof (T);
            left -= sizeof (T);
            return true;
        }
    };
}

std::shared_ptr<KeyFingerprints> KeyFingerprints::empty (const CapturedStrike& layout, int low, int count)
{
    auto f = std::make_shared<KeyFingerprints>();
    f->rate = layout.rate;
    f->fftSize = layout.fftSize;
    f->bin0 = layout.bin0;
    f->bins = layout.bins;
    f->lowKey = low;
    f->keyCount = count;
    f->offsets = layout.offsets;
    f->magnitudes.assign ((std::size_t) count * f->offsets.size() * (std::size_t) f->bins, 0.0f);
    f->known.assign ((std::size_t) count, 0);
    return f;
}

bool KeyFingerprints::valid() const noexcept
{
    return rate > 0.0 && fftSize > 0 && bins > 0 && keyCount > 0 && ! offsets.empty()
        && magnitudes.size() == (std::size_t) keyCount * offsets.size() * (std::size_t) bins
        && known.size() == (std::size_t) keyCount;
}

bool KeyFingerprints::complete() const noexcept
{
    return valid() && knownCount() == keyCount;
}

int KeyFingerprints::knownCount() const noexcept
{
    int n = 0;
    for (auto k : known) n += k != 0 ? 1 : 0;
    return n;
}

bool KeyFingerprints::isKnown (int pitch) const noexcept
{
    const int i = pitch - lowKey;
    return i >= 0 && i < (int) known.size() && known[(std::size_t) i] != 0;
}

bool KeyFingerprints::suits (double sampleRate, int size) const noexcept
{
    return valid() && std::abs (sampleRate - rate) < 0.5 && size == fftSize;
}

const float* KeyFingerprints::at (int keyIndex, int offsetIndex) const noexcept
{
    return magnitudes.data() + ((std::size_t) keyIndex * offsets.size() + (std::size_t) offsetIndex) * (std::size_t) bins;
}

float* KeyFingerprints::at (int keyIndex, int offsetIndex) noexcept
{
    return magnitudes.data() + ((std::size_t) keyIndex * offsets.size() + (std::size_t) offsetIndex) * (std::size_t) bins;
}

int KeyFingerprints::nearestOffset (double seconds) const noexcept
{
    int best = 0;
    for (int i = 1; i < (int) offsets.size(); ++i)
        if (std::abs (offsets[(std::size_t) i] - seconds) < std::abs (offsets[(std::size_t) best] - seconds))
            best = i;
    return best;
}

std::shared_ptr<KeyFingerprints> KeyFingerprints::convertedTo (double sampleRate, int size, int firstBin, int binCount) const
{
    if (! valid() || sampleRate <= 0.0 || size <= 0 || binCount <= 0)
        return nullptr;
    auto f = std::make_shared<KeyFingerprints> (*this);
    f->rate = sampleRate;
    f->fftSize = size;
    f->bin0 = firstBin;
    f->bins = binCount;
    f->magnitudes.assign ((std::size_t) keyCount * offsets.size() * (std::size_t) binCount, 0.0f);
    const float scale = (float) size / (float) fftSize;
    for (int k = 0; k < keyCount; ++k)
        for (int o = 0; o < (int) offsets.size(); ++o)
        {
            const float* from = at (k, o);
            float* to = f->at (k, o);
            for (int b = 0; b < binCount; ++b)
            {
                const double hz = (firstBin + b) * sampleRate / size;
                const double x = hz * fftSize / rate - bin0;
                const int i = (int) std::floor (x);
                if (i < 0 || i + 1 >= bins) continue;
                const double t = x - i;
                to[b] = scale * (float) ((1.0 - t) * from[i] + t * from[i + 1]);
            }
        }
    return f;
}

std::vector<std::uint8_t> KeyFingerprints::serialise() const
{
    std::vector<std::uint8_t> out (kMagic, kMagic + 4);
    put (out, kVersion);
    put (out, rate);
    for (const std::int32_t v : { fftSize, bin0, bins, lowKey, keyCount, (int) offsets.size(), (int) created.size() })
        put (out, v);
    for (const double o : offsets) put (out, o);
    out.insert (out.end(), created.begin(), created.end());
    const auto* m = reinterpret_cast<const std::uint8_t*> (magnitudes.data());
    out.insert (out.end(), m, m + magnitudes.size() * sizeof (float));
    out.insert (out.end(), known.begin(), known.end());
    return out;
}

bool KeyFingerprints::deserialise (const void* data, std::size_t size, KeyFingerprints& out)
{
    Reader r { static_cast<const std::uint8_t*> (data), size };
    char magic[4] {};
    std::int32_t version = 0, fft = 0, b0 = 0, nb = 0, low = 0, count = 0, nOffsets = 0, nCreated = 0;
    KeyFingerprints f;
    if (! r.get (magic) || std::memcmp (magic, kMagic, 4) != 0 || ! r.get (version) || version < 1 || version > kVersion)
        return false;
    if (! r.get (f.rate) || ! r.get (fft) || ! r.get (b0) || ! r.get (nb) || ! r.get (low) || ! r.get (count)
        || ! r.get (nOffsets) || ! r.get (nCreated))
        return false;
    if (fft <= 0 || nb <= 0 || count <= 0 || count > 128 || nOffsets <= 0 || nOffsets > 64 || nCreated < 0 || nCreated > 256)
        return false;
    f.fftSize = fft; f.bin0 = b0; f.bins = nb; f.lowKey = low; f.keyCount = count;
    f.offsets.resize ((std::size_t) nOffsets);
    for (auto& o : f.offsets)
        if (! r.get (o)) return false;
    if (r.left < (std::size_t) nCreated) return false;
    f.created.assign (reinterpret_cast<const char*> (r.p), (std::size_t) nCreated);
    r.p += nCreated;
    r.left -= (std::size_t) nCreated;
    const std::size_t n = (std::size_t) count * (std::size_t) nOffsets * (std::size_t) nb;
    const std::size_t knownBytes = version >= 2 ? (std::size_t) count : 0;
    if (r.left != n * sizeof (float) + knownBytes) return false;
    f.magnitudes.resize (n);
    std::memcpy (f.magnitudes.data(), r.p, n * sizeof (float));
    if (version >= 2)
        f.known.assign (r.p + n * sizeof (float), r.p + n * sizeof (float) + count);
    else
        f.known.assign ((std::size_t) count, 1);
    if (! f.valid()) return false;
    out = std::move (f);
    return true;
}

void KeyCalibration::start (int low, int count)
{
    lowKey = low;
    keyCount = count;
    strikes.clear();
    running = count > 0;
}

bool KeyCalibration::add (const CapturedStrike& strike)
{
    if (! running || isComplete() || strike.spectra.empty())
        return false;
    if (! strikes.empty())
    {
        const auto& last = strikes.back();
        if (strike.time >= last.time && strike.time - last.time < kBounceSeconds)
            return false;
        if (std::abs (strike.rate - last.rate) > 0.5 || strike.fftSize != last.fftSize || strike.bin0 != last.bin0
            || strike.bins != last.bins || strike.offsets != last.offsets)
            return false;
    }
    strikes.push_back (strike);
    return true;
}

void KeyCalibration::back()
{
    if (! strikes.empty())
        strikes.pop_back();
}

std::shared_ptr<KeyFingerprints> KeyCalibration::build (const std::string& created) const
{
    if (! isComplete())
        return nullptr;
    auto f = std::make_shared<KeyFingerprints>();
    const auto& first = strikes.front();
    f->rate = first.rate;
    f->fftSize = first.fftSize;
    f->bin0 = first.bin0;
    f->bins = first.bins;
    f->lowKey = lowKey;
    f->keyCount = keyCount;
    f->offsets = first.offsets;
    f->created = created;
    f->magnitudes.assign ((std::size_t) keyCount * f->offsets.size() * (std::size_t) f->bins, 0.0f);
    f->known.assign ((std::size_t) keyCount, 1);
    for (int k = 0; k < keyCount; ++k)
        for (int o = 0; o < (int) f->offsets.size(); ++o)
        {
            const auto& s = strikes[(std::size_t) k].spectra[(std::size_t) o];
            std::copy_n (s.begin(), std::min ((int) s.size(), f->bins), f->at (k, o));
        }
    return f;
}

}
