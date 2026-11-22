#include "app/AppShell.h"
#include "app/DarkTitleBar.h"
#include "app/PianoLookAndFeel.h"
#include "app/Theme.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace app
{
class MainWindow final : public juce::DocumentWindow
{
public:
    MainWindow (const juce::String& name, juce::PropertiesFile& settings)
        : DocumentWindow (name, theme::colour::bgApp, DocumentWindow::allButtons), properties (settings)
    {
        setUsingNativeTitleBar (true);
        setContentOwned (new AppShell (settings), true);
        setResizable (true, true);
        setResizeLimits (1100, 680, 10000, 10000);
        centreWithSize (getWidth(), getHeight());
        if (const auto saved = properties.getValue ("windowState"); saved.isNotEmpty())
            restoreWindowStateFromString (saved);
        setVisible (true);
        darkTitleBar();
        bringToFront();
        juce::Timer::callAfterDelay (800, [safe = juce::Component::SafePointer<MainWindow> (this)]
        {
            if (safe != nullptr) safe->bringToFront();
        });
    }

    void bringToFront()
    {
        if (isMinimised())
            setMinimised (false);
        toFront (true);
        if (auto* peer = getPeer())
            bringToForeground (peer->getNativeHandle());
    }

    void activeWindowStatusChanged() override
    {
        if (isActiveWindow())
            if (auto* c = content())
                c->focusCurrentScreen();
    }

    AppShell* content() { return dynamic_cast<AppShell*> (getContentComponent()); }

    void parentHierarchyChanged() override
    {
        DocumentWindow::parentHierarchyChanged();
        darkTitleBar();
    }

private:
    void darkTitleBar()
    {
        if (auto* peer = getPeer())
            useDarkTitleBar (peer->getNativeHandle(), 0x171B24);
    }

public:

    void closeButtonPressed() override
    {
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
    }

    ~MainWindow() override
    {
        properties.setValue ("windowState", getWindowStateAsString());
    }

private:
    juce::PropertiesFile& properties;
};

class PianoTrainerApplication final : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override    { return JUCE_APPLICATION_NAME_STRING; }
    const juce::String getApplicationVersion() override { return JUCE_APPLICATION_VERSION_STRING; }
    bool moreThanOneInstanceAllowed() override          { return false; }

    void initialise (const juce::String& commandLine) override
    {
        juce::PropertiesFile::Options options;
        options.applicationName     = "PianoTrainer";
        options.filenameSuffix      = ".settings";
        options.folderName          = "PianoTrainer";
        options.osxLibrarySubFolder = "Application Support";
        properties.setStorageParameters (options);

        lookAndFeel = std::make_unique<PianoLookAndFeel>();
        juce::LookAndFeel::setDefaultLookAndFeel (lookAndFeel.get());

        mainWindow = std::make_unique<MainWindow> (getApplicationName(), *properties.getUserSettings());
        openFromCommandLine (commandLine);
    }

    void shutdown() override
    {
        if (mainWindow != nullptr)
            if (auto* c = mainWindow->content())
                c->saveProgress();
        mainWindow = nullptr;
        juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
        lookAndFeel = nullptr;
        properties.saveIfNeeded();
    }

    void systemRequestedQuit() override { quit(); }

    void anotherInstanceStarted (const juce::String& commandLine) override
    {
        if (mainWindow != nullptr)
            mainWindow->bringToFront();
        openFromCommandLine (commandLine);
    }

private:
    void openFromCommandLine (const juce::String& commandLine)
    {
        const auto path = commandLine.unquoted().trim();
        if (path.isNotEmpty() && mainWindow != nullptr)
            if (const juce::File file (path); file.existsAsFile())
                if (auto* c = mainWindow->content())
                    c->openFile (file);
    }

    juce::ApplicationProperties properties;
    std::unique_ptr<PianoLookAndFeel> lookAndFeel;
    std::unique_ptr<MainWindow> mainWindow;
};

}

START_JUCE_APPLICATION (app::PianoTrainerApplication)
