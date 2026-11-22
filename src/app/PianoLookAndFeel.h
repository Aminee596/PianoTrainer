#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace app
{
class PianoLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    static inline const juce::Identifier captionProperty { "caption" };

    PianoLookAndFeel();

    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    static int idealComboWidth (juce::ComboBox&);

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    static int idealToggleWidth (juce::ToggleButton&);

    juce::Font getPopupMenuFont() override;
    void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;
    void drawTooltip (juce::Graphics&, const juce::String& text, int width, int height) override;
    juce::Rectangle<int> getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos, juce::Rectangle<int> parentArea) override;
};

}
