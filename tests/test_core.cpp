#include "core/ChordDetector.h"
#include "core/MidiReader.h"
#include "core/KeyFingerprints.h"
#include "core/NoteTracker.h"
#include "core/NoteListener.h"
#include "core/PracticeSession.h"
#include "core/PitchDetector.h"
#include "core/Score.h"
#include "core/WaitModeFollower.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <functional>
#include <iterator>
#include <random>
#include <string>
#include <vector>

namespace
{
int failures = 0;

#define CHECK(cond)                                                              \
    do {                                                                         \
        if (! (cond)) {                                                          \
            std::printf ("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);       \
            ++failures;                                                          \
        }                                                                        \
    } while (0)

constexpr double kRate = 48000.0;
constexpr double kPi   = 3.14159265358979323846;

void addPianoNote (std::vector<float>& buffer, int midi, double start, double hold, float gain = 0.3f,
                   float fundamentalGain = 1.0f)
{
    const double f0 = piano::pitchToFrequency (midi);
    const double inharmonicity = 0.0004;
    const double release = 0.08;
    const int first = (int) (start * kRate);
    const int count = (int) ((hold + release) * kRate);
    const double phase = 0.37 * midi;

    for (int k = 1; k <= 10; ++k)
    {
        const double fk = k * f0 * std::sqrt (1.0 + inharmonicity * k * k);
        if (fk > kRate * 0.45) break;
        const double amp   = gain / std::pow (k, 1.1) * (k == 1 ? fundamentalGain : 1.0f);
        const double decay = 1.5 + 0.6 * k;
        for (int i = 0; i < count && first + i < (int) buffer.size(); ++i)
        {
            const double t = i / kRate;
            const double attack = std::min (1.0, t / 0.003);
            const double rel = t < hold ? 1.0 : std::exp (-(t - hold) / (release / 5.0));
            buffer[(size_t) (first + i)] += (float) (amp * attack * rel * std::exp (-decay * t)
                                                     * std::sin (2 * kPi * fk * t + phase * k));
        }
    }
}

void addNoise (std::vector<float>& buffer, float level)
{
    std::mt19937 rng (42);
    std::normal_distribution<float> dist (0.0f, level);
    for (auto& s : buffer) s += dist (rng);
}

void addKeyboardNote (std::vector<float>& buffer, int midi, double start, double hold, float gain = 0.25f,
                      float fundamentalGain = 1.0f)
{
    const double f0 = piano::pitchToFrequency (midi);
    const double releaseTau = 0.05;
    const int first = (int) (start * kRate);
    const int count = (int) ((hold + 6 * releaseTau) * kRate);
    std::mt19937 rng ((unsigned) (midi * 7919));
    std::mt19937 pressRng ((unsigned) (midi * 7919 + (int) (start * 1000)) + 17u);
    std::uniform_real_distribution<double> uni (0.0, 1.0);
    const double B = 0.00025 + 0.0005 * uni (rng);
    const double cents[3] = { -1.2, 0.1, 1.4 };
    const int partials = std::min (16, (int) (7000.0 / f0));
    for (int k = 1; k <= partials; ++k)
    {
        const double fk = k * f0 * std::sqrt (1.0 + B * k * k);
        if (fk > kRate * 0.45) break;
        double amp = gain / k * (0.6 + 0.8 * uni (rng));
        if (k == 1) amp *= fundamentalGain;
        const double d1 = 3.0 + 1.2 * k, d2 = 0.5 + 0.25 * k;
        const double ph = 6.283 * uni (pressRng);
        for (int st = 0; st < 3; ++st)
        {
            const double f = fk * std::pow (2.0, cents[st] / 1200.0);
            for (int i = 0; i < count && first + i < (int) buffer.size(); ++i)
            {
                const double t = i / kRate;
                const double attack = std::min (1.0, t / 0.002);
                const double env = 0.6 * std::exp (-d1 * t) + 0.4 * std::exp (-d2 * t);
                const double rel = t < hold ? 1.0 : std::exp (-(t - hold) / releaseTau);
                buffer[(size_t) (first + i)] += (float) (amp / 3.0 * attack * env * rel * std::sin (2 * kPi * f * t + ph));
            }
        }
    }
    std::normal_distribution<float> n (0.0f, 1.0f);
    float lp = 0.0f;
    for (int i = 0; i < (int) (0.008 * kRate) && first + i < (int) buffer.size(); ++i)
    {
        lp += 0.3f * (n (pressRng) - lp);
        buffer[(size_t) (first + i)] += gain * 0.15f * lp * (float) std::exp (-(i / kRate) / 0.002);
    }
}

void addRoom (std::vector<float>& x)
{
    const int combs[4] = { 1557, 1617, 1491, 1422 };
    std::vector<float> wet (x.size(), 0.0f);
    for (int c : combs)
    {
        std::vector<float> d ((size_t) c, 0.0f);
        int pos = 0;
        for (size_t i = 0; i < x.size(); ++i)
        {
            const float y = d[(size_t) pos];
            d[(size_t) pos] = x[i] + 0.84f * y;
            pos = (pos + 1) % c;
            wet[i] += y * 0.25f;
        }
    }
    for (int a : { 225, 556 })
    {
        std::vector<float> d ((size_t) a, 0.0f);
        int pos = 0;
        for (size_t i = 0; i < x.size(); ++i)
        {
            const float b = d[(size_t) pos], in = wet[i], y = -0.5f * in + b;
            d[(size_t) pos] = in + 0.5f * y;
            pos = (pos + 1) % a;
            wet[i] = y;
        }
    }
    for (size_t i = 0; i < x.size(); ++i) x[i] += 0.12f * wet[i];
    addNoise (x, 0.0003f);
}

std::vector<piano::NoteEvent> strikes (const std::vector<float>& audio, const std::function<std::vector<int> (double)>& expected)
{
    piano::NoteTracker t;
    t.prepare (kRate);
    std::vector<piano::NoteEvent> ons, ev;
    for (size_t i = 0; i < audio.size(); i += 512)
    {
        t.setWatched (expected (i / kRate), {});
        ev.clear();
        t.process (audio.data() + i, (int) std::min<size_t> (512, audio.size() - i), ev);
        for (const auto& e : ev)
            if (e.type == piano::NoteEvent::Type::On) ons.push_back (e);
    }
    return ons;
}

std::vector<piano::NoteEvent> strikesWith (std::shared_ptr<const piano::KeyFingerprints> fp, const std::vector<float>& audio,
                                           std::vector<int> expected = {})
{
    piano::NoteTracker t;
    t.prepare (kRate);
    t.setFingerprints (fp);
    std::vector<piano::NoteEvent> ons, ev;
    for (size_t i = 0; i < audio.size(); i += 512)
    {
        t.setWatched (expected, {});
        ev.clear();
        t.process (audio.data() + i, (int) std::min<size_t> (512, audio.size() - i), ev);
        for (const auto& e : ev)
            if (e.type == piano::NoteEvent::Type::On) ons.push_back (e);
    }
    return ons;
}

std::function<std::vector<int> (double)> always (std::vector<int> chord)
{
    return [chord] (double) { return chord; };
}

std::vector<int> pitchesOf (const std::vector<piano::NoteEvent>& ons)
{
    std::vector<int> p;
    for (const auto& e : ons) p.push_back (e.pitch);
    return p;
}

std::vector<piano::NoteEvent> listen (const std::vector<float>& audio)
{
    piano::NoteListener listener;
    listener.prepare (kRate);
    std::vector<piano::NoteEvent> events;
    for (size_t i = 0; i < audio.size(); i += 256)
        listener.process (audio.data() + i, (int) std::min<size_t> (256, audio.size() - i), events);
    return events;
}

std::vector<piano::NoteEvent> onsOnly (const std::vector<piano::NoteEvent>& events)
{
    std::vector<piano::NoteEvent> ons;
    for (const auto& e : events)
        if (e.type == piano::NoteEvent::Type::On) ons.push_back (e);
    return ons;
}

void testYinAcrossKeyboard()
{
    std::printf ("YIN accuracy, MIDI 36..96 (PSR-F51 range)\n");
    piano::YinPitchDetector yin;
    yin.prepare (kRate, 60.0f, 2200.0f);

    int wrong = 0;
    float worstCents = 0.0f;
    for (int midi = 36; midi <= 96; ++midi)
    {
        std::vector<float> audio ((size_t) (0.2 * kRate), 0.0f);
        addPianoNote (audio, midi, 0.0, 0.2);
        const auto est = yin.process (audio.data() + (size_t) (0.05 * kRate));
        const double p = est.voiced ? piano::frequencyToPitch (est.frequency) : -1.0;
        const float cents = (float) ((p - midi) * 100.0);
        if (! est.voiced || std::lround (p) != midi)
        {
            ++wrong;
            std::printf ("  midi %d read as %.2f\n", midi, p);
        }
        else
            worstCents = std::max (worstCents, std::abs (cents));
    }
    std::printf ("  wrong: %d / 61, worst error: %.1f cents\n", wrong, worstCents);
    CHECK (wrong == 0);
    CHECK (worstCents < 20.0f);
}

void testListenerMelody()
{
    std::printf ("Listener: melody with ringing notes, repeats and leaps\n");
    const std::vector<int> melody = { 64, 64, 65, 67, 67, 65, 64, 62, 60, 60, 62, 64, 36, 48, 96, 72, 72, 43 };
    const double spacing = 0.28;

    std::vector<float> audio ((size_t) ((melody.size() * spacing + 1.0) * kRate), 0.0f);
    for (size_t i = 0; i < melody.size(); ++i)
        addPianoNote (audio, melody[i], 0.1 + i * spacing, spacing - 0.01);
    addNoise (audio, 0.0005f);

    const auto ons = onsOnly (listen (audio));

    std::vector<int> heard;
    double worstLatency = 0.0, sumLatency = 0.0;
    for (const auto& e : ons) heard.push_back (e.pitch);

    CHECK (heard == melody);
    if (heard != melody)
    {
        std::printf ("  heard:");
        for (int p : heard) std::printf (" %d", p);
        std::printf ("\n");
    }
    else
    {
        for (size_t i = 0; i < ons.size(); ++i)
        {
            const double latency = ons[i].time - (0.1 + i * spacing);
            worstLatency = std::max (worstLatency, latency);
            sumLatency += latency;
        }
        std::printf ("  detection latency: mean %.1f ms, worst %.1f ms\n",
                     1000.0 * sumLatency / ons.size(), 1000.0 * worstLatency);
        CHECK (worstLatency < 0.060);
    }
}

void testListenerTransitions()
{
    std::printf ("Listener: note-to-note transitions\n");
    const int pairs[][2] = { { 60, 72 }, { 60, 79 }, { 48, 96 }, { 36, 48 }, { 84, 36 }, { 79, 43 },
                             { 72, 60 }, { 64, 65 }, { 96, 95 }, { 40, 41 }, { 55, 67 }, { 67, 55 } };
    int bad = 0;
    double worst = 0.0;
    for (const auto& p : pairs)
    {
        std::vector<float> audio ((size_t) (1.2 * kRate), 0.0f);
        addPianoNote (audio, p[0], 0.1, 0.29);
        addPianoNote (audio, p[1], 0.4, 0.5);
        const auto ons = onsOnly (listen (audio));
        const bool ok = ons.size() == 2 && ons[0].pitch == p[0] && ons[1].pitch == p[1];
        if (! ok)
        {
            ++bad;
            std::printf ("  %d -> %d heard:", p[0], p[1]);
            for (const auto& e : ons) std::printf (" %d", e.pitch);
            std::printf ("\n");
        }
        else
            worst = std::max (worst, ons[1].time - 0.4);
    }
    std::printf ("  worst transition latency: %.1f ms\n", 1000.0 * worst);
    CHECK (bad == 0);
}

void testListenerFastRepeats()
{
    std::printf ("Listener: fast repeated note (8 x 120 ms)\n");
    std::vector<float> audio ((size_t) (1.6 * kRate), 0.0f);
    for (int i = 0; i < 8; ++i)
        addPianoNote (audio, 67, 0.1 + i * 0.12, 0.10);
    const auto ons = onsOnly (listen (audio));
    std::printf ("  heard %zu note-ons\n", ons.size());
    CHECK (ons.size() == 8);
    for (const auto& e : ons) CHECK (e.pitch == 67);
}

void testListenerWithSustain()
{
    std::printf ("Listener: sustain on (notes ring for 0.9 s)\n");
    const std::vector<int> melody = { 60, 62, 64, 65, 67, 69, 71, 72, 71, 69, 67, 65, 64, 62, 60 };
    std::vector<float> audio ((size_t) ((melody.size() * 0.3 + 1.5) * kRate), 0.0f);
    for (size_t i = 0; i < melody.size(); ++i)
        addPianoNote (audio, melody[i], 0.1 + i * 0.3, 0.9);

    const auto ons = onsOnly (listen (audio));
    size_t hits = 0;
    for (size_t i = 0, j = 0; i < melody.size() && j < ons.size(); ++j)
        if (ons[j].pitch == melody[i]) { ++hits; ++i; }
    std::printf ("  %zu / %zu notes found in order, %zu events total\n", hits, melody.size(), ons.size());
    CHECK (hits == melody.size());
    CHECK (ons.size() <= melody.size() + 2);
}

void testListenerSilenceAndNoise()
{
    std::printf ("Listener: silence and background noise\n");
    std::vector<float> audio ((size_t) (2.0 * kRate), 0.0f);
    CHECK (listen (audio).empty());
    addNoise (audio, 0.002f);
    CHECK (onsOnly (listen (audio)).empty());
}

void testListenerNoteOff()
{
    std::printf ("Listener: note-off after the sound stops\n");
    std::vector<float> audio ((size_t) (1.0 * kRate), 0.0f);
    addPianoNote (audio, 60, 0.1, 0.3);
    const auto events = listen (audio);
    CHECK (events.size() == 2);
    if (events.size() == 2)
    {
        CHECK (events[0].type == piano::NoteEvent::Type::On);
        CHECK (events[1].type == piano::NoteEvent::Type::Off);
    }
}

piano::Score makeScore()
{
    piano::Score s;
    s.tracks = { { "Right", 0, 127, 0 }, { "Left", 0, 127, 0 } };
    const int rh[] = { 64, 64, 65, 67 };
    for (int i = 0; i < 4; ++i) s.notes.push_back ({ rh[i], i * 0.5, 0.45, 0, 90 });
    s.notes.push_back ({ 48, 0.0, 0.9, 1, 70 });
    s.notes.push_back ({ 55, 0.0, 0.9, 1, 70 });
    s.notes.push_back ({ 43, 1.0, 0.9, 1, 70 });
    s.notes.push_back ({ 50, 1.01, 0.9, 1, 70 });
    s.finalise();
    return s;
}

void testFollower()
{
    std::printf ("Wait-mode follower: steps, wrong notes, chord rules, octave tolerance\n");
    using R = piano::WaitModeFollower::Result;
    const auto score = makeScore();
    CHECK (score.tracks[0].noteCount == 4);
    CHECK (score.tracks[1].noteCount == 4);

    piano::WaitModeFollower f;

    f.load (score, { 0 });
    CHECK (f.stepCount() == 4);
    CHECK (f.notePlayed (60) == R::Wrong);
    CHECK (f.currentIndex() == 0);
    CHECK (f.notePlayed (64) == R::StepComplete);
    CHECK (f.notePlayed (64) == R::StepComplete);
    CHECK (f.notePlayed (65) == R::StepComplete);
    CHECK (f.notePlayed (67) == R::StepComplete);
    CHECK (! f.finished());
    CHECK (f.notePlayed (67) == R::Ignored);
    f.advance (5.0);
    CHECK (f.finished());
    CHECK (f.wrongCount() == 1);
    CHECK (f.correctCount() == 4);

    f.load (score, { 1 });
    CHECK (f.stepCount() == 2);
    CHECK (f.currentStep()->required.size() == 2);
    CHECK (f.noteOn (55, 0.00) == R::Correct);
    CHECK (f.noteOn (55, 0.01) == R::Ignored);
    CHECK (f.noteOn (48, 0.05) == R::StepComplete);

    piano::FollowerOptions lowest;
    lowest.chordRule = piano::ChordRule::Lowest;
    f.load (score, { 1 }, lowest);
    CHECK (f.notePlayed (48) == R::StepComplete);

    piano::FollowerOptions oct;
    oct.octaveTolerant = true;
    f.load (score, { 0 }, oct);
    CHECK (f.notePlayed (52) == R::StepComplete);
    CHECK (f.notePlayed (63) == R::Wrong);

    f.load (score, {});
    CHECK (f.stepCount() == 4);
    CHECK (f.currentStep()->required.size() == 3);
}

void testChordsTogether()
{
    std::printf ("Follower: chord notes must be struck together\n");
    using R = piano::WaitModeFollower::Result;
    piano::Score s;
    s.tracks = { { "Right", 0, 127, 0 } };
    for (int p : { 60, 64, 67 }) s.notes.push_back ({ p, 0.0, 1.0, 0, 90 });
    s.notes.push_back ({ 72, 1.0, 0.25, 0, 90 });
    s.finalise();

    piano::WaitModeFollower f;
    f.load (s, {});
    CHECK (f.noteOn (60, 0.00) == R::Correct);
    CHECK (f.noteOn (64, 0.05) == R::Correct);
    CHECK (f.noteOn (67, 0.10) == R::StepComplete);

    f.load (s, {});
    CHECK (f.noteOn (60, 0.00) == R::Correct);
    CHECK (f.noteOn (64, 0.05) == R::Correct);
    CHECK (f.noteOn (67, 0.40) == R::Correct);
    CHECK (f.matchedPitches() == std::vector<int> { 67 });
    CHECK (f.currentIndex() == 0);
    f.advance (2.0);
    CHECK (f.currentTime() == 0.0);
    CHECK (f.waiting() == piano::WaitModeFollower::Waiting::ForChord);
    CHECK (f.noteOn (60, 1.00) == R::Correct);
    CHECK (f.noteOn (64, 1.02) == R::Correct);
    CHECK (f.noteOn (67, 1.04) == R::StepComplete);

    f.load (s, {});
    CHECK (f.noteOn (60, 0.00) == R::Correct);
    f.noteOff (60, 0.05);
    CHECK (f.noteOn (64, 0.06) == R::Correct);
    CHECK (f.noteOn (67, 0.07) == R::Correct);
    CHECK (f.currentIndex() == 0);
}

void testRhythmGrid()
{
    std::printf ("Follower: rhythm accuracy grid (1/4, 1/8, 1/16)\n");
    using W = piano::WaitModeFollower::Waiting;

    piano::Score s;
    s.tracks = { { "Right", 0, 127, 0 } };
    auto add = [&] (int pitch, double startBeat, double beats)
    {
        piano::Note n { pitch, startBeat * 0.5, beats * 0.5, 0, 90 };
        n.startBeat = startBeat;
        n.beats = beats;
        n.secondsPerBeat = 0.5;
        s.notes.push_back (n);
    };
    add (60, 0.0, 0.5);
    add (62, 0.5, 1.0);
    add (64, 1.5, 2.0);
    add (65, 3.5, 1.0);
    s.finalise();

    auto holdOf = [&] (double grid, int pitch)
    {
        piano::FollowerOptions o;
        o.gridBeats = grid;
        piano::WaitModeFollower f;
        f.load (s, {}, o);
        for (const auto& step : f.allSteps())
            if (step.required.front() == pitch)
                return step.holdFor.front();
        return -1.0;
    };

    CHECK (holdOf (1.0, 60) == 0.0);
    CHECK (holdOf (1.0, 62) == 0.0);
    CHECK (std::abs (holdOf (1.0, 64) - 0.75) < 1e-9);
    CHECK (holdOf (0.5, 60) == 0.0);
    CHECK (std::abs (holdOf (0.5, 62) - 0.375) < 1e-9);
    CHECK (std::abs (holdOf (0.25, 60) - 0.1875) < 1e-9);
    CHECK (std::abs (holdOf (0.25, 64) - 0.9375) < 1e-9);

    piano::FollowerOptions o;
    o.gridBeats = 1.0;
    piano::WaitModeFollower f;
    f.load (s, {}, o);
    f.noteOn (60, 0.0);
    f.noteOff (60, 0.005);
    CHECK (f.earlyReleaseCount() == 0);
    CHECK (f.waiting() != W::ForHold);

    piano::Score r = s;
    r.notes.back().start = 2.25;
    r.notes.back().startBeat = 4.5;
    r.finalise();
    CHECK (s.rests.empty());
    CHECK (r.rests.size() == 1);
    CHECK (! r.rests.empty() && std::abs (r.rests[0].beats - 1.0) < 1e-9 && std::abs (r.rests[0].start - 1.75) < 1e-9);
}

void testPlayingFast()
{
    std::printf ("Follower: playing fast with short taps never gets stuck\n");
    using R = piano::WaitModeFollower::Result;
    using W = piano::WaitModeFollower::Waiting;

    piano::Score s;
    s.tracks = { { "Right", 0, 127, 0 } };
    for (int i = 0; i < 8; ++i)
    {
        piano::Note n { 60 + (i % 3) * 2, i * 0.6, 0.6, 0, 90 };
        n.startBeat = i; n.beats = 1.0; n.secondsPerBeat = 0.6;
        s.notes.push_back (n);
    }
    s.finalise();

    piano::WaitModeFollower f;
    f.load (s, {});
    double t = 0.0;
    for (int i = 0; i < 8; ++i)
    {
        const auto r = f.noteOn (60 + (i % 3) * 2, t);
        CHECK (r == R::StepComplete || r == R::Finished);
        f.advance (0.05);
        f.noteOff (60 + (i % 3) * 2, t + 0.05);
        CHECK (f.currentTime() >= s.notes[(size_t) i].start - 1e-9);
        f.advance (0.15);
        t += 0.2;
    }
    CHECK (f.earlyReleaseCount() == 8);
    CHECK (f.currentIndex() == f.stepCount());
    CHECK (f.activeHolds().size() == 1);
    CHECK (f.waiting() == W::ForHold);
    CHECK (f.noteOn (62, 2.0) == R::Resumed);
    f.advance (0.5);
    CHECK (f.finished());
}

void testHolds()
{
    std::printf ("Follower: long notes must be held\n");
    using R = piano::WaitModeFollower::Result;
    using W = piano::WaitModeFollower::Waiting;
    piano::Score s;
    s.tracks = { { "Right", 0, 127, 0 }, { "Left", 0, 127, 0 } };
    s.notes.push_back ({ 64, 0.0, 2.0, 0, 90 });
    s.notes.push_back ({ 65, 2.0, 0.25, 0, 90 });
    s.finalise();

    piano::WaitModeFollower f;
    f.load (s, {});
    CHECK (f.noteOn (64, 0.0) == R::StepComplete);
    f.advance (1.0);
    CHECK (std::abs (f.currentTime() - 1.0) < 1e-9);
    f.noteOff (64, 1.0);
    CHECK (f.waiting() == W::ForHold);
    CHECK (f.earlyReleaseCount() == 1);
    f.advance (1.0);
    CHECK (std::abs (f.currentTime() - 1.0) < 1e-9);
    CHECK (f.noteOn (64, 2.5) == R::Resumed);
    f.advance (1.5);
    CHECK (std::abs (f.currentTime() - 2.0) < 1e-9);
    CHECK (f.waiting() == W::ForChord);
    CHECK (f.activeHolds().empty());

    f.load (s, {});
    f.noteOn (64, 0.0);
    f.advance (1.9);
    f.noteOff (64, 1.9);
    CHECK (f.earlyReleaseCount() == 0);
    CHECK (f.waiting() != W::ForHold);
    f.load (s, {});
    f.noteOn (64, 0.0);
    f.advance (1.8);
    f.noteOff (64, 1.8);
    CHECK (f.earlyReleaseCount() == 1);

    piano::Score o;
    o.tracks = { { "Right", 0, 127, 0 }, { "Left", 0, 127, 0 } };
    o.notes.push_back ({ 48, 0.0, 1.0, 1, 70 });
    o.notes.push_back ({ 64, 0.0, 0.5, 0, 90 });
    o.notes.push_back ({ 65, 0.5, 0.5, 0, 90 });
    o.finalise();
    f.load (o, {});
    CHECK (f.noteOn (48, 0.00) == R::Correct);
    CHECK (f.noteOn (64, 0.03) == R::StepComplete);
    f.advance (0.6);
    CHECK (std::abs (f.currentTime() - 0.5) < 1e-9);
    f.noteOff (64, 0.45);
    CHECK (f.noteOn (65, 0.6) == R::StepComplete);
    f.noteOff (48, 0.6);
    CHECK (f.waiting() == W::ForHold);
    CHECK (f.noteOn (48, 0.9) == R::Resumed);
    f.advance (1.0);
    CHECK (f.finished());
}

void testChordDetector()
{
    std::printf ("Chord detector: expected notes in chords, wrong notes, releases\n");
    struct Case { const char* name; std::vector<int> played, watched, expectOn; };
    const std::vector<Case> cases = {
        { "C major triad",            { 60, 64, 67 },     { 60, 64, 67 },     { 60, 64, 67 } },
        { "spread C3 G3 E4",          { 48, 55, 64 },     { 48, 55, 64 },     { 48, 55, 64 } },
        { "octave C3 + C4",           { 48, 60 },         { 48, 60 },         { 48, 60 } },
        { "low D2 + A2",              { 38, 45 },         { 38, 45 },         { 38, 45 } },
        { "high C6 E6 G6",            { 84, 88, 91 },     { 84, 88, 91 },     { 84, 88, 91 } },
        { "dense A3 C4 E4 F#4",       { 57, 60, 64, 66 }, { 57, 60, 64, 66 }, { 57, 60, 64, 66 } },
        { "chord missing E4",         { 60 },             { 60, 64 },         { 60 } },
        { "C3 instead of C4",         { 48 },             { 60 },             {} },
        { "C#4 instead of C4",        { 61 },             { 60 },             {} },
        { "C4 instead of C5",         { 60 },             { 72 },             {} },
        { "G2 instead of D4 (12th)",  { 43 },             { 62 },             {} },
        { "G#4 instead of G4",        { 60, 64, 68 },     { 60, 64, 67 },     { 60, 64 } },
    };
    int bad = 0;
    double worst = 0.0;
    for (const auto& c : cases)
    {
        piano::ChordDetector d;
        d.prepare (kRate);
        d.setWatched (c.watched);
        std::vector<float> audio ((size_t) (2.0 * kRate), 0.0f);
        for (int p : c.played) addPianoNote (audio, p, 0.3, 1.0);
        std::vector<piano::NoteEvent> ev;
        for (size_t i = 0; i < audio.size(); i += 256)
            d.process (audio.data() + i, (int) std::min<size_t> (256, audio.size() - i), ev);

        std::vector<int> ons;
        bool offsOk = true;
        for (const auto& e : ev)
        {
            if (e.type == piano::NoteEvent::Type::On) { ons.push_back (e.pitch); worst = std::max (worst, e.time - 0.3); }
            else if (e.time < 1.3 || e.time > 1.45) offsOk = false;
        }
        std::sort (ons.begin(), ons.end());
        if (ons != c.expectOn || ! offsOk)
        {
            ++bad;
            std::printf ("  %s:", c.name);
            for (const auto& e : ev) std::printf (" %s%s@%.0fms", e.type == piano::NoteEvent::Type::On ? "+" : "-", piano::noteName (e.pitch).c_str(), (e.time - 0.3) * 1000);
            std::printf ("\n");
        }
    }
    std::printf ("  worst chord-note latency: %.0f ms\n", worst * 1000.0);
    CHECK (bad == 0);
    CHECK (worst < 0.12);

    piano::ChordDetector d;
    d.prepare (kRate);
    d.setWatched ({ 84 });
    std::vector<float> audio ((size_t) (3.5 * kRate), 0.0f);
    addPianoNote (audio, 84, 0.2, 3.0);
    std::vector<piano::NoteEvent> ev;
    for (size_t i = 0; i < audio.size(); i += 256) d.process (audio.data() + i, (int) std::min<size_t> (256, audio.size() - i), ev);
    CHECK (ev.size() == 2);
    if (ev.size() == 2) CHECK (ev[1].type == piano::NoteEvent::Type::Off && ev[1].time > 3.2);
}

void testWrongKeysDontMovePiece()
{
    std::printf ("Tracker: a wrong key whose overtones contain the expected note is not accepted\n");
    int accepted = 0, total = 0;
    for (int expected : { 50, 64, 79 })
        for (int interval : { -34, -28, -26, -22, -16, -7, 12 })
        {
            const int wrong = expected + interval;
            if (wrong < 36 || wrong > 96) continue;
            piano::NoteTracker t;
            t.prepare (kRate);
            t.setWatched ({ expected }, {});
            std::vector<float> audio ((size_t) (0.6 * kRate), 0.0f);
            addPianoNote (audio, wrong, 0.05, 0.4);
            std::vector<piano::NoteEvent> ev;
            bool hit = false;
            for (size_t i = 0; i < audio.size(); i += 512)
            {
                ev.clear();
                t.process (audio.data() + i, (int) std::min<size_t> (512, audio.size() - i), ev);
                for (const auto& e : ev) hit |= e.type == piano::NoteEvent::Type::On && e.pitch == expected;
            }
            ++total;
            accepted += hit;
        }
    std::printf ("  accepted %d of %d wrong keys\n", accepted, total);
    CHECK (accepted == 0);

    {
        auto accepts = [] (int expected, auto addAudio, double watchFrom)
        {
            piano::NoteTracker t;
            t.prepare (kRate);
            std::vector<float> audio ((size_t) (2.5 * kRate), 0.0f);
            addAudio (audio);
            std::vector<piano::NoteEvent> ev;
            bool hit = false;
            for (size_t i = 0; i < audio.size(); i += 512)
            {
                t.setWatched (i / kRate >= watchFrom ? std::vector<int> { expected } : std::vector<int> {}, {});
                ev.clear();
                t.process (audio.data() + i, (int) std::min<size_t> (512, audio.size() - i), ev);
                for (const auto& e : ev) hit |= e.type == piano::NoteEvent::Type::On && e.pitch == expected;
            }
            return hit;
        };
        CHECK (! accepts (64, [] (auto& a) { addPianoNote (a, 36, 0.05, 1.2, 0.3f, 0.01f); }, 0.0));
        CHECK (! accepts (64, [] (auto& a) { addPianoNote (a, 40, 0.05, 2.4, 0.3f, 0.05f); }, 1.6));
        CHECK (accepts (64, [] (auto& a) { addPianoNote (a, 64, 0.05, 0.6); }, 0.0));
    }

    {
        piano::Score sc;
        sc.tracks = { { "Right", 0, 127, 0 } };
        for (int i = 0; i < 5; ++i)
            sc.notes.push_back ({ std::array<int, 5> { 64, 64, 65, 67, 67 }[(size_t) i], i * 0.3, 0.3, 0, 90 });
        sc.finalise();
        piano::PracticeSession session;
        session.load (sc);
        piano::NoteTracker t;
        t.prepare (kRate);
        std::vector<float> audio ((size_t) (2.5 * kRate), 0.0f);
        addPianoNote (audio, 64, 0.05, 2.0);
        auto steps = [&] (const std::vector<float>& a)
        {
            piano::PracticeSession ps;
            ps.load (sc);
            piano::NoteTracker tr;
            tr.prepare (kRate);
            std::vector<piano::NoteEvent> evs;
            for (size_t i = 0; i < a.size(); i += 512)
            {
                const auto w = ps.watched();
                tr.setWatched (w.chord, w.held);
                evs.clear();
                tr.process (a.data() + i, (int) std::min<size_t> (512, a.size() - i), evs);
                for (const auto& e : evs)
                    (e.type == piano::NoteEvent::Type::On) ? (void) ps.noteOn (e.pitch, e.time) : ps.noteOff (e.pitch, e.time);
                ps.advance (512 / kRate);
            }
            return (int) ps.follower().currentIndex();
        };
        std::vector<float> twice ((size_t) (2.5 * kRate), 0.0f);
        addPianoNote (twice, 64, 0.05, 0.35);
        addPianoNote (twice, 64, 0.45, 1.5);
        std::printf ("  two E4 presses: %d step(s) done\n", steps (twice));
        CHECK (steps (twice) == 2);
        std::vector<piano::NoteEvent> ev;
        for (size_t i = 0; i < audio.size(); i += 512)
        {
            const auto w = session.watched();
            t.setWatched (w.chord, w.held);
            ev.clear();
            t.process (audio.data() + i, (int) std::min<size_t> (512, audio.size() - i), ev);
            for (const auto& e : ev)
                (e.type == piano::NoteEvent::Type::On) ? (void) session.noteOn (e.pitch, e.time) : session.noteOff (e.pitch, e.time);
            session.advance (512 / kRate);
        }
        std::printf ("  one E4 press: %d step(s) done\n", (int) session.follower().currentIndex());
        CHECK (session.follower().currentIndex() == 1);
    }

    piano::NoteTracker t;
    t.prepare (kRate);
    t.setWatched ({ 60, 64, 67 }, {});
    std::vector<float> audio ((size_t) (0.6 * kRate), 0.0f);
    for (int p : { 60, 64, 67 }) addPianoNote (audio, p, 0.05, 0.4);
    std::vector<piano::NoteEvent> ev;
    double last = -1.0;
    int found = 0;
    for (size_t i = 0; i < audio.size(); i += 512)
    {
        ev.clear();
        t.process (audio.data() + i, (int) std::min<size_t> (512, audio.size() - i), ev);
        for (const auto& e : ev)
            if (e.type == piano::NoteEvent::Type::On && (e.pitch == 60 || e.pitch == 64 || e.pitch == 67))
            {
                ++found;
                last = std::max (last, (i + 512) / kRate);
            }
    }
    std::printf ("  chord: %d of 3 notes, all in by %.0f ms after the strike\n", found, (last - 0.05) * 1000.0);
    CHECK (found == 3);
    CHECK (last - 0.05 < 0.2);
}

void testKeyStrikes()
{
    std::printf ("Key strikes (keyboard-like voice, reverb, noise): one press = one note\n");
    using V = std::vector<int>;
    {
        std::vector<float> a ((size_t) (3.5 * kRate), 0.0f);
        addKeyboardNote (a, 64, 0.1, 3.0);
        addRoom (a);
        CHECK (pitchesOf (strikes (a, always ({ 64 }))) == V { 64 });
    }
    for (double gap : { 0.12, 0.2 })
    {
        std::vector<float> a ((size_t) (1.6 * kRate), 0.0f);
        for (int i = 0; i < 6; ++i) addKeyboardNote (a, 64, 0.1 + i * gap, gap < 0.15 ? 0.08 : gap - 0.02);
        addRoom (a);
        const auto ons = strikes (a, always ({ 64 }));
        std::printf ("  E4 x6, %.0f ms apart: %zu note-ons\n", gap * 1000.0, ons.size());
        CHECK (pitchesOf (ons) == V (6, 64));
    }
    {
        const int m[] = { 64, 64, 65, 67, 67, 65, 64, 62, 60, 60, 62, 64, 64, 62, 62 };
        std::vector<float> a ((size_t) (4.5 * kRate), 0.0f);
        for (int i = 0; i < 15; ++i) addKeyboardNote (a, m[i], 0.1 + i * 0.25, 0.23);
        addRoom (a);
        const auto ons = strikes (a, [&m] (double t) { return V { m[std::clamp ((int) ((t - 0.1) / 0.25 + 0.5), 0, 14)] }; });
        std::printf ("  Ode to Joy, legato: %zu of 15 notes\n", ons.size());
        CHECK (pitchesOf (ons) == V (std::begin (m), std::end (m)));
    }
    {
        std::vector<float> a ((size_t) (1.2 * kRate), 0.0f);
        const int m[] = { 60, 62, 64, 65, 67, 69 };
        for (int i = 0; i < 6; ++i) addKeyboardNote (a, m[i], 0.1 + i * 0.09, 0.12);
        addRoom (a);
        CHECK (pitchesOf (strikes (a, always ({}))) == V (std::begin (m), std::end (m)));
    }
    for (const V& chord : { V { 60, 64, 67 }, V { 53, 57, 60, 65 }, V { 60, 72 }, V { 43, 50, 59 } })
    {
        std::vector<float> a ((size_t) (1.0 * kRate), 0.0f);
        for (int p : chord) addKeyboardNote (a, p, 0.1 + 0.004 * (p % 3), 0.7);
        addRoom (a);
        const auto ons = strikes (a, always (chord));
        CHECK (pitchesOf (ons) == chord);
        if (! ons.empty()) CHECK (ons.back().time - ons.front().time < 0.001);
    }
    for (double gap : { 0.0, 0.04 })
    {
        std::vector<float> a ((size_t) (1.0 * kRate), 0.0f);
        addKeyboardNote (a, 68, 0.1, 0.5);
        addKeyboardNote (a, 66, 0.1 + gap, 0.5, 0.18f);
        addRoom (a);
        CHECK (pitchesOf (strikes (a, always ({ 66, 68 }))) == V ({ 66, 68 }));
    }
    {
        std::vector<float> a ((size_t) (1.0 * kRate), 0.0f);
        addKeyboardNote (a, 59, 0.1, 0.5);
        addKeyboardNote (a, 71, 0.143, 0.5);
        addRoom (a);
        CHECK (pitchesOf (strikes (a, always ({ 59, 71 }))) == V ({ 59, 71 }));
    }

    std::printf ("Key strikes: wrong keys never count as the expected one\n");
    for (int wrong : { 65, 52, 57 })
    {
        std::vector<float> a ((size_t) (1.0 * kRate), 0.0f);
        addKeyboardNote (a, wrong, 0.1, 0.5);
        addRoom (a);
        CHECK (pitchesOf (strikes (a, always ({ 64 }))) == V { wrong });
    }
    for (float fundamental : { 1.0f, 0.01f })
    {
        std::vector<float> a ((size_t) (1.5 * kRate), 0.0f);
        addKeyboardNote (a, 36, 0.1, 1.0, 0.25f, fundamental);
        addRoom (a);
        CHECK (pitchesOf (strikes (a, always ({ 64 }))) == V { 36 });
    }
    struct Case { int ringing, wrong; double gap; };
    for (const Case& c : { Case { 64, 66, 0.15 }, Case { 67, 60, 0.12 }, Case { 64, 52, 0.15 } })
    {
        std::vector<float> a ((size_t) (2.0 * kRate), 0.0f);
        addKeyboardNote (a, c.ringing, 0.1, 1.6);
        for (int i = 0; i < 5; ++i) addKeyboardNote (a, c.wrong, 0.5 + i * c.gap, c.gap < 0.13 ? 0.08 : 0.1);
        addRoom (a);
        const auto ons = strikes (a, always ({ c.ringing }));
        V expectedOns { c.ringing };
        for (int i = 0; i < 5; ++i) expectedOns.push_back (c.wrong);
        std::printf ("  %s ringing, %s x5: %s\n", piano::noteName (c.ringing).c_str(), piano::noteName (c.wrong).c_str(),
                     pitchesOf (ons) == expectedOns ? "right" : "WRONG");
        CHECK (pitchesOf (ons) == expectedOns);
    }
    {
        std::vector<float> a ((size_t) (1.5 * kRate), 0.0f);
        for (int p : { 60, 64, 67 }) addKeyboardNote (a, p, 0.1, 1.2);
        addKeyboardNote (a, 74, 0.6, 0.5);
        addRoom (a);
        CHECK (pitchesOf (strikes (a, always ({ 74 }))) == (V { 60, 64, 67, 74 }));
    }
    {
        std::vector<float> a ((size_t) (2.0 * kRate), 0.0f);
        addNoise (a, 0.003f);
        CHECK (strikes (a, always ({ 64 })).empty());
    }
}

void testSwitchingHandsKeepsPlace()
{
    std::printf ("Switching hands keeps the place in the piece\n");
    piano::Score s;
    s.tracks = { { "Right", 0, 127, 0 }, { "Left", 0, 127, 0 } };
    for (int i = 0; i < 8; ++i)
    {
        s.notes.push_back ({ 72 + (i % 3), i * 0.5, 0.4, 0, 90 });
        if (i % 2 == 0) s.notes.push_back ({ 48, i * 0.5, 0.9, 1, 90 });
    }
    s.finalise();
    piano::PracticeSession session;
    session.load (s);
    CHECK (session.canChooseHand());
    session.setHand (piano::Hand::Right);
    for (int i = 0; i < 5; ++i) session.play (72 + (i % 3));
    CHECK (std::abs (session.follower().currentStep()->time - 2.5) < 1e-6);

    session.setHand (piano::Hand::Left);
    CHECK (session.follower().currentStep() != nullptr);
    CHECK (std::abs (session.follower().currentStep()->time - 3.0) < 1e-6);
    session.setHand (piano::Hand::Both);
    CHECK (std::abs (session.follower().currentStep()->time - 3.0) < 1e-6);
    session.setHand (piano::Hand::Right);
    CHECK (std::abs (session.follower().currentStep()->time - 3.0) < 1e-6);
}

void testTrackerDrivesSession()
{
    std::printf ("Audio -> tracker -> session: chord, hold, early release\n");
    piano::Score s;
    s.tracks = { { "Right", 0, 127, 0 } };
    for (int p : { 60, 64, 67 }) s.notes.push_back ({ p, 0.0, 1.0, 0, 90 });
    s.notes.push_back ({ 72, 1.0, 0.25, 0, 90 });
    s.finalise();

    auto run = [&] (double holdFor, int& earlyReleases, bool& waitingAtC5)
    {
        piano::PracticeSession session;
        session.load (s);
        piano::NoteTracker tracker;
        tracker.prepare (kRate);
        std::vector<float> audio ((size_t) (2.0 * kRate), 0.0f);
        for (int p : { 60, 64, 67 }) addPianoNote (audio, p, 0.2, holdFor);
        std::vector<piano::NoteEvent> ev;
        for (size_t i = 0; i < audio.size(); i += 512)
        {
            const auto w = session.watched();
            tracker.setWatched (w.chord, w.held);
            ev.clear();
            tracker.process (audio.data() + i, (int) std::min<size_t> (512, audio.size() - i), ev);
            for (const auto& e : ev)
                (e.type == piano::NoteEvent::Type::On) ? (void) session.noteOn (e.pitch, e.time) : session.noteOff (e.pitch, e.time);
            if (session.follower().currentIndex() > 0)
                session.advance (512 / kRate);
        }
        earlyReleases = session.earlyReleases();
        waitingAtC5 = session.follower().currentIndex() == 1 && std::abs (session.follower().currentTime() - 1.0) < 1e-6;
        return session.follower().correctCount();
    };

    int early = 0; bool atC5 = false;
    CHECK (run (1.2, early, atC5) == 3);
    CHECK (early == 0);
    CHECK (atC5);
    CHECK (run (0.4, early, atC5) == 3);
    CHECK (early >= 1);
    CHECK (! atC5);
}

std::vector<std::uint8_t> readFile (const std::string& path)
{
    std::ifstream in (path, std::ios::binary);
    return { std::istreambuf_iterator<char> (in), std::istreambuf_iterator<char>() };
}

void testMidiReaderSynthetic()
{
    std::printf ("MIDI reader: running status, tempo change, 3/4, drums skipped\n");
    const std::vector<std::uint8_t> track = {
        0x00, 0xFF, 0x58, 0x04, 0x03, 0x02, 0x18, 0x08,
        0x00, 0xFF, 0x51, 0x03, 0x0F, 0x42, 0x40,
        0x00, 0xFF, 0x03, 0x04, 'T', 'e', 's', 't',
        0x00, 0x90, 0x3C, 0x64,
        0x60, 0x3C, 0x00,
        0x00, 0x3E, 0x64,
        0x60, 0x80, 0x3E, 0x40,
        0x00, 0xFF, 0x51, 0x03, 0x07, 0xA1, 0x20,
        0x00, 0x90, 0x40, 0x64,
        0x60, 0x40, 0x00,
        0x00, 0x99, 0x24, 0x64,
        0x60, 0x89, 0x24, 0x00,
        0x00, 0xFF, 0x2F, 0x00 };
    std::vector<std::uint8_t> file = { 'M','T','h','d', 0,0,0,6, 0,0, 0,1, 0,96, 'M','T','r','k' };
    const auto len = (std::uint32_t) track.size();
    file.insert (file.end(), { (std::uint8_t) (len >> 24), (std::uint8_t) (len >> 16), (std::uint8_t) (len >> 8), (std::uint8_t) len });
    file.insert (file.end(), track.begin(), track.end());

    piano::Score s;
    std::string error;
    CHECK (piano::parseMidi (file.data(), file.size(), s, error));
    CHECK (s.title == "Test");
    CHECK (s.tracks.size() == 1);
    CHECK (s.notes.size() == 3);
    if (s.notes.size() == 3)
    {
        auto near = [] (double a, double b) { return std::abs (a - b) < 1.0e-9; };
        CHECK (s.notes[0].pitch == 60 && near (s.notes[0].start, 0.0) && near (s.notes[0].duration, 1.0));
        CHECK (s.notes[1].pitch == 62 && near (s.notes[1].start, 1.0) && near (s.notes[1].duration, 1.0));
        CHECK (s.notes[2].pitch == 64 && near (s.notes[2].start, 2.0) && near (s.notes[2].duration, 0.5));
    }
    CHECK (s.barStarts.size() == 2);
    if (s.barStarts.size() == 2) CHECK (std::abs (s.barStarts[1] - 2.5) < 1.0e-9);

    const std::uint8_t junk[] = { 'R', 'I', 'F', 'F', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    CHECK (! piano::parseMidi (junk, sizeof (junk), s, error));
}

void testMidiReaderAndSessionOnSample()
{
    std::printf ("MIDI reader + practice session on tests/data/ode_to_joy.mid\n");
    const auto bytes = readFile (std::string (PIANO_TEST_DATA_DIR) + "/ode_to_joy.mid");
    piano::Score s;
    std::string error;
    CHECK (piano::parseMidi (bytes.data(), bytes.size(), s, error));
    CHECK (s.title == "Ode to Joy");
    CHECK (s.tracks.size() == 2);
    if (s.tracks.size() == 2)
    {
        CHECK (s.tracks[0].name == "Right Hand" && s.tracks[0].noteCount == 62);
        CHECK (s.tracks[1].name == "Left Hand" && s.tracks[1].noteCount == 32);
    }
    CHECK (std::abs (s.lengthSeconds() - 38.4) < 1.0e-6);

    using R = piano::WaitModeFollower::Result;
    piano::PracticeSession session;
    session.load (s);
    CHECK (session.canChooseHand());
    CHECK (session.hand() == piano::Hand::Right);
    CHECK (session.followedTracks() == std::vector<int> { 0 });
    CHECK (session.barCount() == 16);
    CHECK (session.prompt().big == "E4");

    CHECK (session.play (64) == R::StepComplete);
    CHECK (session.play (72) == R::Wrong);
    CHECK (session.streak() == 0);
    for (int p : { 64, 65, 67 }) session.play (p);
    CHECK (session.streak() == 3);
    CHECK (session.accuracyPercent() == 80);
    session.advance (10.0);
    CHECK (std::abs (session.follower().currentTime() - 2.4) < 1e-6);
    CHECK (session.currentBar() == 2);
    CHECK (session.prompt().big == "G4");

    session.setHand (piano::Hand::Left);
    CHECK (session.followedTracks() == std::vector<int> { 1 });
    CHECK (session.isLeftHandTrack (1) && ! session.isLeftHandTrack (0));
    const piano::Note* nextLeft = nullptr;
    for (const auto& n : session.score().notes)
        if (n.track == 1 && n.start >= 2.4 - 1e-6 && (nextLeft == nullptr || n.start < nextLeft->start)) nextLeft = &n;
    CHECK (nextLeft != nullptr);
    CHECK (std::abs (session.follower().currentStep()->time - nextLeft->start) < 1e-6);
    CHECK (session.prompt().big == piano::noteName (nextLeft->pitch));
    auto o = session.options();
    o.inputShift = 1;
    session.setOptions (o);
    CHECK (session.play (nextLeft->pitch - 12) == R::StepComplete);
}

}

void testFingerprints()
{
    std::printf ("Fingerprints: calibrate 61 keys, then chords, octaves, a bass cluster\n");
    using V = std::vector<int>;

    std::vector<float> a ((size_t) ((0.5 + 61 * 0.6) * kRate), 0.0f);
    for (int i = 0; i < 61; ++i) addKeyboardNote (a, 36 + i, 0.3 + i * 0.6, 0.4);
    addRoom (a);
    piano::NoteTracker t;
    t.prepare (kRate);
    t.setCapture (true);
    piano::KeyCalibration calibration;
    calibration.start (36, 61);
    std::vector<piano::NoteEvent> ev;
    for (size_t i = 0; i < a.size(); i += 512)
    {
        ev.clear();
        t.process (a.data() + i, (int) std::min<size_t> (512, a.size() - i), ev);
        for (const auto& c : t.takeCaptured()) calibration.add (c);
    }
    CHECK (calibration.isComplete());
    const auto built = calibration.build ("test");
    CHECK (built != nullptr);
    if (built == nullptr) return;

    const auto bytes = built->serialise();
    auto fp = std::make_shared<piano::KeyFingerprints>();
    CHECK (piano::KeyFingerprints::deserialise (bytes.data(), bytes.size(), *fp));
    CHECK (fp->magnitudes == built->magnitudes && fp->offsets == built->offsets && fp->lowKey == 36 && fp->keyCount == 61);
    piano::KeyFingerprints truncated;
    CHECK (! piano::KeyFingerprints::deserialise (bytes.data(), bytes.size() - 1, truncated));

    auto heard = [&] (const V& keys, double spread = 0.0, V expected = {})
    {
        std::vector<float> x ((size_t) (1.2 * kRate), 0.0f);
        for (size_t i = 0; i < keys.size(); ++i) addKeyboardNote (x, keys[i], 0.2 + spread * (double) i, 0.6);
        addRoom (x);
        auto p = pitchesOf (strikesWith (fp, x, expected));
        std::sort (p.begin(), p.end());
        return p;
    };
    CHECK (heard ({ 36, 38, 40 }) == V ({ 36, 38, 40 }));
    CHECK (heard ({ 60, 72 }) == V ({ 60, 72 }));
    CHECK (heard ({ 60 }, 0.0, { 60, 72 }) == V ({ 60 }));
    CHECK (heard ({ 48, 64, 67, 72 }) == V ({ 48, 64, 67, 72 }));
    CHECK (heard ({ 52 }, 0.0, { 64 }) == V ({ 52 }));
    CHECK (heard ({ 68, 66 }, 0.04) == V ({ 66, 68 }));
    {
        const int m[] = { 64, 64, 65, 67, 67, 65, 64, 62, 60, 60, 62, 64, 64, 62, 62 };
        std::vector<float> x ((size_t) (4.5 * kRate), 0.0f);
        for (int i = 0; i < 15; ++i) addKeyboardNote (x, m[i], 0.1 + i * 0.25, 0.23);
        addRoom (x);
        const auto ons = strikesWith (fp, x);
        std::printf ("  Ode to Joy with fingerprints: %zu of 15 notes\n", ons.size());
        CHECK (pitchesOf (ons) == V (std::begin (m), std::end (m)));
    }
}

int main()
{
    testYinAcrossKeyboard();
    testListenerMelody();
    testListenerTransitions();
    testListenerFastRepeats();
    testListenerWithSustain();
    testListenerSilenceAndNoise();
    testListenerNoteOff();
    testFollower();
    testChordsTogether();
    testHolds();
    testRhythmGrid();
    testPlayingFast();
    testChordDetector();
    testWrongKeysDontMovePiece();
    testKeyStrikes();
    testFingerprints();
    testTrackerDrivesSession();
    testSwitchingHandsKeepsPlace();
    testMidiReaderSynthetic();
    testMidiReaderAndSessionOnSample();

    if (failures == 0)
        std::printf ("\nAll tests passed.\n");
    else
        std::printf ("\n%d check(s) failed.\n", failures);
    return failures == 0 ? 0 : 1;
}
