#include "app/AppSettings.h"
#include "app/Theme.h"

namespace app
{
namespace
{
    bool isHexColour (const juce::String& s)
    {
        return (s.length() == 7 || s.length() == 9) && s[0] == '#' && s.substring (1).containsOnly ("0123456789abcdefABCDEF");
    }

    juce::Colour toColour (const juce::String& hex, juce::Colour fallback)
    {
        if (! isHexColour (hex))
            return fallback;
        const auto rgb = (juce::uint32) hex.substring (1, 7).getHexValue32();
        const auto alpha = hex.length() == 9 ? (juce::uint32) hex.substring (7, 9).getHexValue32() : 0xffu;
        return juce::Colour ((alpha << 24) | rgb);
    }

    juce::String normalise (const juce::String& hex)
    {
        const auto h = hex.toUpperCase();
        return h.length() == 9 && h.endsWith ("FF") ? h.substring (0, 7) : h;
    }

    const char* chordName (piano::ChordChoice c)
    {
        switch (c)
        {
            case piano::ChordChoice::Top:    return "top";
            case piano::ChordChoice::Bottom: return "bottom";
            case piano::ChordChoice::Auto:
            case piano::ChordChoice::All:    break;
        }
        return "all";
    }

    int validGrid (int g) { return g == 4 || g == 16 ? g : 8; }

    const char* handName (piano::Hand h)
    {
        return h == piano::Hand::Left ? "left" : h == piano::Hand::Right ? "right" : "both";
    }

    piano::Hand handFromName (const juce::String& s)
    {
        return s == "left" ? piano::Hand::Left : s == "right" ? piano::Hand::Right : piano::Hand::Both;
    }

    piano::ChordChoice chordFromName (const juce::String& s)
    {
        if (s == "top")    return piano::ChordChoice::Top;
        if (s == "bottom") return piano::ChordChoice::Bottom;
        return piano::ChordChoice::Auto;
    }
}

void AppSettings::load (const juce::PropertiesFile& f)
{
    const AppSettings d;
    inputShift    = juce::jlimit (-2, 2, f.getIntValue ("inputShift", d.inputShift));
    monitor       = f.getBoolValue ("monitor", d.monitor);
    outputDevice  = f.getValue ("outputDevice");
    listenVolume  = juce::jlimit (0, 100, f.getIntValue ("listenVolume", d.listenVolume));
    listenHands   = handFromName (f.getValue ("listenHands", handName (d.listenHands)));
    speed         = juce::jlimit (25, 150, f.getIntValue ("speed", d.speed));
    practiceHand  = handFromName (f.getValue ("practiceHand", handName (d.practiceHand)));
    anyOctave     = f.getBoolValue ("octaveTolerant", d.anyOctave);
    chordWindowMs = juce::jlimit (50, 300, f.getIntValue ("chordWindowMs", d.chordWindowMs));
    buildChords   = f.getBoolValue ("buildChords", d.buildChords);
    holdNotes     = f.getBoolValue ("holdNotes", d.holdNotes);
    rhythmGrid    = validGrid (f.getIntValue ("rhythmGrid", d.rhythmGrid));
    detectionLog  = f.getBoolValue ("detectionLog", d.detectionLog);
    gateDb        = (float) juce::jlimit (-80.0, -30.0, f.getDoubleValue ("gateDb", d.gateDb));
    chords        = (piano::ChordChoice) juce::jlimit ((int) piano::ChordChoice::Auto, (int) piano::ChordChoice::All,
                                                   f.getIntValue ("chordRule", (int) d.chords));
    noteNames     = f.getBoolValue ("noteNames", d.noteNames);
    fallingSpeed  = juce::jlimit (80, 320, f.getIntValue ("fallingSpeed", d.fallingSpeed));
    showOtherHand = f.getBoolValue ("showOtherHand", d.showOtherHand);
    keyboardKeys  = f.getIntValue ("keyboardKeys", d.keyboardKeys) == 88 ? 88 : 61;

    auto colour = [&] (const char* key, const juce::String& fallback)
    {
        const auto v = f.getValue (key, fallback);
        return isHexColour (v) ? normalise (v) : fallback;
    };
    toPlay    = colour ("colourToPlay", d.toPlay);
    played    = colour ("colourPlayed", d.played);
    rightHand = colour ("colourRight",  d.rightHand);
    leftHand  = colour ("colourLeft",   d.leftHand);
}

void AppSettings::save (juce::PropertiesFile& f) const
{
    f.setValue ("inputShift", inputShift);
    f.setValue ("monitor", monitor);
    f.setValue ("outputDevice", outputDevice);
    f.setValue ("listenVolume", listenVolume);
    f.setValue ("listenHands", handName (listenHands));
    f.setValue ("speed", speed);
    f.setValue ("practiceHand", handName (practiceHand));
    f.setValue ("octaveTolerant", anyOctave);
    f.setValue ("chordWindowMs", chordWindowMs);
    f.setValue ("buildChords", buildChords);
    f.setValue ("holdNotes", holdNotes);
    f.setValue ("rhythmGrid", rhythmGrid);
    f.setValue ("detectionLog", detectionLog);
    f.setValue ("gateDb", gateDb);
    f.setValue ("chordRule", (int) chords);
    f.setValue ("noteNames", noteNames);
    f.setValue ("fallingSpeed", fallingSpeed);
    f.setValue ("showOtherHand", showOtherHand);
    f.setValue ("keyboardKeys", keyboardKeys);
    f.setValue ("colourToPlay", toPlay);
    f.setValue ("colourPlayed", played);
    f.setValue ("colourRight", rightHand);
    f.setValue ("colourLeft", leftHand);
}

juce::var AppSettings::toVar() const
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("inputShift", inputShift);
    o->setProperty ("monitor", monitor);
    o->setProperty ("outputDevice", outputDevice);
    o->setProperty ("listenVolume", listenVolume);
    o->setProperty ("listenHands", handName (listenHands));
    o->setProperty ("speed", speed);
    o->setProperty ("anyOctave", anyOctave);
    o->setProperty ("chordWindowMs", chordWindowMs);
    o->setProperty ("buildChords", buildChords);
    o->setProperty ("holdNotes", holdNotes);
    o->setProperty ("rhythmGrid", rhythmGrid);
    o->setProperty ("detectionLog", detectionLog);
    o->setProperty ("gateDb", gateDb);
    o->setProperty ("chords", chordName (chords));
    o->setProperty ("noteNames", noteNames);
    o->setProperty ("fallingSpeed", fallingSpeed);
    o->setProperty ("showOtherHand", showOtherHand);
    o->setProperty ("keyboardKeys", keyboardKeys);

    auto* c = new juce::DynamicObject();
    c->setProperty ("toPlay", toPlay);
    c->setProperty ("played", played);
    c->setProperty ("right", rightHand);
    c->setProperty ("left", leftHand);
    o->setProperty ("colours", juce::var (c));
    return juce::var (o);
}

void AppSettings::merge (const juce::var& v)
{
    auto has = [&] (const char* key) { return v.hasProperty (key); };

    if (has ("inputShift"))    inputShift    = juce::jlimit (-2, 2, (int) v["inputShift"]);
    if (has ("monitor"))       monitor       = (bool) v["monitor"];
    if (has ("outputDevice"))  outputDevice  = v["outputDevice"].toString();
    if (has ("listenVolume"))  listenVolume  = juce::jlimit (0, 100, (int) v["listenVolume"]);
    if (has ("listenHands"))   listenHands   = handFromName (v["listenHands"].toString());
    if (has ("speed"))         speed         = juce::jlimit (25, 150, (int) v["speed"]);
    if (has ("anyOctave"))     anyOctave     = (bool) v["anyOctave"];
    if (has ("chordWindowMs")) chordWindowMs = juce::jlimit (50, 300, (int) v["chordWindowMs"]);
    if (has ("buildChords"))   buildChords   = (bool) v["buildChords"];
    if (has ("holdNotes"))     holdNotes     = (bool) v["holdNotes"];
    if (has ("rhythmGrid"))    rhythmGrid    = validGrid ((int) v["rhythmGrid"]);
    if (has ("detectionLog"))  detectionLog  = (bool) v["detectionLog"];
    if (has ("gateDb"))        gateDb        = (float) juce::jlimit (-80.0, -30.0, (double) v["gateDb"]);
    if (has ("chords"))        chords        = chordFromName (v["chords"].toString());
    if (has ("noteNames"))     noteNames     = (bool) v["noteNames"];
    if (has ("fallingSpeed"))  fallingSpeed  = juce::jlimit (80, 320, (int) v["fallingSpeed"]);
    if (has ("showOtherHand")) showOtherHand = (bool) v["showOtherHand"];
    if (has ("keyboardKeys"))  keyboardKeys  = (int) v["keyboardKeys"] == 88 ? 88 : 61;

    if (const auto c = v["colours"]; c.isObject())
    {
        auto set = [&] (const char* key, juce::String& target)
        {
            const auto s = c[key].toString();
            if (isHexColour (s)) target = normalise (s);
        };
        set ("toPlay", toPlay);
        set ("played", played);
        set ("right", rightHand);
        set ("left", leftHand);
    }
}

piano::SessionOptions AppSettings::sessionOptions() const
{
    piano::SessionOptions o;
    o.chords       = chords;
    o.anyOctave    = anyOctave;
    o.inputShift   = inputShift;
    o.strikeWindow = chordWindowMs / 1000.0;
    o.buildChords  = buildChords;
    o.holdNotes    = holdNotes;
    o.gridBeats    = 4.0 / rhythmGrid;
    return o;
}

void AppSettings::applyColours() const
{
    const AppSettings d;
    auto& nc = theme::noteColours();
    nc.toPlay = toColour (toPlay,    toColour (d.toPlay, {}));
    nc.played = toColour (played,    toColour (d.played, {}));
    nc.right  = toColour (rightHand, toColour (d.rightHand, {}));
    nc.left   = toColour (leftHand,  toColour (d.leftHand, {}));
}

}
