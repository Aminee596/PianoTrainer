#include "core/NoteTracker.h"

namespace piano
{
void NoteTracker::prepare (double sampleRate, const ListenerOptions& options)
{
    attacks.prepare (sampleRate, options);
}

void NoteTracker::reset()
{
    attacks.reset();
}

void NoteTracker::setWatched (const std::vector<int>& chord, const std::vector<int>& held)
{
    attacks.setExpected (chord, held);
}

void NoteTracker::process (const float* samples, int numSamples, std::vector<NoteEvent>& out,
                           std::vector<NoteEvent>* rejected)
{
    attacks.process (samples, numSamples, out, rejected);
}

}
