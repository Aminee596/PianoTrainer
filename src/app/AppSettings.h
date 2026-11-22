#pragma once

#include "core/PracticeSession.h"

#include <juce_data_structures/juce_data_structures.h>
#include <juce_graphics/juce_graphics.h>

namespace app
{
struct AppSettings
{
    int  inputShift = 0;
    bool monitor    = false;
    juce::String outputDevice;
    int  listenVolume = 70;
    piano::Hand listenHands = piano::Hand::Both;
    int  speed = 100;
    piano::Hand practiceHand = piano::Hand::Right;

    bool   anyOctave     = false;
    int    chordWindowMs = 150;
    bool   buildChords   = true;
    bool   holdNotes     = true;
    int    rhythmGrid    = 8;
    bool   detectionLog  = true;
    float  gateDb        = -50.0f;
    piano::ChordChoice chords = piano::ChordChoice::Auto;

    bool  noteNames     = true;
    int   fallingSpeed  = 160;
    bool  showOtherHand = true;
    int   keyboardKeys  = 61;

    juce::String toPlay = "#46C2FF", played = "#37D483", rightHand = "#6E8BFF", leftHand = "#FFA94D";

    void load (const juce::PropertiesFile& file);
    void save (juce::PropertiesFile& file) const;

    juce::var toVar() const;

    void merge (const juce::var& changes);

    piano::SessionOptions sessionOptions() const;

    void applyColours() const;
};

}
