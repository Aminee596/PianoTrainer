#include "app/PracticeScreen.h"
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

PracticeScreen::PracticeScreen (AudioEngine& e)
    : engine (e)
{
    for (auto* c : std::initializer_list<juce::Component*> { &topBar, &roll, &keyboard, &bottomBar })
        addAndMakeVisible (c);

    topBar.home.onClick     = [this] { if (onHome) onHome(); };
    topBar.open.onClick     = [this] { if (onOpenFile) onOpenFile(); };
    topBar.library.onClick  = [this] { if (onLibrary) onLibrary(); };
    topBar.settings.onClick = [this] { if (onSettings) onSettings(); };
    topBar.onInputClicked   = [this] { if (onAudioSettings) onAudioSettings(); };
    topBar.hands.onChange   = [this] (int index)
    {
        stopListening();
        preferredHand = (piano::Hand) index;
        session.setHand (preferredHand);
        wasFinished = session.follower().currentStep() == nullptr;
        updateRoll();
        refresh();
        if (onHandChanged) onHandChanged (preferredHand);
    };

    auto& b = bottomBar;
    b.restart.onClick = [this] { stopListening(); session.restart(); wasFinished = false; refresh(); };
    b.prev.onClick    = [this] { stopListening(); session.step (-1); refresh(); };
    b.next.onClick    = [this] { stopListening(); session.step (1);  refresh(); };
    b.listen.onClick  = [this] { toggleListening(); };
    b.speed.onChange  = [this] { setSpeed (bottomBar.speed.getSelectedId(), true); };

    roll.onScroll    = [this] (double seconds) { scrollBy (seconds); };
    roll.onScrollEnd = [this] { endScroll(); };

    b.chords.addItem ("All notes",   (int) piano::ChordChoice::Auto);
    b.chords.addItem ("Top note",    (int) piano::ChordChoice::Top);
    b.chords.addItem ("Bottom note", (int) piano::ChordChoice::Bottom);
    b.chords.onChange = [this] { bottomBarChanged(); };

    b.hear.addItem ("Both hands", (int) piano::Hand::Both + 1);
    b.hear.addItem ("Right hand", (int) piano::Hand::Right + 1);
    b.hear.addItem ("Left hand",  (int) piano::Hand::Left + 1);
    b.hear.setSelectedId ((int) listenHands + 1, juce::dontSendNotification);
    b.hear.onChange = [this]
    {
        setListenHands ((piano::Hand) (bottomBar.hear.getSelectedId() - 1));
        if (onListenHandsChanged) onListenHandsChanged (listenHands);
    };

    for (auto* c : std::initializer_list<juce::Component*> { &topBar.home, &topBar.library, &topBar.hands, &topBar.settings, &topBar.open,
                                                             &b.restart, &b.prev, &b.next, &b.listen, &b.hear, &b.speed, &b.chords })
        c->setWantsKeyboardFocus (false);

    roll.setOpaque (true);
    keyboard.setOpaque (true);

    setOptions (session.options());
    setWantsKeyboardFocus (true);
    setSize (1280, 800);
    refresh();
    refreshInputChip();
}

PracticeScreen::~PracticeScreen() = default;

bool PracticeScreen::loadFile (const juce::File& f, juce::String& error)
{
    juce::MemoryBlock bytes;
    piano::Score loaded;
    std::string parseError;
    if (! f.loadFileAsData (bytes) || ! piano::parseMidi ((const std::uint8_t*) bytes.getData(), bytes.getSize(), loaded, parseError))
    {
        error = "Couldn't open " + f.getFileName() + ": "
              + (parseError.empty() ? juce::String ("the file can't be read") : juce::String (parseError));
        return false;
    }
    if (loaded.empty())
    {
        error = f.getFileName() + " has no notes";
        return false;
    }

    stopListening();
    session.load (std::move (loaded));
    session.setHand (preferredHand);
    errorTitle.clear();
    errorDetail.clear();
    file = f;
    wasFinished = false;

    int lo = 127, hi = 0;
    for (const auto& n : session.score().notes)
    {
        lo = std::min (lo, n.pitch);
        hi = std::max (hi, n.pitch);
    }
    pieceLowest = lo;
    pieceHighest = hi;
    layout.setRange (lo, hi);

    const auto& sc = session.score();
    topBar.setPiece (sc.title.empty() ? f.getFileNameWithoutExtension() : juce::String (sc.title), f.getFileName());
    topBar.hands.setOptionEnabled ((int) piano::Hand::Left,  session.canChooseHand());
    topBar.hands.setOptionEnabled ((int) piano::Hand::Right, session.canChooseHand());
    topBar.hands.setSelected ((int) session.hand());
    bottomBar.hear.setEnabled (session.canChooseHand());

    updateRoll();
    resized();
    refresh();
    displayTime = session.follower().currentTime() - 1.0;
    return true;
}

void PracticeScreen::updateRoll()
{
    const auto& nc = noteColours();
    std::vector<juce::Colour> colours;
    for (size_t t = 0; t < session.score().tracks.size(); ++t)
        colours.push_back (session.isLeftHandTrack ((int) t) ? nc.left : nc.right);
    roll.setScore (&session.score(), &session.follower(), session.followedTracks(), colours);
}

void PracticeScreen::setOptions (const piano::SessionOptions& o)
{
    session.setOptions (o);

    auto& b = bottomBar;
    b.chords.setSelectedId ((int) o.chords, juce::dontSendNotification);
    refresh();
}

void PracticeScreen::bottomBarChanged()
{
    auto o = session.options();
    o.chords     = (piano::ChordChoice) bottomBar.chords.getSelectedId();
    setOptions (o);
    if (onOptionsChanged)
        onOptionsChanged (o);
}

void PracticeScreen::applySettings (const AppSettings& s)
{
    setOptions (s.sessionOptions());
    roll.setDisplay ((float) s.fallingSpeed, s.showOtherHand, s.noteNames);
    keyboard.setShowNames (s.noteNames);
    if (s.keyboardKeys != layout.keys)
    {
        layout.keys = s.keyboardKeys;
        layout.setRange (pieceLowest, pieceHighest);
        resized();
    }
    setListenHands (s.listenHands);
    setSpeed (s.speed, false);
    if (s.practiceHand != preferredHand)
    {
        preferredHand = s.practiceHand;
        session.setHand (preferredHand);
        topBar.hands.setSelected ((int) session.hand());
    }
    updateRoll();
    keyboard.repaint();
}

void PracticeScreen::setAudioError (const juce::String& error)
{
    if (error != audioError)
    {
        audioError = error;
        refresh();
    }
}

PracticeScreen::Judged PracticeScreen::context() const
{
    Judged j;
    j.expected = session.remaining();
    j.step = session.hasPiece() ? (int) session.follower().currentIndex() + 1 : -1;
    j.piece = file.getFileName();
    j.inputShift = session.options().inputShift;
    return j;
}

void PracticeScreen::onNoteEvent (const piano::NoteEvent& e)
{
    auto judged = context();
    auto report = [&] (const char* result)
    {
        judged.result = result;
        if (onJudged) onJudged (e, judged);
    };

    if (listening || juce::Time::getMillisecondCounterHiRes() / 1000.0 < ignoreInputUntil)
    {
        report ("ignored: listen mode");
        return;
    }

    const int heard = e.pitch + 12 * session.options().inputShift;

    if (e.type == piano::NoteEvent::Type::Off)
    {
        const int before = session.earlyReleases();
        session.noteOff (e.pitch, e.time);
        const bool early = session.earlyReleases() != before;
        if (early)
            keyboard.flash (heard, false);
        report (early ? "let go too early" : "released");
        refresh();
        return;
    }

    if (! session.hasPiece())
    {
        keyboard.flash (heard, true);
        report ("no piece open");
        return;
    }

    using R = piano::WaitModeFollower::Result;
    switch (session.noteOn (e.pitch, e.time))
    {
        case R::Correct:      keyboard.flash (heard, true);  report ("correct"); break;
        case R::StepComplete: keyboard.flash (heard, true);  report ("correct, step done"); break;
        case R::Resumed:      keyboard.flash (heard, true);  report ("pressed again (held note)"); break;
        case R::Finished:     keyboard.flash (heard, true);  report ("correct, piece finished"); break;
        case R::Wrong:        keyboard.flash (heard, false); report ("wrong"); break;
        case R::Ignored:      report ("ignored: already counted"); break;
    }
    refresh();
}

void PracticeScreen::syncWatched()
{
    const auto w = session.watched();
    engine.setWatched (w.chord, w.held);
}

void PracticeScreen::refresh()
{
    syncWatched();
    const auto& f = session.follower();

    if (listening)
    {
        keyboard.setExpected (engine.getPlayer().soundingPitches(), {}, {});
    }
    else
    {
        if (f.currentIndex() != pulsedStep)
        {
            pulsedStep = f.currentIndex();
            if (const auto* step = f.currentStep())
                for (int p : step->required)
                    keyboard.pulse (p);
        }

        std::vector<int> toPlay = session.remaining(), done = f.matchedPitches();
        for (const auto& h : f.activeHolds())
            (h.released ? toPlay : done).push_back (h.pitch);
        keyboard.setExpected (toPlay, session.chordOthers(), done);
    }

    PianoRollView::Prompt prompt;
    if (errorTitle.isNotEmpty())
        prompt = { "SOMETHING WENT WRONG", errorTitle, errorDetail, colour::wrong };
    else if (listening)
        prompt = { "LISTENING", topBar.getTitle(), "Press Space or Stop to play it yourself from here", noteColours().toPlay };
    else if (audioError.isNotEmpty())
        prompt = { "NO AUDIO INPUT", "Choose an input", audioError, colour::wrong };
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

    const bool finished = session.hasPiece() && f.finished() && ! listening;
    if (finished && ! wasFinished && onRunFinished)
        onRunFinished();
    wasFinished = finished;
}

void PracticeScreen::refreshInputChip()
{
    auto* device = engine.getDeviceManager().getCurrentAudioDevice();
    const auto meter = engine.getMeter();
    const int shift = session.options().inputShift;

    juce::String deviceLine, hearing;
    const auto input = engine.getInputName();
    if (device == nullptr || input.isEmpty())
    {
        deviceLine = "No input";
        hearing = "Click to set up";
    }
    else
    {
        const double khz = device->getCurrentSampleRate() / 1000.0;
        const auto rate = std::abs (khz - std::round (khz)) < 0.05 ? juce::String (juce::roundToInt (khz)) : juce::String (khz, 1);
        deviceLine = input.upToFirstOccurrenceOf (" (", false, false) + juce::String::fromUTF8 ("  \xc2\xb7  ") + rate + " kHz";
        hearing = meter.pitch >= 0 ? "Hearing " + juce::String (piano::noteName (meter.pitch + 12 * shift))
                                   : juce::String ("Listening...");
    }
    topBar.setInput (deviceLine, hearing, device != nullptr && input.isNotEmpty() ? meter.levelDb : -100.0f);

    bottomBar.setDetectionText (device != nullptr
                                    ? "Analysis window  " + juce::String (juce::roundToInt (engine.getAnalysisWindowMs())) + " ms"
                                    : juce::String());
}

void PracticeScreen::frame (double now)
{
    const double dt = lastFrame > 0.0 ? juce::jlimit (0.0, 0.1, now - lastFrame) : 0.0;
    lastFrame = now;

    if (listening && followListening())
    {
        if (now - lastChipUpdate > 1.0 / 15.0)
        {
            lastChipUpdate = now;
            refreshInputChip();
        }
        return;
    }

    if (scrolling)
    {
        if (! roll.isDragging() && ! progressDragging
            && juce::Time::getMillisecondCounterHiRes() / 1000.0 - lastScrollAt > 0.3)
            endScroll();
        else
        {
            keyboard.tick();
            return;
        }
    }

    const auto stepBefore = session.follower().currentIndex();
    const auto holdsBefore = session.follower().activeHolds().size();
    session.advance (dt * speedPercent / 100.0);
    if (session.follower().activeHolds().size() != holdsBefore || session.follower().currentIndex() != stepBefore)
        refresh();
    else
        syncWatched();

    const double target = session.follower().currentTime();
    if (std::abs (target - displayTime) > 0.25)
        displayTime += (target - displayTime) * (1.0 - std::exp (-dt * 13.0));
    else
        displayTime = target;
    roll.setTime (displayTime);
    repaint (progressArea);

    const auto meter = engine.getMeter();
    keyboard.setHeardPitch (meter.pitch >= 0 ? meter.pitch + 12 * session.options().inputShift : -1);
    keyboard.tick();

    if (now - lastChipUpdate > 1.0 / 15.0)
    {
        lastChipUpdate = now;
        refreshInputChip();
    }
}

void PracticeScreen::visibilityChanged()
{
    if (isVisible())
    {
        lastFrame = 0.0;
        vblank = juce::VBlankAttachment (this, [this] (double t) { frame (t); });
    }
    else
    {
        stopListening();
        vblank = {};
    }
}

void PracticeScreen::setSpeed (int percent, bool notify)
{
    percent = juce::jlimit (25, 150, percent);
    bottomBar.speed.setSelectedId (percent, juce::dontSendNotification);
    if (bottomBar.speed.getSelectedId() != percent)
        bottomBar.speed.setText (juce::String (percent) + "%", juce::dontSendNotification);
    if (percent == speedPercent)
        return;
    speedPercent = percent;
    engine.getPlayer().setSpeed (percent / 100.0f);
    if (notify && onSpeedChanged)
        onSpeedChanged (percent);
}

void PracticeScreen::scrollTo (double seconds)
{
    if (! session.hasPiece())
        return;

    if (! scrolling)
    {
        scrolling = true;
        resumeListening = listening;
        stopListening();
    }

    scrollTime = juce::jlimit (-1.0, session.score().lengthSeconds(), seconds);
    lastScrollAt = juce::Time::getMillisecondCounterHiRes() / 1000.0;
    session.seekToTime (scrollTime);
    wasFinished = false;
    displayTime = scrollTime;
    roll.setTime (displayTime);
    refresh();
}

void PracticeScreen::endScroll()
{
    if (! scrolling)
        return;
    scrolling = false;
    lastFrame = 0.0;
    if (std::exchange (resumeListening, false))
        startListening (std::max (0.0, scrollTime));
    else
        refresh();
    repaint (progressArea);
}

bool PracticeScreen::overProgress (juce::Point<float> p) const
{
    return session.hasPiece() && progressArea.toFloat().contains (p)
        && p.x >= progressTrack.getX() - 8.0f && p.x <= progressTrack.getRight() + 8.0f;
}

double PracticeScreen::progressTime (float x) const
{
    const double fraction = juce::jlimit (0.0, 1.0, (double) ((x - progressTrack.getX()) / juce::jmax (1.0f, progressTrack.getWidth())));
    return fraction * session.score().lengthSeconds();
}

void PracticeScreen::mouseDown (const juce::MouseEvent& e)
{
    if (overProgress (e.position))
    {
        progressDragging = true;
        scrollTo (progressTime (e.position.x));
    }
}

void PracticeScreen::mouseDrag (const juce::MouseEvent& e)
{
    if (progressDragging)
        scrollTo (progressTime (e.position.x));
}

void PracticeScreen::mouseUp (const juce::MouseEvent&)
{
    if (std::exchange (progressDragging, false))
        endScroll();
}

void PracticeScreen::mouseMove (const juce::MouseEvent& e)
{
    setMouseCursor (overProgress (e.position) ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
}

void PracticeScreen::setListenHands (piano::Hand h)
{
    bottomBar.hear.setSelectedId ((int) h + 1, juce::dontSendNotification);
    if (h == listenHands)
        return;
    listenHands = h;
    if (listening)
    {
        const double t = engine.getPlayer().position();
        engine.getPlayer().stop();
        listening = false;
        startListening (t);
    }
}

void PracticeScreen::startListening (double from)
{
    if (! session.hasPiece())
        return;

    if (from < 0.0)
        from = std::max (0.0, session.follower().currentTime() - 0.4);

    const auto& followed = session.followedTracks();
    std::vector<piano::Note> notes;
    std::vector<std::uint8_t> lit;
    for (const auto& n : session.score().notes)
    {
        const bool left = session.isLeftHandTrack (n.track);
        if (session.canChooseHand()
            && ((listenHands == piano::Hand::Left && ! left) || (listenHands == piano::Hand::Right && left)))
            continue;
        notes.push_back (n);
        lit.push_back (followed.empty() || std::find (followed.begin(), followed.end(), n.track) != followed.end() ? 1 : 0);
    }
    engine.getPlayer().start (std::move (notes), std::move (lit), from);
    listening = true;
    bottomBar.setListening (true);
    refresh();
}

void PracticeScreen::stopListening (bool reachedEnd)
{
    if (! listening)
        return;
    engine.getPlayer().stop();
    listening = false;
    bottomBar.setListening (false);
    ignoreInputUntil = juce::Time::getMillisecondCounterHiRes() / 1000.0 + 0.6;
    if (reachedEnd)
    {
        session.restart();
        wasFinished = false;
    }
    refresh();
}

bool PracticeScreen::followListening()
{
    auto& player = engine.getPlayer();
    if (! player.isPlaying())
    {
        stopListening (true);
        return false;
    }

    const double t = player.position();
    const auto before = session.follower().currentIndex();
    session.seekToTime (t);
    displayTime = t;
    roll.setTime (t);
    keyboard.setExpected (player.soundingPitches(), {}, {});
    std::vector<int> struck;
    player.takeStrikes (strikesSeen, struck);
    for (int p : struck)
        keyboard.pulse (p);
    if (session.follower().currentIndex() != before)
        refresh();
    repaint (progressArea);
    keyboard.tick();
    return true;
}

bool PracticeScreen::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::spaceKey)
    {
        toggleListening();
        return true;
    }
    if (key.getTextCharacter() == '-' || key.getTextCharacter() == '=' || key.getTextCharacter() == '+')
    {
        const auto& list = BottomBar::speeds;
        const int n = (int) std::size (list);
        int i = 0;
        while (i < n - 1 && list[i] < speedPercent) ++i;
        i = juce::jlimit (0, n - 1, i + (key.getTextCharacter() == '-' ? -1 : (list[i] > speedPercent ? 0 : 1)));
        setSpeed (list[i], true);
        return true;
    }

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
    if (c == 'r')                        { stopListening(); session.restart(); wasFinished = false; refresh(); return true; }
    if (key == juce::KeyPress::leftKey)  { stopListening(); session.step (-1); refresh(); return true; }
    if (key == juce::KeyPress::rightKey) { stopListening(); session.step (1);  refresh(); return true; }
    return false;
}

bool PracticeScreen::keyStateChanged (bool)
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

bool PracticeScreen::isInterestedInFileDrag (const juce::StringArray& files)
{
    return std::any_of (files.begin(), files.end(), [] (const juce::String& f) { return isMidiFile (f); });
}

void PracticeScreen::filesDropped (const juce::StringArray& files, int, int)
{
    for (const auto& f : files)
        if (isMidiFile (f))
        {
            if (onFileDropped) onFileDropped (juce::File (f));
            return;
        }
}

void PracticeScreen::paint (juce::Graphics& g)
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
    progressTrack = track;
    g.setColour (colour::surfaceHover);
    g.fillRoundedRectangle (track, 2.0f);

    const double length = session.hasPiece() ? session.score().lengthSeconds() : 0.0;
    if (length > 0.0)
    {
        const float x = track.getX() + track.getWidth() * (float) juce::jlimit (0.0, 1.0, displayTime / length);
        g.setColour (colour::expected);
        g.fillRoundedRectangle (track.withRight (x), 2.0f);
        const float handle = progressDragging || scrolling ? 14.0f : 10.0f;
        g.setColour (colour::textPrimary);
        g.fillEllipse (x - handle * 0.5f, track.getCentreY() - handle * 0.5f, handle, handle);
    }
}

void PracticeScreen::resized()
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
