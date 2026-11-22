#pragma once

#include "app/PianoPlayer.h"
#include "core/NoteTracker.h"

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_events/juce_events.h>

#include <atomic>
#include <functional>
#include <vector>

class FingerprintLearner;

namespace app
{
class AudioEngine final : public juce::AudioIODeviceCallback,
                          private juce::Thread,
                          private juce::AsyncUpdater
{
public:
    AudioEngine();
    ~AudioEngine() override;

    juce::String initialise (const juce::XmlElement* savedDeviceState);

    juce::AudioDeviceManager& getDeviceManager() noexcept { return deviceManager; }

    std::function<void (const piano::NoteEvent&)> onNoteEvent;

    std::function<void (const piano::NoteEvent&)> onRejectedEvent;

    struct Meter
    {
        float levelDb   = -100.0f;
        float frequency = 0.0f;
        int   pitch     = -1;
    };
    Meter getMeter() const noexcept;

    void setWatched (const std::vector<int>& chord, const std::vector<int>& held) { listener.setWatched (chord, held); }

    void setGateDb (float db);

    void setMonitoring (bool shouldMonitor);

    void followDefaultOutput();

    void setOutputChoice (const juce::String& deviceName);
    juce::StringArray getOutputNames() const;
    juce::String getDefaultName (bool input) const;

    void ensureInput();
    juce::String getOutputName() const;

    PianoPlayer& getPlayer() noexcept { return player; }
    bool isMonitoring() const noexcept { return monitor.load(); }

    juce::StringArray getDriverNames();
    juce::String      getDriverName() const;
    void              setDriver (const juce::String& typeName);
    juce::StringArray getInputNames() const;
    juce::String      getInputName() const;
    juce::String      setInput (const juce::String& deviceName);

    void setRecording (bool shouldRecord, const juce::File& folder);

    void setFingerprints (std::shared_ptr<const piano::KeyFingerprints> f) { listener.setFingerprints (std::move (f)); }

    void setCalibrating (bool on) noexcept { calibrating = on; }
    std::function<void (const piano::CapturedStrike&)> onCalibrationStrike;

    void setLearner (FingerprintLearner* l) noexcept { learner = l; }

    double getSampleRate() const noexcept { return sampleRate.load(); }

    double getAnalysisWindowMs() const noexcept { return windowMs.load(); }

private:
    void audioDeviceIOCallbackWithContext (const float* const* inputs, int numInputs,
                                           float* const* outputs, int numOutputs,
                                           int numSamples,
                                           const juce::AudioIODeviceCallbackContext&) override;
    void audioDeviceAboutToStart (juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;

    void run() override;
    void handleAsyncUpdate() override;

    juce::AudioDeviceManager deviceManager;

    static constexpr int fifoSize = 1 << 16;
    juce::AbstractFifo fifo { fifoSize };
    std::vector<float> fifoBuffer = std::vector<float> ((size_t) fifoSize);

    std::atomic<double> sampleRate   { 0.0 };
    std::atomic<bool>   needsPrepare { false };
    std::atomic<double> windowMs     { 0.0 };
    std::atomic<float>  gateDb       { -50.0f };
    std::atomic<bool>   monitor      { false };
    juce::String autoInputTried, autoOutputTried;
    juce::String outputChoice;

    void updateRecorder (double rate, std::int64_t detectorSamples);
    std::atomic<bool> recordWanted { false };
    juce::CriticalSection recordFolderLock;
    juce::File recordFolder;
    juce::TimeSliceThread recorderThread { "Input recorder" };
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> recorder;

    piano::NoteTracker listener;
    PianoPlayer player;

    juce::CriticalSection eventLock;
    std::vector<piano::NoteEvent> pendingEvents;
    std::vector<piano::CapturedStrike> pendingStrikes;
    std::atomic<FingerprintLearner*> learner { nullptr };
    std::atomic<bool> calibrating { false };

    std::atomic<float> meterLevel { -100.0f };
    std::atomic<float> meterFreq  { 0.0f };
    std::atomic<int>   meterPitch { -1 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioEngine)
};

}
