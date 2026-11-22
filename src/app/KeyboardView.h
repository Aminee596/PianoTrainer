#pragma once

#include "app/KeyLayout.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <vector>

namespace app
{
class KeyboardView final : public juce::Component
{
public:
    void setLayout (const KeyLayout& newLayout);

    void setExpected (std::vector<int> required, std::vector<int> chord, std::vector<int> done);

    void setHeardPitch (int pitch);

    void flash (int pitch, bool correct);

    void setShowNames (bool show) { if (show != showNames) { showNames = show; repaint(); } }

    void pulse (int pitch);

    void tick();

    static int idealHeight (float whiteKeyWidth);

    void paint (juce::Graphics& g) override;

private:
    struct KeyLook
    {
        juce::Colour fill, overlay;
        juce::Colour glow;
        bool coloured = false;
    };
    KeyLook lookFor (int pitch, bool black) const;
    void paintKey (juce::Graphics& g, int pitch, juce::Rectangle<float> r, bool black) const;

    KeyLayout layout;
    std::vector<int> required, chord, done;
    int heardPitch = -1;
    bool showNames = true;

    struct Flash { int pitch; bool correct; double startMs; };
    std::vector<Flash> flashes;
    std::array<double, 128> pulses {};
    double lastPulseMs = 0.0;
};

}
