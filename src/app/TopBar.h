#pragma once

#include "app/Widgets.h"

namespace app
{
class TopBar final : public juce::Component
{
public:
    TopBar();
    ~TopBar() override;

    void setPiece (const juce::String& title, const juce::String& subtitle);
    const juce::String& getTitle() const noexcept { return title; }
    void setInput (const juce::String& device, const juce::String& hearing, float levelDb);

    ActionButton     home     { "Home", Icon::ChevronLeft, ActionButton::Variant::Ghost };
    ActionButton     library  { "Library", Icon::Library, ActionButton::Variant::Ghost };
    SegmentedControl hands;
    IconButton       settings { Icon::Sliders, "Settings" };
    ActionButton     open     { "Open piece", Icon::FolderOpen, ActionButton::Variant::Secondary };

    std::function<void()> onInputClicked;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    class InputChip;
    std::unique_ptr<InputChip> chip;
    juce::String title, subtitle;
};

}
