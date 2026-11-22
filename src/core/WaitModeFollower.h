#pragma once

#include "core/Score.h"

#include <cstddef>
#include <vector>

namespace piano
{
enum class ChordRule
{
    All,
    Highest,
    Lowest
};

struct FollowerOptions
{
    double    groupWindow    = 0.04;
    ChordRule chordRule      = ChordRule::All;
    bool      octaveTolerant = false;
    double    strikeWindow   = 0.15;
    bool      buildChords    = false;
    bool      holdNotes      = true;

    double    gridBeats      = 0.5;
};

struct Step
{
    double              time = 0.0;
    std::vector<int>    pitches;
    std::vector<int>    required;
    std::vector<double> durations;
    std::vector<double> holdFor;
    std::vector<size_t> noteIndices;
};

struct Hold
{
    int    pitch = 0;
    double start = 0.0, end = 0.0;
    double remaining = 0.0;
    bool   released = false;
};

class WaitModeFollower
{
public:
    enum class Result
    {
        Ignored,
        Correct,
        StepComplete,
        Finished,
        Resumed,
        Wrong
    };

    void load (const Score& score, const std::vector<int>& tracks, const FollowerOptions& options = {});
    void restart();
    void seek (size_t stepIndex);

    Result noteOn (int pitch, double when);
    void   noteOff (int pitch, double when);

    Result notePlayed (int pitch) { return noteOn (pitch, lastEventTime); }

    void advance (double seconds, double speed = 1.0);

    enum class Waiting { No, ForChord, ForHold };
    Waiting waiting() const noexcept;

    bool   finished() const noexcept     { return index >= steps.size() && holds.empty(); }
    size_t currentIndex() const noexcept { return index; }
    size_t stepCount() const noexcept    { return steps.size(); }
    const Step* currentStep() const noexcept { return index < steps.size() ? &steps[index] : nullptr; }
    const std::vector<Step>& allSteps() const noexcept { return steps; }
    const std::vector<Hold>& activeHolds() const noexcept { return holds; }

    const std::vector<int>& matchedPitches() const noexcept { return matched; }

    std::vector<int> chordPitches() const;
    std::vector<int> heldPitches() const;

    double currentTime() const noexcept { return time; }

    int correctCount() const noexcept { return correct; }
    int wrongCount() const noexcept   { return wrong; }
    int earlyReleaseCount() const noexcept { return earlyReleases; }

    const FollowerOptions& options() const noexcept { return opts; }

private:
    bool matches (int played, int expected) const noexcept;

    FollowerOptions   opts;
    std::vector<Step> steps;
    std::vector<Hold> holds;
    std::vector<int>  matched;
    std::vector<double> matchedTimes;
    size_t index = 0;
    double time = 0.0, endTime = 0.0, lastEventTime = 0.0;
    int correct = 0, wrong = 0, earlyReleases = 0;
};

}
