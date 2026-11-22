#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include <functional>
#include <memory>

namespace app
{
class WebMenu final : public juce::Component
{
public:
    using Completion = juce::WebBrowserComponent::NativeFunctionCompletion;
    using Handler = std::function<void (const juce::String& name, const juce::Array<juce::var>& args, Completion done)>;

    WebMenu (Handler onCall, const juce::File& dataFolder);
    ~WebMenu() override;

    void emit (const juce::String& eventId, const juce::var& payload);

    void resized() override;

private:
    class Browser;
    std::unique_ptr<Browser> browser;
};

}
