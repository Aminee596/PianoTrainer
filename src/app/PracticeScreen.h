#pragma once

#include "app/AppSettings.h"
#include "app/AudioEngine.h"
#include "app/BottomBar.h"
#include "app/KeyboardView.h"
#include "app/PianoRollView.h"
#include "app/TopBar.h"
#include "core/PracticeSession.h"

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include <map>

namespace app
{
class PracticeScreen final : public juce::Component,
                             public juce::FileDragAndDropTarget
{
public:
    explicit PracticeScreen (AudioEngine& engine);
    ~PracticeScreen() override;

    bool loadFile (const juce::File& file, juce::String& error);
    juce::File currentFile() const { return file; }

    void applySettings (const AppSettings& s);
    void setAudioError (const juce::String& error);

    void onNoteEvent (const piano::NoteEvent& e);

    const piano::PracticeSession& getSession() const noexcept { return session; }

    std::function<void()> onHome, onLibrary, onSettings, onAudioSettings, onOpenFile;
    std::function<void (const juce::File&)> onFileDropped;
    std::function<void (const piano::SessionOptions&)> onOptionsChanged;
    std::function<void (piano::Hand)> onListenHandsChanged;
    std::function<void (piano::Hand)> onHandChanged;
    std::function<void (int)> onSpeedChanged;

    struct Judged { juce::String result; std::vector<int> expected; int step = -1; juce::String piece; int inputShift = 0; };
    std::function<void (const piano::NoteEvent&, const Judged&)> onJudged;
    Judged context() const;
    std::function<void()> onRunFinished;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void visibilityChanged() override;
    bool keyPressed (const juce::KeyPress& key) override;
    bool keyStateChanged (bool isKeyDown) override;

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

private:
    void frame (double timestampSeconds);
    bool followListening();

    void toggleListening() { if (listening) stopListening(); else startListening(); }
    void startListening (double from = -1.0);
    void stopListening (bool reachedEnd = false);

    void bottomBarChanged();
    void updateRoll();
    void refresh();
    void refreshInputChip();
    void syncWatched();
    void setOptions (const piano::SessionOptions& o);

    AudioEngine& engine;

    piano::PracticeSession session;
    piano::Hand preferredHand = piano::Hand::Right;
    juce::File file;
    juce::String errorTitle, errorDetail, audioError;
    bool wasFinished = false;
    bool listening = false;
    piano::Hand listenHands = piano::Hand::Both;
    void setListenHands (piano::Hand h);

    void setSpeed (int percent, bool notify);
    void scrollBy (double seconds) { scrollTo ((scrolling ? scrollTime : displayTime) + seconds); }
    void scrollTo (double seconds);
    void endScroll();
    bool overProgress (juce::Point<float> p) const;
    double progressTime (float x) const;

    int speedPercent = 100;
    bool scrolling = false, resumeListening = false, progressDragging = false;
    double scrollTime = 0.0, lastScrollAt = 0.0;
    juce::Rectangle<float> progressTrack;
    double ignoreInputUntil = 0.0;
    int typingOctave = 0;
    std::map<int, int> typingDown;
    double lastFrame = 0.0, lastChipUpdate = 0.0;

    TopBar        topBar;
    PianoRollView roll;
    KeyboardView  keyboard;
    BottomBar     bottomBar;
    KeyLayout     layout;
    juce::Rectangle<int> progressArea;
    int pieceLowest = 60, pieceHighest = 72;

    double displayTime = 0.0;
    size_t pulsedStep = (size_t) -1;
    std::array<std::uint32_t, 128> strikesSeen {};
    juce::VBlankAttachment vblank;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PracticeScreen)
};

}
