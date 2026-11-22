#include "app/WebMenu.h"
#include "app/Theme.h"

#include <WebUI.h>

namespace app
{
namespace
{
    const char* const functions[] = { "ready", "openPiece", "continuePractice", "addPieces", "importFiles","toggleFavourite",
                                      "removePiece", "removeMissing", "removeDuplicates", "updateSettings", "resetSettings", "setDriver", "setInput",
                                      "reloadPiano", "setPianoSound", "openSoundsFolder", "openLogFolder",
                                      "startCalibration", "calibrationBack", "stopCalibration", "clearCalibration" };

    std::optional<juce::WebBrowserComponent::Resource> serve (const juce::String& url)
    {
        const auto path = url.upToFirstOccurrenceOf ("?", false, false);
        if (path == "/" || path == "/index.html")
        {
            juce::WebBrowserComponent::Resource r;
            const auto* bytes = reinterpret_cast<const std::byte*> (WebUI::index_html);
            r.data.assign (bytes, bytes + WebUI::index_htmlSize);
            r.mimeType = "text/html";
            return r;
        }
        return std::nullopt;
    }

    juce::WebBrowserComponent::Options makeOptions (const WebMenu::Handler& onCall, const juce::File& dataFolder)
    {
        auto options = juce::WebBrowserComponent::Options {}
                           .withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
                           .withWinWebView2Options (juce::WebBrowserComponent::Options::WinWebView2 {}
                                                        .withUserDataFolder (dataFolder.getChildFile ("WebView2"))
                                                        .withBackgroundColour (theme::colour::bgApp)
                                                        .withStatusBarDisabled()
                                                        .withBuiltInErrorPageDisabled())
                           .withNativeIntegrationEnabled()
                           .withKeepPageLoadedWhenBrowserIsHidden()
                           .withResourceProvider (serve);

        for (const auto* name : functions)
            options = options.withNativeFunction (name, [onCall, n = juce::String (name)] (const juce::Array<juce::var>& args,
                                                                                         WebMenu::Completion done)
                                                  {
                                                      onCall (n, args, std::move (done));
                                                  });
        return options;
    }
}

class WebMenu::Browser final : public juce::WebBrowserComponent
{
public:
    using WebBrowserComponent::WebBrowserComponent;

    bool pageAboutToLoad (const juce::String& url) override
    {
        if (isAppUrl (url))
            return true;
        juce::URL (url).launchInDefaultBrowser();
        return false;
    }

    void newWindowAttemptingToLoad (const juce::String& url) override
    {
        if (url.startsWithIgnoreCase ("http"))
            juce::URL (url).launchInDefaultBrowser();
    }

    static juce::String startUrl()
    {
       #if PIANO_UI_DEV_SERVER
        return "http://localhost:5173/";
       #else
        return getResourceProviderRoot();
       #endif
    }

private:
    static bool isAppUrl (const juce::String& url)
    {
        return url.startsWith (getResourceProviderRoot()) || url.startsWith ("about:")
            || url.startsWith ("http://localhost:5173") || url.startsWith ("data:");
    }
};

WebMenu::WebMenu (Handler onCall, const juce::File& dataFolder)
    : browser (std::make_unique<Browser> (makeOptions (onCall, dataFolder)))
{
    addAndMakeVisible (*browser);
    browser->goToURL (Browser::startUrl());
}

WebMenu::~WebMenu() = default;

void WebMenu::emit (const juce::String& eventId, const juce::var& payload)
{
    browser->emitEventIfBrowserIsVisible (juce::Identifier (eventId), payload);
}

void WebMenu::resized()
{
    browser->setBounds (getLocalBounds());
}

}
