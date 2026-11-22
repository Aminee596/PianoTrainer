#pragma once

#include "app/Icons.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <optional>

namespace app
{
class ActionButton final : public juce::Button
{
public:
    enum class Variant { Primary, Secondary, Ghost };

    ActionButton (const juce::String& label, std::optional<Icon> icon, Variant variant);
    int getIdealWidth() const;
    void setIcon (std::optional<Icon> newIcon) { icon = newIcon; repaint(); }
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;

private:
    std::optional<Icon> icon;
    Variant variant;
};

class IconButton final : public juce::Button
{
public:
    explicit IconButton (Icon icon, const juce::String& tooltip = {});
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;

private:
    Icon icon;
};

class SegmentedControl final : public juce::Component
{
public:
    void setOptions (const juce::StringArray& labels);
    void setSelected (int index);
    int  getSelected() const noexcept { return selected; }
    void setOptionEnabled (int index, bool enabled);
    int  getIdealWidth() const;

    std::function<void (int)> onChange;

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> segmentBounds (int index) const;
    int indexAt (juce::Point<float>) const;

    juce::StringArray options;
    juce::Array<bool> enabled;
    int selected = 0;
    int hovered = -1;
};

class LevelMeter final : public juce::Component
{
public:
    void setLevelDb (float db);
    void paint (juce::Graphics&) override;

private:
    float levelDb = -100.0f;
};

}
