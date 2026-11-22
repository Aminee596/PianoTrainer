#include "core/WaitModeFollower.h"

#include <algorithm>
#include <cmath>

namespace piano
{
void WaitModeFollower::load (const Score& score, const std::vector<int>& tracks, const FollowerOptions& options)
{
    opts = options;
    steps.clear();
    endTime = 0.0;

    auto followed = [&] (int track)
    {
        return tracks.empty() || std::find (tracks.begin(), tracks.end(), track) != tracks.end();
    };

    for (size_t i = 0; i < score.notes.size(); ++i)
    {
        const auto& note = score.notes[i];
        if (! followed (note.track))
            continue;

        endTime = std::max (endTime, note.start + note.duration);
        if (steps.empty() || note.start - steps.back().time > opts.groupWindow)
        {
            steps.emplace_back();
            steps.back().time = note.start;
        }

        auto& step = steps.back();
        if (std::find (step.pitches.begin(), step.pitches.end(), note.pitch) == step.pitches.end())
            step.pitches.push_back (note.pitch);
        step.noteIndices.push_back (i);
    }

    for (auto& step : steps)
    {
        std::sort (step.pitches.begin(), step.pitches.end());
        switch (opts.chordRule)
        {
            case ChordRule::All:     step.required = step.pitches;             break;
            case ChordRule::Highest: step.required = { step.pitches.back() };  break;
            case ChordRule::Lowest:  step.required = { step.pitches.front() }; break;
        }
        for (int p : step.required)
        {
            double d = 0.0, hold = 0.0;
            for (auto i : step.noteIndices)
                if (const auto& n = score.notes[i]; n.pitch == p)
                {
                    d = std::max (d, n.duration);

                    const double grid = std::max (0.0625, opts.gridBeats);
                    const double steps = std::max (1.0, std::round (n.lengthInBeats() / grid));
                    if (steps >= 2.0)
                        hold = std::max (hold, std::min (n.duration, (steps - 0.5) * grid * n.secondsPerBeat));
                }
            step.durations.push_back (d);
            step.holdFor.push_back (hold);
        }
    }

    restart();
}

void WaitModeFollower::restart()
{
    correct = wrong = earlyReleases = 0;
    seek (0);
}

void WaitModeFollower::seek (size_t stepIndex)
{
    index = std::min (stepIndex, steps.size());
    time = index < steps.size() ? steps[index].time : endTime;
    holds.clear();
    matched.clear();
    matchedTimes.clear();
}

bool WaitModeFollower::matches (int played, int expected) const noexcept
{
    if (played == expected)
        return true;
    return opts.octaveTolerant && ((played - expected) % 12 + 12) % 12 == 0;
}

WaitModeFollower::Waiting WaitModeFollower::waiting() const noexcept
{
    for (const auto& h : holds)
        if (h.released) return Waiting::ForHold;
    if (index < steps.size() && time >= steps[index].time - 1.0e-9)
        return Waiting::ForChord;
    return Waiting::No;
}

std::vector<int> WaitModeFollower::chordPitches() const
{
    if (const auto* s = currentStep())
        return s->required;
    return {};
}

std::vector<int> WaitModeFollower::heldPitches() const
{
    std::vector<int> w;
    for (const auto& h : holds)
        w.push_back (h.pitch);
    return w;
}

WaitModeFollower::Result WaitModeFollower::noteOn (int pitch, double when)
{
    lastEventTime = when;

    for (auto& h : holds)
        if (h.released && matches (pitch, h.pitch))
        {
            h.released = false;
            return Result::Resumed;
        }

    if (const auto* step = currentStep())
    {
        for (int expected : step->required)
        {
            if (! matches (pitch, expected))
                continue;
            if (std::find (matched.begin(), matched.end(), expected) != matched.end())
                continue;

            for (size_t i = matched.size(); i-- > 0;)
                if (! opts.buildChords && when - matchedTimes[i] > opts.strikeWindow)
                {
                    matched.erase (matched.begin() + (long) i);
                    matchedTimes.erase (matchedTimes.begin() + (long) i);
                }

            matched.push_back (expected);
            matchedTimes.push_back (when);
            ++correct;

            if (matched.size() < step->required.size())
                return Result::Correct;

            holds.erase (std::remove_if (holds.begin(), holds.end(), [] (const Hold& h) { return h.released; }),
                         holds.end());

            if (opts.holdNotes)
                for (size_t i = 0; i < step->required.size(); ++i)
                    if (step->holdFor[i] > 0.0)
                        holds.push_back ({ step->required[i], step->time, step->time + step->durations[i],
                                           step->holdFor[i], false });

            time = std::max (time, step->time);

            ++index;
            matched.clear();
            matchedTimes.clear();
            return finished() ? Result::Finished : Result::StepComplete;
        }

        for (int expected : matched)
            if (matches (pitch, expected))
                return Result::Ignored;
    }

    for (const auto& h : holds)
        if (matches (pitch, h.pitch))
            return Result::Ignored;

    ++wrong;
    return Result::Wrong;
}

void WaitModeFollower::noteOff (int pitch, double when)
{
    lastEventTime = when;

    for (auto& h : holds)
        if (! h.released && matches (pitch, h.pitch) && h.remaining > 0.0)
        {
            h.released = true;
            ++earlyReleases;
        }

    for (size_t i = matched.size(); i-- > 0;)
        if (matches (pitch, matched[i]))
        {
            matched.erase (matched.begin() + (long) i);
            matchedTimes.erase (matchedTimes.begin() + (long) i);
        }
}

void WaitModeFollower::advance (double seconds, double speed)
{
    const bool blocked = std::any_of (holds.begin(), holds.end(), [] (const Hold& h) { return h.released; });
    if (! blocked)
    {
        const double limit = index < steps.size() ? steps[index].time : endTime;
        time = std::min (time + seconds * speed, std::max (time, limit));
    }

    for (auto& h : holds)
        if (! h.released)
            h.remaining -= seconds * speed;
    holds.erase (std::remove_if (holds.begin(), holds.end(), [] (const Hold& h)
                 {
                     return ! h.released && h.remaining <= 0.0;
                 }),
                 holds.end());
}

}
