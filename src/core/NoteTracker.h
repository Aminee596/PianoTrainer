#pragma once

#include "core/KeyAttacks.h"

#include <vector>

namespace piano
{
class NoteTracker
{
public:
    void prepare (double sampleRate, const ListenerOptions& options = {});
    void reset();

    void setWatched (const std::vector<int>& chord, const std::vector<int>& held);

    void process (const float* samples, int numSamples, std::vector<NoteEvent>& out,
                  std::vector<NoteEvent>* rejected = nullptr);

    ListenerState state() const noexcept { return attacks.state(); }
    double latencySeconds() const noexcept { return attacks.latencySeconds(); }
    const KeyAttacks& keyAttacks() const noexcept { return attacks; }

    void setFingerprints (std::shared_ptr<const KeyFingerprints> f) { attacks.setFingerprints (std::move (f)); }
    void setCapture (bool on) noexcept { attacks.setCapture (on); }
    std::vector<CapturedStrike> takeCaptured() { return attacks.takeCaptured(); }

private:
    KeyAttacks attacks;
};

}
