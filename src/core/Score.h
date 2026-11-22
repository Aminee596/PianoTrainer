#pragma once

#include <string>
#include <vector>

namespace piano
{
struct Note
{
    int    pitch    = 60;
    double start    = 0.0;
    double duration = 0.0;
    int    track    = 0;
    int    velocity = 100;

    double startBeat      = -1.0;
    double beats          = -1.0;
    double secondsPerBeat = 0.5;

    double lengthInBeats() const { return beats > 0.0 ? beats : duration / secondsPerBeat; }
};

struct Rest
{
    int    track = 0;
    double start = 0.0, duration = 0.0;
    double beats = 0.0;
};

struct TrackInfo
{
    std::string name;
    int noteCount = 0;
    int lowest    = 127;
    int highest   = 0;
};

struct Score
{
    std::string            title;
    std::vector<Note>      notes;
    std::vector<TrackInfo> tracks;
    std::vector<double>    barStarts;
    std::vector<Rest>      rests;

    int barAt (double seconds) const;

    void   clear();
    void   finalise();
    double lengthSeconds() const;
    bool   empty() const { return notes.empty(); }
};

std::string noteName (int midiPitch);

double pitchToFrequency (double midiPitch);

double frequencyToPitch (double hz);

}
