#include "app/DetectionLog.h"
#include "core/Score.h"

namespace app
{
DetectionLog::DetectionLog (juce::File f) : folder (std::move (f)) {}

void DetectionLog::setEnabled (bool shouldLog)
{
    enabled = shouldLog;
    if (! enabled)
        out = nullptr;
}

void DetectionLog::open()
{
    folder.createDirectory();
    const auto file = folder.getChildFile ("detection-" + juce::Time::getCurrentTime().formatted ("%Y-%m-%d_%H-%M-%S") + ".csv")
                          .getNonexistentSibling();
    out = std::make_unique<juce::FileOutputStream> (file);
    if (out->failedToOpen())
    {
        out = nullptr;
        return;
    }
    *out << "time,audio_s,event,detector,note,midi,frequency_hz,level_db,rise_db,attack,heard_as,expected,result,step,piece,fit\n";
}

void DetectionLog::write (const piano::NoteEvent& e, const Context& c)
{
    if (! enabled)
        return;
    if (out == nullptr)
        open();
    if (out == nullptr)
        return;

    auto csv = [] (const juce::String& s) { return s.containsAnyOf (",\"\n") ? s.quoted() : s; };
    const int heard = e.pitch + 12 * c.inputShift;
    juce::StringArray expected;
    for (int p : c.expected)
        expected.add (juce::String (piano::noteName (p)));

    juce::String result = c.result;
    if (e.rejected == piano::NoteEvent::Rejected::Overtones)
        result = "rejected: overtones of " + juce::String (piano::noteName (e.otherPitch + 12 * c.inputShift));
    else if (e.rejected == piano::NoteEvent::Rejected::Unconfirmed)
        result = "rejected: not confirmed";
    else if (e.rejected == piano::NoteEvent::Rejected::TooQuiet)
        result = "rejected: too quiet";
    else if (e.rejected == piano::NoteEvent::Rejected::NoStrike)
        result = "rejected: still ringing, not pressed again";
    else if (e.rejected == piano::NoteEvent::Rejected::NoKey)
        result = "strike heard, no key found";
    else if (e.rejected == piano::NoteEvent::Rejected::NotInSound)
        result = "rejected: not in the sound";
    else if (e.rejected == piano::NoteEvent::Rejected::TooSoon)
        result = "rejected: counted by the strike just before";

    auto name = [] (int p) { return juce::String (piano::noteName (p)); };
    const auto now = juce::Time::getCurrentTime();
    juce::StringArray cols {
        now.formatted ("%H:%M:%S") + "." + juce::String (now.getMilliseconds()).paddedLeft ('0', 3),
        juce::String (e.time, 3),
        e.type == piano::NoteEvent::Type::On ? "on" : "off",
        e.source == piano::NoteEvent::Source::Fingerprint ? "fingerprint"
            : e.source == piano::NoteEvent::Source::Attack ? "attack" : e.source == piano::NoteEvent::Source::Chord ? "chord" : "single",
        name (e.pitch),
        juce::String (e.pitch),
        juce::String (piano::pitchToFrequency (e.pitch + e.cents / 100.0), 2),
        juce::String (e.levelDb, 1),
        e.type == piano::NoteEvent::Type::On ? juce::String (e.riseDb, 1) : juce::String(),
        e.type == piano::NoteEvent::Type::On ? juce::String (e.attack, 2) : juce::String(),
        name (heard),
        csv (expected.joinIntoString (" ")),
        csv (result),
        c.step >= 0 ? juce::String (c.step) : juce::String(),
        csv (c.piece),
        e.fitCost >= 0.0f ? juce::String (e.fitCost, 2) : juce::String() };
    *out << cols.joinIntoString (",") << "\n";
    out->flush();
}

}
