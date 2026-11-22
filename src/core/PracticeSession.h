#pragma once

#include "core/Score.h"
#include "core/WaitModeFollower.h"

#include <string>
#include <vector>

namespace piano
{
enum class Hand { Left = 0, Right = 1, Both = 2 };
enum class ChordChoice { Auto = 1, Top, Bottom, All };

struct SessionOptions
{
    ChordChoice chords     = ChordChoice::Auto;
    bool        anyOctave  = false;
    int         inputShift = 0;
    double      strikeWindow = 0.15;
    bool        buildChords = false;
    bool        holdNotes  = true;
    double      gridBeats  = 0.5;
};

class PracticeSession
{
public:
    void load (Score newScore);
    bool hasPiece() const noexcept          { return ! score_.empty(); }
    const Score& score() const noexcept     { return score_; }
    const WaitModeFollower& follower() const noexcept { return follower_; }

    bool canChooseHand() const noexcept     { return rightTrack != leftTrack && rightTrack >= 0; }
    Hand hand() const noexcept              { return hand_; }
    void setHand (Hand h);
    bool isLeftHandTrack (int track) const;
    const std::vector<int>& followedTracks() const noexcept { return followed; }

    const SessionOptions& options() const noexcept { return opts; }
    void setOptions (const SessionOptions& o);

    WaitModeFollower::Result noteOn (int detectedPitch, double when);
    void noteOff (int detectedPitch, double when);
    WaitModeFollower::Result play (int detectedPitch) { return noteOn (detectedPitch, lastWhen); }

    void advance (double seconds);
    void restart();
    void step (int delta);

    void seekToTime (double seconds);

    struct Watch { std::vector<int> chord, held; };
    Watch watched() const;

    int streak() const noexcept { return streak_; }
    int accuracyPercent() const noexcept;
    int earlyReleases() const noexcept { return follower_.earlyReleaseCount(); }

    std::vector<int> remaining() const;
    std::vector<int> chordOthers() const;

    int currentBar() const;
    int barCount() const;

    struct Prompt { std::string caption, big, detail; enum class Kind { GetStarted, PlayNow, PlayTogether, KeepHolding, Finished } kind; };
    Prompt prompt() const;

private:
    void rebuild (bool keepPosition);

    Score score_;
    WaitModeFollower follower_;
    SessionOptions opts;
    Hand hand_ = Hand::Right;
    int rightTrack = -1, leftTrack = -1;
    std::vector<int> followed;
    std::vector<bool> leftHanded;
    int streak_ = 0;
    int lastEarlyReleases = 0;
    double lastWhen = 0.0;
};

}
