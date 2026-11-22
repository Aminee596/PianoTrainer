#include "app/AppShell.h"
#include "app/Embedded.h"
#include "app/Theme.h"

#include <WebUI.h>

namespace app
{
namespace
{
    juce::var object (std::initializer_list<std::pair<const char*, juce::var>> properties)
    {
        auto* o = new juce::DynamicObject();
        for (const auto& [key, value] : properties)
            o->setProperty (key, value);
        return juce::var (o);
    }

    juce::var toArray (const juce::StringArray& strings)
    {
        juce::Array<juce::var> list;
        for (const auto& s : strings)
            list.add (s);
        return list;
    }

    bool isMidiFile (const juce::File& f)
    {
        return f.hasFileExtension ("mid;midi");
    }
}

AppShell::AppShell (juce::PropertiesFile& p)
    : properties (p),
      library (p.getFile().getSiblingFile ("library.json")),
      detectionLog (p.getFile().getSiblingFile ("logs"))
{
    settings.load (properties);
    settings.applyColours();

    addChildComponent (practice);
    practice.onHome          = [this] { showMenu ("home"); };
    practice.onLibrary       = [this] { showMenu ("library"); };
    practice.onSettings      = [this] { showMenu ("settings"); };
    practice.onAudioSettings = [this] { showMenu ("settings", "audio"); };
    practice.onOpenFile      = [this]
    {
        chooseFiles (false, [this] (juce::Array<juce::File> files)
        {
            juce::String error;
            if (! files.isEmpty() && ! openPiece (files[0], error))
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Piano Trainer", error);
        });
    };
    practice.onFileDropped = [this] (const juce::File& f)
    {
        juce::String error;
        if (! openPiece (f, error))
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Piano Trainer", error);
    };
    practice.onOptionsChanged = [this] (const piano::SessionOptions& o)
    {
        settings.chords     = o.chords;
        settings.inputShift = o.inputShift;
        settings.anyOctave  = o.anyOctave;
        settings.save (properties);
        pushState();
    };
    practice.onListenHandsChanged = [this] (piano::Hand h)
    {
        settings.listenHands = h;
        settings.save (properties);
        pushState();
    };
    practice.onHandChanged = [this] (piano::Hand h)
    {
        settings.practiceHand = h;
        settings.save (properties);
    };
    practice.onSpeedChanged = [this] (int percent)
    {
        settings.speed = percent;
        settings.save (properties);
    };
    practice.onRunFinished = [this]
    {
        const auto& s = practice.getSession();
        library.recordRun (practice.currentFile().getFullPathName(), s.accuracyPercent(), true);
    };

    engine.onNoteEvent = [this] (const piano::NoteEvent& e)
    {
        if (practiceShowing)
        {
            practice.onNoteEvent (e);
        }
        else
        {
            auto c = practice.context();
            detectionLog.write (e, { c.piece, -1, {}, c.inputShift, "not on the practice screen" });
        }
    };
    engine.onRejectedEvent = [this] (const piano::NoteEvent& e)
    {
        const auto c = practice.context();
        detectionLog.write (e, { c.piece, practiceShowing ? c.step : -1, c.expected, c.inputShift, {} });
    };
    practice.onJudged = [this] (const piano::NoteEvent& e, const PracticeScreen::Judged& j)
    {
        detectionLog.write (e, { j.piece, j.step, j.expected, j.inputShift, j.result });
    };
    engine.onCalibrationStrike = [this] (const piano::CapturedStrike& s)
    {
        if (calibration.isRunning() && calibration.add (s))
        {
            if (calibration.isComplete())
                finishCalibration();
            pushState();
        }
    };
    loadFingerprints();
    {
        juce::String why;
        if (! learner.load (modelFolder(), why))
            learnError = why;
    }
    learner.onProgress = [this] (std::shared_ptr<const piano::KeyFingerprints> f, std::vector<int>)
    {
        fingerprints = f;
        saveFingerprints();
        engine.setFingerprints (fingerprints);
        updateLearning();
        pushState();
    };
    engine.setLearner (&learner);
    updateLearning();

    const auto savedAudio = properties.getXmlValue ("audioDevice");
    const auto audioInitError = engine.initialise (savedAudio.get());
    engine.getDeviceManager().addChangeListener (this);
    if (audioInitError.isNotEmpty())
        practice.setAudioError (audioInitError);
    else
        updateAudioError();

    settingsChanged();
    loadPiano();
    installBuiltInPieces();

    const juce::File last (properties.getValue ("lastFile"));
    if (last.existsAsFile())
    {
        juce::String ignored;
        practice.loadFile (last, ignored);
    }

    menu = std::make_unique<WebMenu> ([this] (const juce::String& name, const juce::Array<juce::var>& args, WebMenu::Completion done)
                                      {
                                          handleCall (name, args, std::move (done));
                                      },
                                      properties.getFile().getParentDirectory());
    addAndMakeVisible (*menu);
    startTimerHz (15);

    setSize (1280, 800);
}

AppShell::~AppShell()
{
    stopTimer();
    cancelPendingUpdate();
    engine.getDeviceManager().removeChangeListener (this);
    engine.onNoteEvent = nullptr;
    engine.onCalibrationStrike = nullptr;
    engine.setLearner (nullptr);
    learner.onProgress = nullptr;
}

void AppShell::resized()
{
    practice.setBounds (getLocalBounds());
    if (menu != nullptr)
        menu->setBounds (getLocalBounds());
}

void AppShell::showMenu (const juce::String& newPage, const juce::String& newSection)
{
    saveProgress();
    openedFrom = practiceShowing ? "practice" : "";
    page = newPage;
    section = newSection;
    practiceShowing = false;
    practice.setVisible (false);
    menu->setVisible (true);
    menu->emit ("navigate", object ({ { "page", page }, { "section", section }, { "from", openedFrom } }));
    pushState();
    startTimerHz (15);
}

void AppShell::showPractice()
{
    stopTimer();
    practiceShowing = true;
    menu->setVisible (false);
    practice.setVisible (true);
    practice.grabKeyboardFocus();
}

void AppShell::focusCurrentScreen()
{
    if (practiceShowing)
        practice.grabKeyboardFocus();
}

void AppShell::openFile (const juce::File& file)
{
    juce::String error;
    if (openPiece (file, error))
        showPractice();
    else
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Piano Trainer", error);
}

bool AppShell::openPiece (const juce::File& file, juce::String& error)
{
    saveProgress();
    if (! practice.loadFile (file, error))
        return false;

    const auto path = file.getFullPathName();
    if (library.find (path) == nullptr)
        library.add (file);
    library.markPlayed (path);
    properties.setValue ("lastFile", path);
    properties.setValue ("lastFolder", file.getParentDirectory().getFullPathName());
    pushState();
    return true;
}

void AppShell::saveProgress()
{
    const auto& s = practice.getSession();
    if (s.hasPiece() && ! s.follower().finished())
        library.recordRun (practice.currentFile().getFullPathName(), s.accuracyPercent(), false);
}

void AppShell::installBuiltInPieces()
{
    const void* data = nullptr;
    std::size_t size = 0;
    if (! embedded::find ("PIANO_PIECES", data, size))
        return;

    juce::MemoryInputStream stream (data, size, false);
    juce::ZipFile zip (stream);
    const auto folder = library.importFolder().getChildFile ("Built-in");

    juce::StringArray offered;
    offered.addTokens (properties.getValue ("builtInPieces"), "|", "");
    bool changed = false;

    for (int i = 0; i < zip.getNumEntries(); ++i)
    {
        const auto* entry = zip.getEntry (i);
        const auto name = entry->filename.fromLastOccurrenceOf ("/", false, false);
        if (name.isEmpty() || ! isMidiFile (juce::File::getCurrentWorkingDirectory().getChildFile (name)))
            continue;

        const auto target = folder.getChildFile (name);
        if (target.getSize() != entry->uncompressedSize)
        {
            folder.createDirectory();
            std::unique_ptr<juce::InputStream> in (zip.createStreamForEntry (i));
            juce::MemoryBlock bytes;
            if (in == nullptr || in->readIntoMemoryBlock (bytes) == 0 || ! target.replaceWithData (bytes.getData(), bytes.getSize()))
                continue;
        }

        if (! offered.contains (name))
        {
            library.add (target);
            offered.add (name);
            changed = true;
        }
    }

    if (changed)
    {
        properties.setValue ("builtInPieces", offered.joinIntoString ("|"));
        properties.saveIfNeeded();
    }
}

juce::File AppShell::midiFolder() const
{
    const auto exeDir = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getParentDirectory();
    for (auto dir = exeDir; dir.getParentDirectory() != dir; dir = dir.getParentDirectory())
        if (dir.getChildFile ("CMakeLists.txt").existsAsFile() && dir.getChildFile ("assets").isDirectory())
            return dir.getChildFile ("assets");
    if (exeDir.getChildFile ("assets").isDirectory())
        return exeDir.getChildFile ("assets");
    return library.importFolder();
}

void AppShell::chooseFiles (bool many, std::function<void (juce::Array<juce::File>)> then)
{
    auto folder = midiFolder();
    folder.createDirectory();

    auto chooserFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
    if (many)
        chooserFlags |= juce::FileBrowserComponent::canSelectMultipleItems;

    chooser = std::make_unique<juce::FileChooser> (many ? "Add MIDI files to the library" : "Open a MIDI file", folder, "*.mid;*.midi");
    chooser->launchAsync (chooserFlags, [then = std::move (then)] (const juce::FileChooser& fc)
    {
        then (fc.getResults());
    });
}

void AppShell::handleCall (const juce::String& name, const juce::Array<juce::var>& args, WebMenu::Completion done)
{
    const auto arg = [&] (int i) { return i < args.size() ? args[i] : juce::var(); };
    auto reply = [this, done] (const juce::String& error = {})
    {
        done (object ({ { "ok", error.isEmpty() }, { "error", error }, { "state", state() } }));
    };
    juce::Component::SafePointer<AppShell> safe (this);
    auto later = [safe] (std::function<void (AppShell&)> f)
    {
        juce::MessageManager::callAsync ([safe, f] { if (safe != nullptr) f (*safe); });
    };

    if (name == "ready")
    {
        reply();
    }
    else if (name == "openPiece")
    {
        juce::String error;
        if (openPiece (juce::File (arg (0).toString()), error))
        {
            reply();
            later ([] (AppShell& s) { s.showPractice(); });
        }
        else
        {
            reply (error);
        }
    }
    else if (name == "continuePractice")
    {
        if (practice.getSession().hasPiece())
        {
            reply();
            later ([] (AppShell& s) { s.showPractice(); });
        }
        else
        {
            reply ("No piece is open");
        }
    }
    else if (name == "addPieces")
    {
        chooseFiles (true, [this, reply] (juce::Array<juce::File> files)
        {
            juce::StringArray errors;
            for (const auto& f : files)
                if (const auto e = library.add (f); e.isNotEmpty())
                    errors.add (e);
            reply (errors.joinIntoString ("\n"));
        });
    }
    else if (name == "importFiles")
    {
        juce::StringArray errors;
        const auto folder = library.importFolder();
        folder.createDirectory();
        if (const auto* list = arg (0).getArray())
            for (const auto& item : *list)
            {
                const auto fileName = juce::File::createLegalFileName (item["name"].toString());
                juce::MemoryBlock bytes;
                if (! isMidiFile (juce::File (fileName)) || ! bytes.fromBase64Encoding (item["data"].toString()))
                {
                    errors.add (fileName + " isn't a MIDI file");
                    continue;
                }
                auto target = folder.getChildFile (fileName);
                if (target.existsAsFile())
                {
                    juce::MemoryBlock existing;
                    if (! target.loadFileAsData (existing) || existing != bytes)
                        target = target.getNonexistentSibling();
                }
                if (! target.existsAsFile() && ! target.replaceWithData (bytes.getData(), bytes.getSize()))
                {
                    errors.add ("Couldn't save " + fileName);
                    continue;
                }
                if (const auto e = library.add (target); e.isNotEmpty())
                    errors.add (e);
            }
        reply (errors.joinIntoString ("\n"));
    }
    else if (name == "removePiece")
    {
        library.remove (arg (0).toString());
        reply();
    }
    else if (name == "removeDuplicates")
    {
        library.removeDuplicates();
        reply();
    }
    else if (name == "removeMissing")
    {
        library.removeMissing();
        reply();
    }
    else if (name == "toggleFavourite")
    {
        library.toggleFavourite (arg (0).toString());
        reply();
    }
    else if (name == "updateSettings")
    {
        settings.merge (arg (0));
        settingsChanged();
        reply();
    }
    else if (name == "resetSettings")
    {
        settings = AppSettings {};
        settingsChanged();
        reply();
    }
    else if (name == "startCalibration")
    {
        startCalibration();
        reply();
    }
    else if (name == "calibrationBack")
    {
        calibration.back();
        reply();
    }
    else if (name == "stopCalibration")
    {
        calibration.cancel();
        engine.setCalibrating (false);
        updateLearning();
        reply();
    }
    else if (name == "clearCalibration")
    {
        fingerprintFile().deleteFile();
        fingerprints = nullptr;
        engine.setFingerprints (nullptr);
        updateLearning();
        reply();
    }
    else if (name == "openLogFolder")
    {
        const auto folder = detectionLog.getFolder();
        folder.createDirectory();
        folder.startAsProcess();
        reply();
    }
    else if (name == "reloadPiano")
    {
        loadPiano();
        reply();
    }
    else if (name == "setPianoSound")
    {
        properties.setValue ("pianoSound", arg (0).toString());
        properties.saveIfNeeded();
        loadPiano();
        reply();
    }
    else if (name == "openSoundsFolder")
    {
        const auto folder = soundsFolder();
        folder.createDirectory();
        folder.startAsProcess();
        reply();
    }
    else if (name == "setDriver")
    {
        engine.setDriver (arg (0).toString());
        updateAudioError();
        reply();
    }
    else if (name == "setInput")
    {
        const auto error = engine.setInput (arg (0).toString());
        updateAudioError();
        reply (error);
    }
    else
    {
        reply ("Unknown function " + name);
    }
}

juce::var AppShell::state()
{
    return object ({ { "page", page },
                     { "section", section },
                     { "current", currentPiece() },
                     { "library", library.toVar() },
                     { "settings", settings.toVar() },
                     { "audio", audioState() },
                     { "calibration", calibrationState() } });
}

juce::var AppShell::currentPiece() const
{
    const auto& s = practice.getSession();
    if (! s.hasPiece())
        return {};

    const auto& f = s.follower();
    const auto steps = (int) f.stepCount();
    const auto index = (int) std::min (f.currentIndex(), f.stepCount());

    juce::String hand = "All notes";
    if (s.canChooseHand())
        hand = s.hand() == piano::Hand::Left ? "Left hand" : s.hand() == piano::Hand::Right ? "Right hand" : "Both hands";

    juce::Array<juce::var> preview;
    {
        const auto& notes = s.score().notes;
        const double from = f.currentTime() - 0.5, to = f.currentTime() + 4.0;
        int lo = 127, hi = 0;
        std::vector<const piano::Note*> shown;
        for (const auto& n : notes)
        {
            if (n.start > to || shown.size() >= 24) break;
            if (n.start + n.duration < from) continue;
            shown.push_back (&n);
            lo = std::min (lo, n.pitch);
            hi = std::max (hi, n.pitch);
        }
        const auto& followed = s.followedTracks();
        for (const auto* n : shown)
        {
            const bool isFollowed = followed.empty() || std::find (followed.begin(), followed.end(), n->track) != followed.end();
            preview.add (object ({ { "pitch", hi > lo ? (n->pitch - lo) / (double) (hi - lo) : 0.5 },
                                   { "start", (n->start - from) / (to - from) },
                                   { "length", n->duration / (to - from) },
                                   { "hand", ! isFollowed ? "other" : s.isLeftHandTrack (n->track) ? "left" : "right" } }));
        }
    }

    const auto& sc = s.score();
    return object ({ { "id", practice.currentFile().getFullPathName() },
                     { "title", sc.title.empty() ? practice.currentFile().getFileNameWithoutExtension() : juce::String (sc.title) },
                     { "hand", hand },
                     { "step", std::min (index + 1, steps) },
                     { "steps", steps },
                     { "progress", steps > 0 ? index / (double) steps : 0.0 },
                     { "bar", s.currentBar() },
                     { "bars", s.barCount() },
                     { "accuracy", s.accuracyPercent() },
                     { "streak", s.streak() },
                     { "finished", f.finished() },
                     { "preview", preview } });
}

juce::var AppShell::audioState()
{
    auto* device = engine.getDeviceManager().getCurrentAudioDevice();
    return object ({ { "drivers", toArray (engine.getDriverNames()) },
                     { "driver", engine.getDriverName() },
                     { "inputs", toArray (engine.getInputNames()) },
                     { "input", engine.getInputName() },
                     { "output", engine.getOutputName() },
                     { "outputs", toArray (engine.getOutputNames()) },
                     { "defaultInput", engine.getDefaultName (true) },
                     { "defaultOutput", engine.getDefaultName (false) },
                     { "open", device != nullptr },
                     { "sampleRate", device != nullptr ? device->getCurrentSampleRate() : 0.0 },
                     { "analysisMs", engine.getAnalysisWindowMs() },
                     { "piano", pianoState() },
                     { "platform", juce::SystemStats::getOperatingSystemName() } });
}

void AppShell::handleAsyncUpdate()
{
    if (menu != nullptr && menu->isVisible())
        menu->emit ("state", state());
}

void AppShell::timerCallback()
{
    if (menu == nullptr || ! menu->isVisible())
        return;
    const auto m = engine.getMeter();
    const bool open = engine.getDeviceManager().getCurrentAudioDevice() != nullptr;
    menu->emit ("meter", object ({ { "levelDb", open ? m.levelDb : -100.0f },
                                   { "note", m.pitch >= 0 ? juce::String (piano::noteName (m.pitch + 12 * settings.inputShift))
                                                          : juce::String() } }));
}

void AppShell::settingsChanged()
{
    settings.save (properties);
    settings.applyColours();
    practice.applySettings (settings);
    engine.setGateDb (settings.gateDb);
    engine.setMonitoring (settings.monitor);
    engine.setOutputChoice (settings.outputDevice);
    engine.getPlayer().setVolume (settings.listenVolume / 100.0f);
    detectionLog.setEnabled (settings.detectionLog);
    engine.setRecording (settings.detectionLog, detectionLog.getFolder());
    updateLearning();
    pushState();
}

juce::File AppShell::fingerprintFile() const
{
    return properties.getFile().getSiblingFile ("keyboard-fingerprints.bin");
}

void AppShell::loadFingerprints()
{
    juce::MemoryBlock bytes;
    auto f = std::make_shared<piano::KeyFingerprints>();
    if (fingerprintFile().loadFileAsData (bytes) && piano::KeyFingerprints::deserialise (bytes.getData(), bytes.getSize(), *f))
        fingerprints = f;
    else
        fingerprints = nullptr;
    engine.setFingerprints (fingerprints);
}

void AppShell::startCalibration()
{
    const bool full = settings.keyboardKeys == 88;
    calibration.start (full ? 21 : 36, full ? 88 : 61);
    engine.setCalibrating (true);
    updateLearning();
}

void AppShell::finishCalibration()
{
    engine.setCalibrating (false);
    auto f = calibration.build (juce::Time::getCurrentTime().formatted ("%d %b %Y").toStdString());
    calibration.cancel();
    if (f != nullptr)
    {
        fingerprints = f;
        saveFingerprints();
        engine.setFingerprints (fingerprints);
    }
    updateLearning();
}

void AppShell::saveFingerprints()
{
    if (fingerprints == nullptr)
        return;
    const auto bytes = fingerprints->serialise();
    fingerprintFile().replaceWithData (bytes.data(), bytes.size());
}

juce::File AppShell::modelFolder() const
{
    const auto exeDir = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getParentDirectory();
    for (auto dir = exeDir; dir.getParentDirectory() != dir; dir = dir.getParentDirectory())
        if (dir.getChildFile ("CMakeLists.txt").existsAsFile() && dir.getChildFile ("assets/models/transkun").isDirectory())
            return dir.getChildFile ("assets/models/transkun");
    return exeDir.getChildFile ("models/transkun");
}

void AppShell::updateLearning()
{
    const bool full = settings.keyboardKeys == 88;
    const int low = full ? 21 : 36, count = full ? 88 : 61;
    const bool sameKeys = fingerprints != nullptr && fingerprints->lowKey == low && fingerprints->keyCount == count;
    const bool needed = ! sameKeys || ! fingerprints->complete();
    if (learner.available() && needed && ! calibration.isRunning())
    {
        if (! learner.isLearning())
            learner.start (sameKeys ? fingerprints : nullptr, low, count);
    }
    else if (learner.isLearning())
        learner.stop();
}

juce::var AppShell::calibrationState() const
{
    juce::var saved;
    if (fingerprints != nullptr && fingerprints->complete())
    {
        const bool suits = fingerprints->valid();
        saved = object ({ { "keys", fingerprints->keyCount },
                          { "low", juce::String (piano::noteName (fingerprints->lowKey)) },
                          { "high", juce::String (piano::noteName (fingerprints->highKey())) },
                          { "date", juce::String (fingerprints->created) },
                          { "suits", suits },
                          { "rate", fingerprints->rate } });
    }
    const bool full = settings.keyboardKeys == 88;
    const int low = full ? 21 : 36, count = full ? 88 : 61;
    const bool sameKeys = fingerprints != nullptr && fingerprints->lowKey == low && fingerprints->keyCount == count;
    juce::Array<juce::var> missing;
    for (int p = low; p < low + count; ++p)
        if (! sameKeys || ! fingerprints->isKnown (p))
            missing.add (juce::String (piano::noteName (p)));
    const auto learning = object ({ { "available", learner.available() },
                                    { "active", learner.isLearning() },
                                    { "known", count - missing.size() },
                                    { "total", count },
                                    { "missing", missing },
                                    { "error", learnError } });

    const int key = calibration.currentKey();
    return object ({ { "running", calibration.isRunning() },
                     { "learning", learning },
                     { "index", calibration.index() },
                     { "total", calibration.total() },
                     { "key", key },
                     { "keyName", juce::String (piano::noteName (key)) },
                     { "saved", saved } });
}

juce::File AppShell::soundsFolder() const
{
    return properties.getFile().getParentDirectory().getChildFile ("Sounds");
}

void AppShell::loadPiano()
{
    const auto folder = soundsFolder();
    if (! folder.isDirectory() && folder.createDirectory())
        folder.getChildFile ("README.txt").replaceWithText (
            "Put your own piano sounds here, then choose one in Piano Trainer: Settings, Listen mode.\r\n\r\n"
            "Each sound is a folder (or a .zip of one) with an .sfz file and its samples,\r\n"
            "or with samples named by their note, like C4.wav or A#3.flac.\r\n"
            "Press Reload in Settings after adding one.\r\n");

    pianoBuiltIn = hasBuiltInPiano();
    pianoOptions = findSounds (soundFolders (properties.getFile().getParentDirectory(), ! pianoBuiltIn));

    juce::File file;
    const auto chosen = properties.getValue ("pianoSound");
    for (const auto& f : pianoOptions)
        if (chosen.isNotEmpty() && f.getFullPathName() == chosen)
            file = f;
    if (file == juce::File() && ! pianoBuiltIn && ! pianoOptions.isEmpty())
        file = pianoOptions[0];
    pianoChoice = file == juce::File() ? juce::String() : file.getFullPathName();

    const int id = ++pianoLoadId;
    if (file == juce::File() && ! pianoBuiltIn)
    {
        pianoSamples = nullptr;
        engine.getPlayer().setSamples (nullptr);
        pianoStatus = { "builtin", "Simple piano", "No piano samples found. Add an SFZ piano to " + folder.getFullPathName() + "." };
        pushState();
        return;
    }

    const auto name = file == juce::File() ? juce::String ("Salamander Grand Piano") : file.getFileNameWithoutExtension();
    pianoStatus = { "loading", name, "Loading samples..." };
    pushState();

    juce::Component::SafePointer<AppShell> safe (this);
    juce::Thread::launch ([safe, file, id]
    {
        juce::String error;
        auto set = file == juce::File() ? loadBuiltInPiano (error) : loadSampleSet (file, error);
        juce::MessageManager::callAsync ([safe, set, error, id]
        {
            if (safe == nullptr || id != safe->pianoLoadId)
                return;
            auto& s = *safe;
            s.pianoSamples = set;
            s.engine.getPlayer().setSamples (set);
            if (set != nullptr)
            {
                const auto keys = set->keyRange();
                s.pianoStatus = { "samples", set->name,
                                  juce::String ((int) set->regions.size()) + " samples, "
                                      + piano::noteName (keys.getStart()) + " to " + piano::noteName (keys.getEnd())
                                      + (set->velocityLayers ? ", several loudness levels" : "")
                                      + ", " + juce::String (juce::roundToInt ((double) set->bytes / (1024.0 * 1024.0))) + " MB" };
            }
            else
            {
                s.pianoStatus = { "builtin", "Simple piano", error };
            }
            s.pushState();
        });
    });
}

juce::var AppShell::pianoState() const
{
    juce::Array<juce::var> options;
    if (pianoBuiltIn)
        options.add (object ({ { "value", juce::String() }, { "label", "Salamander Grand Piano (built in)" } }));
    for (const auto& f : pianoOptions)
        options.add (object ({ { "value", f.getFullPathName() }, { "label", f.getFileNameWithoutExtension() } }));
    return object ({ { "state", pianoStatus.state }, { "name", pianoStatus.name }, { "detail", pianoStatus.detail },
                     { "choice", pianoChoice }, { "options", options }, { "folder", soundsFolder().getFullPathName() } });
}

void AppShell::changeListenerCallback (juce::ChangeBroadcaster*)
{
    engine.followDefaultOutput();
    engine.ensureInput();

    if (auto xml = engine.getDeviceManager().createStateXml())
        properties.setValue ("audioDevice", xml.get());
    properties.saveIfNeeded();
    updateAudioError();
    pushState();
}

void AppShell::updateAudioError()
{
    practice.setAudioError (engine.getDeviceManager().getCurrentAudioDevice() == nullptr
                                ? "No input is open. Pick the microphone or line-in in Settings."
                                : juce::String());
}

}
