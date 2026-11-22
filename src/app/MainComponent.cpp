#include "app/MainComponent.h"
#include "app/Theme.h"
#include "core/MidiReader.h"

#include <algorithm>

namespace app
{
using namespace theme;

namespace
{
    bool isMidiFile (const juce::String& path)
    {
        return path.endsWithIgnoreCase (".mid") || path.endsWithIgnoreCase (".midi");
    }
}

MainComponent::MainComponent (juce::PropertiesFile& s)
    : settings (s)
{
    for (auto* c : std::initializer_list<juce::Component*> { &topBar, &roll, &keyboard, &bottomBar })
        addAndMakeVisible (c);

    topBar.open.onClick     = [this] { chooseFile(); };
    topBar.settings.onClick = [this] { showAudioSettings(); };
    topBar.onInputClicked   = [this] { showAudioSettings(); };
    topBar.hands.onChange   = [this] (int index)
    {
        session.setHand ((piano::Hand) index);
        updateRoll();
        refresh();
    };

    auto& b = bottomBar;
    b.restart.onClick = [this] { session.restart(); refresh(); };
    b.prev.onClick    = [this] { session.step (-1); refresh(); };
    b.next.onClick    = [this] { session.step (1);  refresh(); };

    b.chords.addItem ("All notes",   (int) piano::ChordChoice::Auto);
    b.chords.addItem ("Top note",    (int) piano::ChordChoice::Top);
    b.chords.addItem ("Bottom note", (int) piano::ChordChoice::Bottom);
    b.chords.setSelectedId (settings.getIntValue ("chordRule", (int) piano::ChordChoice::Auto), juce::dontSendNotification);
    b.chords.onChange = [this] { applyOptions(); };

    for (int shift = -2; shift <= 2; ++shift)
        b.input.addItem (shift == 0 ? juce::String ("As heard") : juce::String (shift > 0 ? "+" : "") + juce::String (shift) + " oct", shift + 3);
    b.input.setSelectedId (settings.getIntValue ("inputShift", 0) + 3, juce::dontSendNotification);
    b.input.onChange = [this] { applyOptions(); };

    b.anyOctave.setToggleState (settings.getBoolValue ("octaveTolerant", false), juce::dontSendNotification);
    b.anyOctave.onClick = [this] { applyOptions(); };

    for (auto* c : std::initializer_list<juce::Component*> { &topBar.hands, &topBar.settings, &topBar.open, &b.restart,
                                                             &b.prev, &b.next, &b.chords, &b.input, &b.anyOctave })
        c->setWantsKeyboardFocus (false);

    applyOptions();

    engine.onNoteEvent = [this] (const piano::NoteEvent& e) { onNoteEvent (e); };
    const auto savedAudio = settings.getXmlValue ("audioDevice");
    if (const auto error = engine.initialise (savedAudio.get()); error.isNotEmpty())
    {
        errorTitle  = "No audio input";
        errorDetail = error;
    }
    engine.getDeviceManager().addChangeListener (this);

    setWantsKeyboardFocus (true);
    setSize (1280, 800);
    startTimerHz (60);

    const juce::File last (settings.getValue ("lastFile"));
    if (last.existsAsFile())
        loadFile (last);
    else
        refresh();
    refreshInputChip();
}

MainComponent::~MainComponent()
{
    stopTimer();
    engine.getDeviceManager().removeChangeListener (this);
    engine.onNoteEvent = nullptr;
}

void MainComponent::loadFile (const juce::File& file)
{
    juce::MemoryBlock bytes;
    piano::Score loaded;
    std::string error;
    if (! file.loadFileAsData (bytes) || ! piano::parseMidi ((const std::uint8_t*) bytes.getData(), bytes.getSize(), loaded, error))
    {
        errorTitle  = "Couldn't open " + file.getFileName();
        errorDetail = error.empty() ? juce::String ("The file can't be read") : juce::String (error);
        refresh();
        return;
    }

    session.load (std::move (loaded));
    errorTitle.clear();
    errorDetail.clear();
    fileName = file.getFileName();
    settings.setValue ("lastFile", file.getFullPathName());
    settings.setValue ("lastFolder", file.getParentDirectory().getFullPathName());

    int lo = 127, hi = 0;
    for (const auto& n : session.score().notes)
    {
        lo = std::min (lo, n.pitch);
        hi = std::max (hi, n.pitch);
    }
    layout.setRange (lo, hi);

    const auto& sc = session.score();
    topBar.setPiece (sc.title.empty() ? file.getFileNameWithoutExtension() : juce::String (sc.title), fileName);
    topBar.hands.setOptionEnabled ((int) piano::Hand::Left,  session.canChooseHand());
    topBar.hands.setOptionEnabled ((int) piano::Hand::Right, session.canChooseHand());
    topBar.hands.setSelected ((int) session.hand());

    updateRoll();
    resized();
    refresh();
    displayTime = session.follower().currentTime() - 1.0;
}

void MainComponent::updateRoll()
{
    std::vector<juce::Colour> colours;
    for (size_t t = 0; t < session.score().tracks.size(); ++t)
        colours.push_back (session.isLeftHandTrack ((int) t) ? colour::noteLeft : colour::noteRight);
    roll.setScore (&session.score(), &session.follower(), session.followedTracks(), colours);
}

void MainComponent::applyOptions()
{
    piano::SessionOptions o;
    o.chords     = (piano::ChordChoice) bottomBar.chords.getSelectedId();
    o.inputShift = bottomBar.input.getSelectedId() - 3;
    o.anyOctave  = bottomBar.anyOctave.getToggleState();
    session.setOptions (o);

    settings.setValue ("chordRule", (int) o.chords);
    settings.setValue ("inputShift", o.inputShift);
    settings.setValue ("octaveTolerant", o.anyOctave);
    refresh();
}

void MainComponent::onNoteEvent (const piano::NoteEvent& e)
{
    const int heard = e.pitch + 12 * session.options().inputShift;

    if (e.type == piano::NoteEvent::Type::Off)
    {
        const int before = session.earlyReleases();
        session.noteOff (e.pitch, e.time);
        if (session.earlyReleases() != before)
            keyboard.flash (heard, false);
        refresh();
        return;
    }

    if (! session.hasPiece())
    {
        keyboard.flash (heard, true);
        return;
    }

    using R = piano::WaitModeFollower::Result;
    switch (session.noteOn (e.pitch, e.time))
    {
        case R::Correct:
        case R::StepComplete:
        case R::Resumed:
        case R::Finished: keyboard.flash (heard, true);  break;
        case R::Wrong:    keyboard.flash (heard, false); break;
        case R::Ignored:  break;
    }
    refresh();
}

void MainComponent::syncWatched()
{
    const auto w = session.watched();
    engine.setWatched (w.chord, w.held);
}

void MainComponent::refresh()
{
    syncWatched();
    const auto& f = session.follower();

    std::vector<int> toPlay = session.remaining(), done = f.matchedPitches();
    for (const auto& h : f.activeHolds())
        (h.released ? toPlay : done).push_back (h.pitch);
    keyboard.setExpected (toPlay, session.chordOthers(), done);

    PianoRollView::Prompt prompt;
    if (errorTitle.isNotEmpty())
        prompt = { "SOMETHING WENT WRONG", errorTitle, errorDetail, colour::wrong };
    else
    {
        using K = piano::PracticeSession::Prompt::Kind;
        const auto p = session.prompt();
        const auto c = p.kind == K::Finished ? colour::correct : p.kind == K::KeepHolding ? colour::heard : colour::expected;
        prompt = { p.caption, p.big, p.detail, c };
    }
    roll.setPrompt (prompt);

    PianoRollView::Stats stats;
    stats.visible  = session.hasPiece();
    const int acc  = session.accuracyPercent();
    stats.accuracy = acc >= 0 ? juce::String (acc) + "%" : juce::String ("-");
    stats.correct  = juce::String (f.correctCount());
    stats.wrong    = juce::String (f.wrongCount() + f.earlyReleaseCount());
    stats.streak   = juce::String (session.streak());
    roll.setStats (stats);

    roll.repaint();
    repaint (progressArea);
}

void MainComponent::refreshInputChip()
{
    auto* device = engine.getDeviceManager().getCurrentAudioDevice();
    const auto meter = engine.getMeter();
    const int shift = session.options().inputShift;

    juce::String deviceLine, hearing;
    if (device == nullptr)
    {
        deviceLine = "No input";
        hearing = "Click to set up";
    }
    else
    {
        const double khz = device->getCurrentSampleRate() / 1000.0;
        const auto rate = std::abs (khz - std::round (khz)) < 0.05 ? juce::String (juce::roundToInt (khz)) : juce::String (khz, 1);
        deviceLine = device->getName() + juce::String::fromUTF8 ("  \xc2\xb7  ") + rate + " kHz";
        hearing = meter.pitch >= 0 ? "Hearing " + juce::String (piano::noteName (meter.pitch + 12 * shift))
                                   : juce::String ("Listening...");
    }
    topBar.setInput (deviceLine, hearing, device != nullptr ? meter.levelDb : -100.0f);

    bottomBar.setDetectionText (device != nullptr
                                    ? "Analysis window  " + juce::String (juce::roundToInt (engine.getAnalysisWindowMs())) + " ms"
                                    : juce::String());
}

void MainComponent::timerCallback()
{
    const double nowMs = juce::Time::getMillisecondCounterHiRes();
    const double dt = lastTickMs > 0.0 ? juce::jlimit (0.0, 0.1, (nowMs - lastTickMs) / 1000.0) : 0.0;
    lastTickMs = nowMs;

    const auto stepBefore = session.follower().currentIndex();
    const auto holdsBefore = session.follower().activeHolds().size();
    session.advance (dt);
    if (session.follower().activeHolds().size() != holdsBefore || session.follower().currentIndex() != stepBefore)
        refresh();
    else
        syncWatched();

    const double target = session.follower().currentTime();
    if (std::abs (target - displayTime) > 0.25)
        displayTime += (target - displayTime) * 0.22;
    else
        displayTime = target;
    roll.setTime (displayTime);
    repaint (progressArea);

    const auto meter = engine.getMeter();
    keyboard.setHeardPitch (meter.pitch >= 0 ? meter.pitch + 12 * session.options().inputShift : -1);
    keyboard.tick();

    if (--slowTick <= 0)
    {
        slowTick = 4;
        refreshInputChip();
    }
}

void MainComponent::changeListenerCallback (juce::ChangeBroadcaster*)
{
    if (auto xml = engine.getDeviceManager().createStateXml())
        settings.setValue ("audioDevice", xml.get());
    settings.saveIfNeeded();

    if (engine.getDeviceManager().getCurrentAudioDevice() != nullptr && errorTitle == "No audio input")
    {
        errorTitle.clear();
        errorDetail.clear();
        refresh();
    }
}

void MainComponent::chooseFile()
{
    juce::File folder (settings.getValue ("lastFolder"));
    if (! folder.isDirectory())
        folder = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);

    chooser = std::make_unique<juce::FileChooser> ("Open a MIDI file", folder, "*.mid;*.midi");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
                          {
                              const auto file = fc.getResult();
                              if (file.existsAsFile())
                                  loadFile (file);
                          });
}

void MainComponent::showAudioSettings()
{
    auto* selector = new juce::AudioDeviceSelectorComponent (engine.getDeviceManager(),
                                                             1, 2,
                                                             0, 0,
                                                             false, false, false, false);
    selector->setSize (520, 360);

    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned (selector);
    options.dialogTitle = "Audio input";
    options.dialogBackgroundColour = colour::surfacePanel;
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = false;
    options.launchAsync();
}

bool MainComponent::keyPressed (const juce::KeyPress& key)
{
    static const juce::String keys ("awsedftgyhujk");
    const auto c = juce::CharacterFunctions::toLowerCase (key.getTextCharacter());
    if (const int index = keys.indexOfChar (c); index >= 0)
    {
        const int code = (int) juce::CharacterFunctions::toUpperCase (c);
        if (typingDown.count (code) == 0)
        {
            piano::NoteEvent e;
            e.pitch = 60 + 12 * typingOctave + index - 12 * session.options().inputShift;
            e.time = juce::Time::getMillisecondCounterHiRes() / 1000.0;
            typingDown[code] = e.pitch;
            onNoteEvent (e);
        }
        return true;
    }
    if (c == 'z' || c == 'x')
    {
        typingOctave = juce::jlimit (-3, 3, typingOctave + (c == 'x' ? 1 : -1));
        return true;
    }
    if (c == 'r')                        { session.restart(); refresh(); return true; }
    if (key == juce::KeyPress::leftKey)  { session.step (-1); refresh(); return true; }
    if (key == juce::KeyPress::rightKey) { session.step (1);  refresh(); return true; }
    return false;
}

bool MainComponent::keyStateChanged (bool)
{
    for (auto it = typingDown.begin(); it != typingDown.end();)
    {
        if (juce::KeyPress::isKeyCurrentlyDown (it->first))
        {
            ++it;
            continue;
        }
        piano::NoteEvent e;
        e.type = piano::NoteEvent::Type::Off;
        e.pitch = it->second;
        e.time = juce::Time::getMillisecondCounterHiRes() / 1000.0;
        it = typingDown.erase (it);
        onNoteEvent (e);
    }
    return false;
}

bool MainComponent::isInterestedInFileDrag (const juce::StringArray& files)
{
    return std::any_of (files.begin(), files.end(), [] (const juce::String& f) { return isMidiFile (f); });
}

void MainComponent::filesDropped (const juce::StringArray& files, int, int)
{
    for (const auto& f : files)
        if (isMidiFile (f))
        {
            loadFile (juce::File (f));
            return;
        }
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (colour::bgApp);
    if (progressArea.isEmpty())
        return;

    auto area = progressArea.reduced (space::s24, 0);
    const auto& f = session.follower();
    const auto count = f.stepCount();
    const auto index = std::min (f.currentIndex(), count);

    const juce::String stepText = count > 0 ? "STEP " + juce::String ((int) std::min (index + 1, count)) + " / " + juce::String ((int) count)
                                            : juce::String ("STEP - / -");
    const juce::String barText = session.barCount() > 0
                                     ? "Bar " + juce::String (session.currentBar()) + " of " + juce::String (session.barCount())
                                     : juce::String();

    const auto capFont = font (Text::LabelCaps), monoFont = font (Text::MonoSmall);
    const int stepW = (int) std::ceil (juce::GlyphArrangement::getStringWidth (juce::Font (capFont), stepText)) + 2;
    const int barW  = (int) std::ceil (juce::GlyphArrangement::getStringWidth (juce::Font (monoFont), barText)) + 2;

    g.setColour (colour::textSecondary);
    g.setFont (capFont);
    g.drawText (stepText, area.removeFromLeft (stepW), juce::Justification::centredLeft, false);
    g.setColour (colour::textMuted);
    g.setFont (monoFont);
    g.drawText (barText, area.removeFromRight (barW), juce::Justification::centredRight, false);

    const auto track = area.reduced (space::s16, 0).toFloat().withSizeKeepingCentre ((float) area.getWidth() - 2.0f * space::s16, 4.0f);
    g.setColour (colour::surfaceHover);
    g.fillRoundedRectangle (track, 2.0f);
    if (count > 0 && index > 0)
    {
        g.setColour (colour::expected);
        g.fillRoundedRectangle (track.withWidth (track.getWidth() * (float) index / (float) count), 2.0f);
    }
}

void MainComponent::resized()
{
    auto area = getLocalBounds();
    topBar.setBounds (area.removeFromTop (layout::topBar));
    progressArea = area.removeFromTop (layout::progress);
    bottomBar.setBounds (area.removeFromBottom (layout::bottomBar));

    layout.width = (float) area.getWidth();
    keyboard.setBounds (area.removeFromBottom (KeyboardView::idealHeight (layout.whiteWidth())));
    roll.setBounds (area);

    keyboard.setLayout (layout);
    roll.setLayout (layout);
}

}
