#pragma once

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
class MainComponent final : public juce::Component,
                            public juce::FileDragAndDropTarget,
                            private juce::Timer,
                            private juce::ChangeListener
{
public:
    explicit MainComponent (juce::PropertiesFile& settings);
    ~MainComponent() override;

    void loadFile (const juce::File& file);

    void paint (juce::Graphics& g) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress& key) override;
    bool keyStateChanged (bool isKeyDown) override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

private:
    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    void onNoteEvent (const piano::NoteEvent& e);
    void chooseFile();
    void showAudioSettings();
    void applyOptions();
    void updateRoll();
    void refresh();
    void refreshInputChip();
    void syncWatched();

    juce::PropertiesFile& settings;
    AudioEngine engine;

    piano::PracticeSession session;
    juce::String fileName, errorTitle, errorDetail;
    int typingOctave = 0;
    std::map<int, int> typingDown;
    double lastTickMs = 0.0;

    TopBar        topBar;
    PianoRollView roll;
    KeyboardView  keyboard;
    BottomBar     bottomBar;
    KeyLayout     layout;
    juce::Rectangle<int> progressArea;

    double displayTime = 0.0;
    int    slowTick = 0;
    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};

}
