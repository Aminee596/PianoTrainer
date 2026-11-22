#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace piano
{
struct CapturedStrike;

struct KeyFingerprints
{
    static constexpr double kLowHz = 40.0, kHighHz = 5000.0;

    double rate = 0.0;
    int fftSize = 0;
    int bin0 = 0, bins = 0;
    int lowKey = 0, keyCount = 0;
    std::vector<double> offsets;
    std::vector<float> magnitudes;
    std::vector<std::uint8_t> known;
    std::string created;

    static std::shared_ptr<KeyFingerprints> empty (const CapturedStrike& layout, int lowKey, int keyCount);

    bool valid() const noexcept;
    bool complete() const noexcept;
    int knownCount() const noexcept;
    bool isKnown (int pitch) const noexcept;
    bool suits (double sampleRate, int size) const noexcept;
    int highKey() const noexcept { return lowKey + keyCount - 1; }
    const float* at (int keyIndex, int offsetIndex) const noexcept;
    float* at (int keyIndex, int offsetIndex) noexcept;
    int nearestOffset (double seconds) const noexcept;

    std::shared_ptr<KeyFingerprints> convertedTo (double sampleRate, int size, int firstBin, int binCount) const;

    std::vector<std::uint8_t> serialise() const;
    static bool deserialise (const void* data, std::size_t size, KeyFingerprints& out);
};

struct CapturedStrike
{
    double time = 0.0;
    float strength = 0.0f;
    double rate = 0.0;
    int fftSize = 0, bin0 = 0, bins = 0;
    std::vector<double> offsets;
    std::vector<std::vector<float>> spectra;
};

class KeyCalibration
{
public:
    void start (int lowKey, int keyCount);
    void cancel() { running = false; }

    bool isRunning() const noexcept { return running; }
    bool isComplete() const noexcept { return keyCount > 0 && (int) strikes.size() == keyCount; }
    int  index() const noexcept { return (int) strikes.size(); }
    int  total() const noexcept { return keyCount; }
    int  currentKey() const noexcept { return lowKey + index(); }

    bool add (const CapturedStrike& strike);
    void back();

    std::shared_ptr<KeyFingerprints> build (const std::string& created) const;

private:
    bool running = false;
    int lowKey = 0, keyCount = 0;
    std::vector<CapturedStrike> strikes;
};

}
