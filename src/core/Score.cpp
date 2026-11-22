#include "core/Score.h"

#include <algorithm>
#include <cmath>

namespace piano
{
void Score::clear()
{
    title.clear();
    notes.clear();
    tracks.clear();
    barStarts.clear();
}

int Score::barAt (double seconds) const
{
    if (barStarts.empty())
        return -1;
    const auto it = std::upper_bound (barStarts.begin(), barStarts.end(), seconds + 1.0e-6);
    return std::max (0, (int) (it - barStarts.begin()) - 1);
}

void Score::finalise()
{
    std::sort (notes.begin(), notes.end(), [] (const Note& a, const Note& b)
    {
        if (a.start != b.start) return a.start < b.start;
        return a.pitch < b.pitch;
    });

    for (auto& t : tracks)
        t = { t.name, 0, 127, 0 };

    for (const auto& n : notes)
    {
        if (n.track < 0) continue;
        if (n.track >= (int) tracks.size())
            tracks.resize ((size_t) n.track + 1);

        auto& t = tracks[(size_t) n.track];
        ++t.noteCount;
        t.lowest  = std::min (t.lowest,  n.pitch);
        t.highest = std::max (t.highest, n.pitch);
    }

    rests.clear();
    struct Running { double end = 0.0, endBeat = 0.0; bool started = false; };
    std::vector<Running> running (tracks.size());
    for (const auto& n : notes)
    {
        if (n.track < 0) continue;
        auto& r = running[(size_t) n.track];
        const double gap = n.start - r.end;
        const double gapBeats = n.startBeat >= 0.0 && r.started ? n.startBeat - r.endBeat : gap / n.secondsPerBeat;
        if (r.started && gapBeats >= 0.125 - 1.0e-6)
            rests.push_back ({ n.track, r.end, gap, gapBeats });

        r.started = true;
        if (n.start + n.duration > r.end)
        {
            r.end = n.start + n.duration;
            r.endBeat = n.startBeat >= 0.0 ? n.startBeat + n.lengthInBeats() : r.endBeat + n.lengthInBeats();
        }
    }
    std::sort (rests.begin(), rests.end(), [] (const Rest& a, const Rest& b) { return a.start < b.start; });
}

double Score::lengthSeconds() const
{
    double end = 0.0;
    for (const auto& n : notes)
        end = std::max (end, n.start + n.duration);
    return end;
}

std::string noteName (int midiPitch)
{
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const int octave = midiPitch / 12 - 1;
    return std::string (names[((midiPitch % 12) + 12) % 12]) + std::to_string (octave);
}

double pitchToFrequency (double midiPitch)
{
    return 440.0 * std::pow (2.0, (midiPitch - 69.0) / 12.0);
}

double frequencyToPitch (double hz)
{
    return 69.0 + 12.0 * std::log2 (hz / 440.0);
}

}
