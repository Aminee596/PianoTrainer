#include "app/AudioEngine.h"
#include "app/FingerprintLearner.h"

namespace app
{
AudioEngine::AudioEngine()
    : juce::Thread ("Pitch detection")
{
}

AudioEngine::~AudioEngine()
{
    deviceManager.removeAudioCallback (this);
    stopThread (2000);
    recorder = nullptr;
    recorderThread.stopThread (2000);
    cancelPendingUpdate();
}

void AudioEngine::setRecording (bool shouldRecord, const juce::File& folder)
{
    {
        const juce::ScopedLock lock (recordFolderLock);
        recordFolder = folder;
    }
    recordWanted = shouldRecord;
}

juce::String AudioEngine::initialise (const juce::XmlElement* savedDeviceState)
{
    auto error = deviceManager.initialise (2, 2, savedDeviceState, true);

   #if JUCE_WINDOWS
    if (savedDeviceState == nullptr)
        for (auto* type : deviceManager.getAvailableDeviceTypes())
            if (type->getTypeName() == "Windows Audio (Low Latency Mode)")
            {
                deviceManager.setCurrentAudioDeviceType (type->getTypeName(), true);
                break;
            }
   #endif

    followDefaultOutput();
    ensureInput();
    deviceManager.addAudioCallback (this);
    startThread (juce::Thread::Priority::highest);
    return error;
}

AudioEngine::Meter AudioEngine::getMeter() const noexcept
{
    return { meterLevel.load(), meterFreq.load(), meterPitch.load() };
}

void AudioEngine::setGateDb (float db)
{
    if (std::abs (gateDb.exchange (db) - db) > 0.01f)
        needsPrepare = true;
}

void AudioEngine::setMonitoring (bool shouldMonitor)
{
    monitor = shouldMonitor;
}

void AudioEngine::followDefaultOutput()
{
    auto* type = deviceManager.getCurrentDeviceTypeObject();
    if (type == nullptr)
        return;

    const auto outputs = type->getDeviceNames (false);
    if (outputs.isEmpty())
        return;
    const int index = type->getDefaultDeviceIndex (false);
    auto wanted = juce::isPositiveAndBelow (index, outputs.size()) ? outputs[index] : outputs[0];
    if (outputChoice.isNotEmpty() && outputs.contains (outputChoice))
        wanted = outputChoice;

    auto setup = deviceManager.getAudioDeviceSetup();
    if (setup.outputDeviceName == wanted && ! setup.outputChannels.isZero())
        return;
    if (wanted == autoOutputTried)
        return;
    autoOutputTried = wanted;

    setup.outputDeviceName = wanted;
    setup.outputChannels.clear();
    setup.outputChannels.setRange (0, 2, true);
    setup.useDefaultOutputChannels = false;
    deviceManager.setAudioDeviceSetup (setup, true);
}

void AudioEngine::setOutputChoice (const juce::String& deviceName)
{
    if (deviceName == outputChoice)
        return;
    outputChoice = deviceName;
    autoOutputTried.clear();
    followDefaultOutput();
}

juce::StringArray AudioEngine::getOutputNames() const
{
    if (auto* type = deviceManager.getCurrentDeviceTypeObject())
        return type->getDeviceNames (false);
    return {};
}

juce::String AudioEngine::getDefaultName (bool input) const
{
    if (auto* type = deviceManager.getCurrentDeviceTypeObject())
    {
        const auto names = type->getDeviceNames (input);
        const int index = type->getDefaultDeviceIndex (input);
        if (juce::isPositiveAndBelow (index, names.size()))
            return names[index];
    }
    return {};
}

void AudioEngine::ensureInput()
{
    auto* type = deviceManager.getCurrentDeviceTypeObject();
    auto setup = deviceManager.getAudioDeviceSetup();
    if (type == nullptr || (setup.inputDeviceName.isNotEmpty() && ! setup.inputChannels.isZero()))
        return;

    const auto inputs = type->getDeviceNames (true);
    if (inputs.isEmpty())
        return;

    const int index = type->getDefaultDeviceIndex (true);
    const auto wanted = juce::isPositiveAndBelow (index, inputs.size()) ? inputs[index] : inputs[0];
    if (wanted == autoInputTried)
        return;
    autoInputTried = wanted;

    setup.inputDeviceName = wanted;
    setup.useDefaultInputChannels = true;
    deviceManager.setAudioDeviceSetup (setup, true);
}

juce::String AudioEngine::getOutputName() const
{
    return deviceManager.getCurrentAudioDevice() != nullptr ? deviceManager.getAudioDeviceSetup().outputDeviceName
                                                            : juce::String();
}

juce::StringArray AudioEngine::getDriverNames()
{
    juce::StringArray names;
    for (auto* type : deviceManager.getAvailableDeviceTypes())
        if (type->getDeviceNames (true).size() > 0)
            names.add (type->getTypeName());
    return names;
}

juce::String AudioEngine::getDriverName() const
{
    return deviceManager.getCurrentAudioDeviceType();
}

void AudioEngine::setDriver (const juce::String& typeName)
{
    if (typeName != getDriverName())
    {
        deviceManager.setCurrentAudioDeviceType (typeName, true);
        followDefaultOutput();
        ensureInput();
    }
}

juce::StringArray AudioEngine::getInputNames() const
{
    if (auto* type = deviceManager.getCurrentDeviceTypeObject())
        return type->getDeviceNames (true);
    return {};
}

juce::String AudioEngine::getInputName() const
{
    return deviceManager.getCurrentAudioDevice() != nullptr ? deviceManager.getAudioDeviceSetup().inputDeviceName
                                                            : juce::String();
}

juce::String AudioEngine::setInput (const juce::String& deviceName)
{
    auto setup = deviceManager.getAudioDeviceSetup();
    setup.inputDeviceName = deviceName;
    setup.useDefaultInputChannels = true;
    return deviceManager.setAudioDeviceSetup (setup, true);
}

void AudioEngine::audioDeviceIOCallbackWithContext (const float* const* inputs, int numInputs,
                                                    float* const* outputs, int numOutputs,
                                                    int numSamples,
                                                    const juce::AudioIODeviceCallbackContext&)
{
    for (int ch = 0; ch < numOutputs; ++ch)
        if (outputs[ch] != nullptr)
            juce::FloatVectorOperations::clear (outputs[ch], numSamples);

    int active = 0;
    for (int ch = 0; ch < numInputs; ++ch)
        if (inputs[ch] != nullptr)
            ++active;

    player.render (outputs, numOutputs, numSamples);

    if (active == 0)
        return;

    const float scale = 1.0f / (float) active;

    if (monitor.load (std::memory_order_relaxed))
        for (int out = 0; out < numOutputs; ++out)
            if (outputs[out] != nullptr)
                for (int ch = 0; ch < numInputs; ++ch)
                    if (inputs[ch] != nullptr)
                        juce::FloatVectorOperations::addWithMultiply (outputs[out], inputs[ch], scale, numSamples);
    int sample = 0;

    const auto scope = fifo.write (numSamples);
    scope.forEach ([&] (int index)
    {
        float sum = 0.0f;
        for (int ch = 0; ch < numInputs; ++ch)
            if (inputs[ch] != nullptr)
                sum += inputs[ch][sample];
        fifoBuffer[(size_t) index] = sum * scale;
        ++sample;
    });
}

void AudioEngine::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    sampleRate = device->getCurrentSampleRate();
    player.prepare (device->getCurrentSampleRate(), device->getCurrentBufferSizeSamples());
    needsPrepare = true;
}

void AudioEngine::audioDeviceStopped()
{
    sampleRate = 0.0;
}

void AudioEngine::run()
{
    std::vector<float> block (2048);
    std::vector<piano::NoteEvent> events;
    std::int64_t detectorSamples = 0;

    while (! threadShouldExit())
    {
        if (needsPrepare.exchange (false))
        {
            const double sr = sampleRate.load();
            if (sr > 0.0)
            {
                piano::ListenerOptions options;
                options.gateDb = gateDb.load();
                listener.prepare (sr, options);
                windowMs = listener.latencySeconds() * 1000.0;
            }
            fifo.read (fifo.getNumReady());
            detectorSamples = 0;
            recorder = nullptr;
        }

        const int ready = fifo.getNumReady();
        if (ready == 0 || sampleRate.load() <= 0.0)
        {
            wait (1.0);
            continue;
        }

        int count = 0;
        {
            const auto scope = fifo.read (juce::jmin (ready, (int) block.size()));
            scope.forEach ([&] (int index) { block[(size_t) count++] = fifoBuffer[(size_t) index]; });
        }

        updateRecorder (sampleRate.load(), detectorSamples);
        if (recorder != nullptr)
        {
            const float* channels[] = { block.data() };
            recorder->write (channels, count);
        }
        auto* const learning = learner.load();
        const bool learnerWants = learning != nullptr && learning->wantsInput();
        if (learnerWants)
            learning->addAudio (block.data(), count, sampleRate.load(), detectorSamples);
        detectorSamples += count;

        listener.setCapture (calibrating.load() || learnerWants);
        listener.process (block.data(), count, events, &events);
        auto strikes = listener.takeCaptured();
        if (learnerWants && ! strikes.empty())
            learning->addStrikes (strikes);
        if (! strikes.empty() && calibrating.load())
        {
            {
                const juce::ScopedLock lock (eventLock);
                for (auto& s : strikes) pendingStrikes.push_back (std::move (s));
            }
            triggerAsyncUpdate();
        }

        const auto state = listener.state();
        meterLevel = state.levelDb;
        meterFreq  = state.frequency;
        meterPitch = state.pitch;

        if (! events.empty())
        {
            {
                const juce::ScopedLock lock (eventLock);
                pendingEvents.insert (pendingEvents.end(), events.begin(), events.end());
            }
            events.clear();
            triggerAsyncUpdate();
        }
    }
}

void AudioEngine::updateRecorder (double rate, std::int64_t detectorSamples)
{
    if (! recordWanted.load())
    {
        recorder = nullptr;
        return;
    }
    if (recorder != nullptr || rate <= 0.0)
        return;

    juce::File folder;
    {
        const juce::ScopedLock lock (recordFolderLock);
        folder = recordFolder;
    }
    if (folder == juce::File())
        return;
    folder.createDirectory();

    auto name = "detection-" + juce::Time::getCurrentTime().formatted ("%Y-%m-%d_%H-%M-%S");
    if (detectorSamples > 0)
        name << "_from-" << juce::String ((double) detectorSamples / rate, 1) << "s";
    const auto file = folder.getChildFile (name + ".flac").getNonexistentSibling();

    std::unique_ptr<juce::OutputStream> stream = file.createOutputStream();
    if (stream == nullptr)
    {
        recordWanted = false;
        return;
    }
    auto writer = juce::FlacAudioFormat().createWriterFor (stream, juce::AudioFormatWriterOptions {}
                                                                       .withSampleRate (rate)
                                                                       .withNumChannels (1)
                                                                       .withBitsPerSample (16));
    if (writer == nullptr)
    {
        recordWanted = false;
        return;
    }
    if (! recorderThread.isThreadRunning())
        recorderThread.startThread();
    recorder = std::make_unique<juce::AudioFormatWriter::ThreadedWriter> (writer.release(), recorderThread, (int) rate);
}

void AudioEngine::handleAsyncUpdate()
{
    std::vector<piano::NoteEvent> events;
    std::vector<piano::CapturedStrike> strikes;
    {
        const juce::ScopedLock lock (eventLock);
        events.swap (pendingEvents);
        strikes.swap (pendingStrikes);
    }
    if (onCalibrationStrike != nullptr)
        for (const auto& s : strikes)
            onCalibrationStrike (s);

    for (const auto& e : events)
    {
        if (e.rejected != piano::NoteEvent::Rejected::No)
        {
            if (onRejectedEvent != nullptr) onRejectedEvent (e);
        }
        else if (onNoteEvent != nullptr)
        {
            onNoteEvent (e);
        }
    }
}

}
