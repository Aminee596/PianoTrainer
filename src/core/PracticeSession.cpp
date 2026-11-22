#include "core/PracticeSession.h"

#include <algorithm>
#include <cmath>

namespace piano
{
namespace
{
    bool has (const std::vector<int>& v, int x) { return std::find (v.begin(), v.end(), x) != v.end(); }

    std::string join (const std::vector<int>& pitches)
    {
        std::string s;
        for (int p : pitches)
            s += (s.empty() ? "" : " + ") + noteName (p);
        return s;
    }
}

void PracticeSession::load (Score newScore)
{
    score_ = std::move (newScore);

    std::vector<std::pair<double, int>> byPitch;
    for (std::size_t t = 0; t < score_.tracks.size(); ++t)
    {
        if (score_.tracks[t].noteCount == 0) continue;
        double sum = 0.0;
        for (const auto& n : score_.notes)
            if (n.track == (int) t) sum += n.pitch;
        byPitch.emplace_back (sum / score_.tracks[t].noteCount, (int) t);
    }
    std::sort (byPitch.begin(), byPitch.end());

    rightTrack = byPitch.empty() ? -1 : byPitch.back().second;
    leftTrack  = byPitch.empty() ? -1 : byPitch.front().second;

    leftHanded.assign (score_.tracks.size(), false);
    for (const auto& [avg, t] : byPitch)
        leftHanded[(std::size_t) t] = (t == leftTrack && leftTrack != rightTrack) || (t != rightTrack && avg < 60.0);

    hand_ = canChooseHand() ? Hand::Right : Hand::Both;
    rebuild (false);
}

void PracticeSession::setHand (Hand h)
{
    if (! canChooseHand()) h = Hand::Both;
    if (h == hand_) return;

    const auto* current = follower_.currentStep();
    const bool atEnd = current == nullptr && ! follower_.allSteps().empty();
    const double at = current != nullptr ? current->time : follower_.currentTime();
    const int streak = streak_;

    hand_ = h;
    rebuild (false);
    streak_ = streak;
    if (atEnd) follower_.seek (follower_.allSteps().size());
    else       seekToTime (at);
}

bool PracticeSession::isLeftHandTrack (int track) const
{
    return track >= 0 && (std::size_t) track < leftHanded.size() && leftHanded[(std::size_t) track];
}

void PracticeSession::setOptions (const SessionOptions& o)
{
    opts = o;
    rebuild (true);
}

void PracticeSession::rebuild (bool keepPosition)
{
    followed.clear();
    if (hand_ == Hand::Left && leftTrack >= 0)   followed.push_back (leftTrack);
    if (hand_ == Hand::Right && rightTrack >= 0) followed.push_back (rightTrack);

    FollowerOptions f;
    f.octaveTolerant = opts.anyOctave;
    f.strikeWindow   = opts.strikeWindow;
    f.buildChords    = opts.buildChords;
    f.holdNotes      = opts.holdNotes;
    f.gridBeats      = opts.gridBeats;
    switch (opts.chords)
    {
        case ChordChoice::Top:    f.chordRule = ChordRule::Highest; break;
        case ChordChoice::Bottom: f.chordRule = ChordRule::Lowest;  break;
        case ChordChoice::All:
        case ChordChoice::Auto:   f.chordRule = ChordRule::All;     break;
    }

    const auto position = follower_.currentIndex();
    follower_.load (score_, followed, f);
    lastEarlyReleases = 0;
    if (keepPosition)
        follower_.seek (position);
    else
        streak_ = 0;
}

WaitModeFollower::Result PracticeSession::noteOn (int detectedPitch, double when)
{
    lastWhen = when;
    const auto result = follower_.noteOn (detectedPitch + 12 * opts.inputShift, when);
    switch (result)
    {
        case WaitModeFollower::Result::Correct:
        case WaitModeFollower::Result::StepComplete:
        case WaitModeFollower::Result::Finished: ++streak_; break;
        case WaitModeFollower::Result::Wrong:    streak_ = 0; break;
        case WaitModeFollower::Result::Resumed:
        case WaitModeFollower::Result::Ignored:  break;
    }
    return result;
}

void PracticeSession::noteOff (int detectedPitch, double when)
{
    lastWhen = when;
    follower_.noteOff (detectedPitch + 12 * opts.inputShift, when);
    if (follower_.earlyReleaseCount() != lastEarlyReleases)
    {
        lastEarlyReleases = follower_.earlyReleaseCount();
        streak_ = 0;
    }
}

void PracticeSession::advance (double seconds)
{
    follower_.advance (seconds);
}

PracticeSession::Watch PracticeSession::watched() const
{
    Watch w { follower_.chordPitches(), follower_.heldPitches() };
    for (auto* list : { &w.chord, &w.held })
        for (auto& p : *list) p -= 12 * opts.inputShift;
    return w;
}

void PracticeSession::restart()
{
    follower_.restart();
    streak_ = 0;
    lastEarlyReleases = 0;
}

void PracticeSession::step (int delta)
{
    const auto index = (long) follower_.currentIndex() + delta;
    follower_.seek ((std::size_t) std::max (0L, index));
}

void PracticeSession::seekToTime (double seconds)
{
    const auto& steps = follower_.allSteps();
    const auto it = std::lower_bound (steps.begin(), steps.end(), seconds - 1.0e-4,
                                      [] (const Step& s, double t) { return s.time < t; });
    const auto index = (std::size_t) (it - steps.begin());
    if (index != follower_.currentIndex())
        follower_.seek (index);
}

int PracticeSession::accuracyPercent() const noexcept
{
    const int c = follower_.correctCount(), w = follower_.wrongCount() + follower_.earlyReleaseCount();
    return c + w > 0 ? (int) std::lround (100.0 * c / (c + w)) : -1;
}

std::vector<int> PracticeSession::remaining() const
{
    std::vector<int> r;
    if (const auto* s = follower_.currentStep())
        for (int p : s->required)
            if (! has (follower_.matchedPitches(), p))
                r.push_back (p);
    return r;
}

std::vector<int> PracticeSession::chordOthers() const
{
    std::vector<int> r;
    if (const auto* s = follower_.currentStep())
        for (int p : s->pitches)
            if (! has (s->required, p))
                r.push_back (p);
    return r;
}

int PracticeSession::barCount() const
{
    if (score_.barStarts.empty() || score_.empty()) return 0;
    const double end = score_.lengthSeconds() - 1.0e-3;
    return (int) std::count_if (score_.barStarts.begin(), score_.barStarts.end(), [end] (double t) { return t < end; });
}

int PracticeSession::currentBar() const
{
    const int bars = barCount();
    if (bars == 0) return 0;
    return std::clamp (score_.barAt (follower_.currentTime()) + 1, 1, bars);
}

PracticeSession::Prompt PracticeSession::prompt() const
{
    using K = Prompt::Kind;
    if (! hasPiece())
        return { "GET STARTED", "Open a piece", "Drop a MIDI file on this window, or click Open piece", K::GetStarted };
    if (follower_.finished())
        return { "FINISHED", "Well played", "Press R or Restart to play it again", K::Finished };

    std::vector<int> released;
    for (const auto& h : follower_.activeHolds())
        if (h.released) released.push_back (h.pitch);
    if (! released.empty())
        return { "KEEP HOLDING", join (released), "Press it again and hold it until the note ends", K::KeepHolding };

    const auto* s = follower_.currentStep();
    if (s == nullptr)
        return { "HOLD", "", "Hold the last notes until they end", K::KeepHolding };

    if (s->required.size() > 1)
        return { "PLAY TOGETHER", join (remaining()), "Strike all of them at the same time", K::PlayTogether };

    std::string detail = "The piece waits until you play it";
    if (s->pitches.size() > s->required.size())
        detail = follower_.options().chordRule == ChordRule::Lowest ? "Bottom note of the chord" : "Top note of the chord";
    else if (! follower_.activeHolds().empty())
        detail = "Keep holding " + join ([this] { std::vector<int> p; for (const auto& h : follower_.activeHolds()) p.push_back (h.pitch); return p; }()) + " until it ends";
    return { "PLAY NOW", join (remaining()), detail, K::PlayNow };
}

}
