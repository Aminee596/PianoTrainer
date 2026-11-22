#pragma once

#include "app/KeyLayout.h"
#include "core/Score.h"
#include "core/WaitModeFollower.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <vector>

namespace app
{
class PianoRollView final : public juce::Component
{
public:
    void setScore (const piano::Score* score, const piano::WaitModeFollower* follower,
                   std::vector<int> followedTracks, std::vector<juce::Colour> trackColours);
    void setLayout (const KeyLayout& newLayout);
    void setTime (double seconds);

    void setDisplay (float fallingPixelsPerSecond, bool showOtherHand, bool noteNames);

    struct Prompt
    {
        juce::String caption, big, detail;
        juce::Colour captionColour;
    };
    void setPrompt (const Prompt& p);

    struct Stats
    {
        juce::String accuracy, correct, wrong, streak;
        bool visible = false;
    };
    void setStats (const Stats& s);

    std::function<void (double)> onScroll;
    std::function<void()> onScrollEnd;
    bool isDragging() const noexcept { return dragging; }

    void paint (juce::Graphics& g) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void paintNotes (juce::Graphics& g, float hitY);
    void paintContacts (juce::Graphics& g, float hitY);
    juce::Colour handColourFor (const piano::Step& step, int pitch) const;

    std::vector<juce::Rectangle<float>> contacts;
    void paintPrompt (juce::Graphics& g);
    void paintStats (juce::Graphics& g);

    const piano::Score* score = nullptr;
    const piano::WaitModeFollower* follower = nullptr;
    std::vector<int> followed;
    std::vector<juce::Colour> colours;
    KeyLayout layout;
    double time = 0.0;
    float pixelsPerSecond = 160.0f;
    bool showOther = true, showNames = true;

    bool dragging = false;
    float lastDragY = 0.0f;

    Prompt prompt;
    Stats stats;
};

}
