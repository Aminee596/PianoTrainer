#pragma once

#include "app/AppSettings.h"
#include "app/AudioEngine.h"
#include "app/FingerprintLearner.h"
#include "app/DetectionLog.h"
#include "app/Library.h"
#include "app/PracticeScreen.h"
#include "app/WebMenu.h"

#include <juce_gui_extra/juce_gui_extra.h>

namespace app
{
class AppShell final : public juce::Component,
                       private juce::ChangeListener,
                       private juce::AsyncUpdater,
                       private juce::Timer
{
public:
    explicit AppShell (juce::PropertiesFile& properties);
    ~AppShell() override;

    void openFile (const juce::File& file);

    void focusCurrentScreen();

    void saveProgress();

    void resized() override;

private:
    void showMenu (const juce::String& page, const juce::String& section = {});
    void showPractice();
    bool openPiece (const juce::File& file, juce::String& error);
    void chooseFiles (bool many, std::function<void (juce::Array<juce::File>)> then);
    juce::File midiFolder() const;

    void handleCall (const juce::String& name, const juce::Array<juce::var>& args, WebMenu::Completion done);
    juce::var state();
    juce::var currentPiece() const;
    juce::var audioState();
    void pushState() { triggerAsyncUpdate(); }
    void handleAsyncUpdate() override;
    void timerCallback() override;

    void settingsChanged();
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void updateAudioError();
    void loadPiano();
    void installBuiltInPieces();
    juce::File soundsFolder() const;
    juce::var pianoState() const;

    juce::File fingerprintFile() const;
    void loadFingerprints();
    void startCalibration();
    void finishCalibration();
    void saveFingerprints();
    juce::File modelFolder() const;
    void updateLearning();
    juce::var calibrationState() const;

    juce::PropertiesFile& properties;
    AppSettings settings;
    Library library;
    DetectionLog detectionLog;
    FingerprintLearner learner;
    AudioEngine engine;
    PracticeScreen practice { engine };
    std::unique_ptr<WebMenu> menu;

    juce::TooltipWindow tooltips { this, 600 };

    bool practiceShowing = false;
    juce::String page = "home", section, openedFrom;
    std::unique_ptr<juce::FileChooser> chooser;

    struct PianoStatus { juce::String state = "builtin", name, detail; };
    PianoStatus pianoStatus;
    std::shared_ptr<const SampleSet> pianoSamples;
    juce::Array<juce::File> pianoOptions;
    juce::String pianoChoice;
    bool pianoBuiltIn = false;
    int pianoLoadId = 0;

    std::shared_ptr<const piano::KeyFingerprints> fingerprints;
    piano::KeyCalibration calibration;
    juce::String learnError;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AppShell)
};

}
