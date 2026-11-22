#pragma once

#include "app/Widgets.h"

namespace app
{
class BottomBar final : public juce::Component
{
public:
    BottomBar();

    IconButton restart { Icon::Restart,      "Restart (R)" };
    IconButton prev    { Icon::ChevronLeft,  "Previous step (Left arrow)" };
    IconButton next    { Icon::ChevronRight, "Next step (Right arrow)" };
    ActionButton listen { "Listen", Icon::Play, ActionButton::Variant::Secondary };

    void setListening (bool isListening);

    juce::ComboBox hear;
    juce::ComboBox speed;
    static constexpr int speeds[] = { 25, 40, 50, 60, 70, 75, 80, 85, 90, 95, 100, 110, 125, 150 };
    juce::ComboBox chords;

    void setDetectionText (const juce::String& text);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::String detection;
    juce::Rectangle<int> detectionArea;
};

}
