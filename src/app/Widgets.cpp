#include "app/Widgets.h"
#include "app/Theme.h"

namespace app
{
using namespace theme;

namespace
{
    constexpr float iconSize = 20.0f;

    float textWidth (juce::FontOptions options, const juce::String& text)
    {
        return juce::GlyphArrangement::getStringWidth (juce::Font (options), text);
    }
}

ActionButton::ActionButton (const juce::String& label, std::optional<Icon> i, Variant v)
    : juce::Button (label), icon (i), variant (v)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

int ActionButton::getIdealWidth() const
{
    const float iconPart = icon ? iconSize + space::s8 : 0.0f;
    return (int) std::ceil (space::s16 * 2 + iconPart + textWidth (font (Text::BodyStrong), getButtonText())) + 2;
}

void ActionButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    const auto bounds = getLocalBounds().toFloat().reduced (0.5f);
    juce::Colour fill, fg;

    switch (variant)
    {
        case Variant::Primary:   fill = colour::expected;      fg = colour::textOnAccent;  break;
        case Variant::Secondary: fill = colour::surfaceRaised; fg = colour::textPrimary;   break;
        case Variant::Ghost:     fill = juce::Colours::transparentBlack; fg = colour::textSecondary; break;
    }
    if (highlighted) fill = variant == Variant::Primary ? fill.brighter (0.12f) : colour::surfaceHover;
    if (down)        fill = fill.darker (0.15f);
    if (! isEnabled()) fg = colour::textMuted;

    g.setColour (fill);
    g.fillRoundedRectangle (bounds, radius::md);
    if (variant == Variant::Secondary)
    {
        g.setColour (colour::borderSubtle);
        g.drawRoundedRectangle (bounds, radius::md, 1.0f);
    }

    auto content = getLocalBounds().toFloat().reduced ((float) space::s16, 0.0f);
    if (icon)
    {
        drawIcon (g, *icon, content.removeFromLeft (iconSize).withSizeKeepingCentre (iconSize, iconSize), fg);
        content.removeFromLeft ((float) space::s8);
    }
    g.setColour (fg);
    g.setFont (font (Text::BodyStrong));
    g.drawText (getButtonText(), content, juce::Justification::centredLeft, false);
}

IconButton::IconButton (Icon i, const juce::String& tooltip)
    : juce::Button (tooltip), icon (i)
{
    setTooltip (tooltip);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void IconButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    const auto bounds = getLocalBounds().toFloat().reduced (0.5f);
    auto fill = highlighted ? colour::surfaceHover : colour::surfaceRaised;
    if (down) fill = fill.darker (0.15f);

    g.setColour (fill);
    g.fillRoundedRectangle (bounds, radius::md);
    g.setColour (colour::borderSubtle);
    g.drawRoundedRectangle (bounds, radius::md, 1.0f);
    drawIcon (g, icon, bounds.withSizeKeepingCentre (iconSize, iconSize), colour::textPrimary);
}

void SegmentedControl::setOptions (const juce::StringArray& labels)
{
    options = labels;
    enabled.clearQuick();
    for (int i = 0; i < options.size(); ++i)
        enabled.add (true);
    selected = juce::jlimit (0, juce::jmax (0, options.size() - 1), selected);
    repaint();
}

void SegmentedControl::setSelected (int index)
{
    selected = juce::jlimit (0, juce::jmax (0, options.size() - 1), index);
    repaint();
}

void SegmentedControl::setOptionEnabled (int index, bool isEnabled)
{
    if (juce::isPositiveAndBelow (index, enabled.size()))
    {
        enabled.set (index, isEnabled);
        repaint();
    }
}

int SegmentedControl::getIdealWidth() const
{
    float w = (float) space::s4 * 2.0f + 2.0f * (float) juce::jmax (0, options.size() - 1);
    for (const auto& o : options)
        w += textWidth (font (Text::BodyStrong), o) + space::s16 * 2;
    return (int) std::ceil (w) + 2;
}

juce::Rectangle<float> SegmentedControl::segmentBounds (int index) const
{
    auto inner = getLocalBounds().toFloat().reduced ((float) space::s4);
    float x = inner.getX();
    for (int i = 0; i < options.size(); ++i)
    {
        const float w = textWidth (font (Text::BodyStrong), options[i]) + space::s16 * 2;
        if (i == index)
            return { x, inner.getY(), w, inner.getHeight() };
        x += w + 2.0f;
    }
    return {};
}

int SegmentedControl::indexAt (juce::Point<float> p) const
{
    for (int i = 0; i < options.size(); ++i)
        if (segmentBounds (i).contains (p))
            return i;
    return -1;
}

void SegmentedControl::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (colour::surfaceRaised);
    g.fillRoundedRectangle (bounds, radius::md);
    g.setColour (colour::borderSubtle);
    g.drawRoundedRectangle (bounds, radius::md, 1.0f);

    for (int i = 0; i < options.size(); ++i)
    {
        const auto r = segmentBounds (i);
        const bool isOn = i == selected;

        if (isOn)
        {
            juce::Path p;
            p.addRoundedRectangle (r, radius::sm);
            juce::DropShadow (juce::Colours::black.withAlpha (0.35f), 3, { 0, 1 }).drawForPath (g, p);
            g.setColour (colour::surfaceHover);
            g.fillPath (p);
        }
        else if (i == hovered && enabled[i])
        {
            g.setColour (colour::surfaceHover.withAlpha (0.45f));
            g.fillRoundedRectangle (r, radius::sm);
        }

        g.setColour (! enabled[i] ? colour::textMuted : isOn ? colour::textPrimary : colour::textSecondary);
        g.setFont (font (Text::BodyStrong));
        g.drawText (options[i], r, juce::Justification::centred, false);
    }
}

void SegmentedControl::mouseMove (const juce::MouseEvent& e)
{
    const int h = indexAt (e.position);
    if (h != hovered)
    {
        hovered = h;
        setMouseCursor (h >= 0 && enabled[h] ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void SegmentedControl::mouseExit (const juce::MouseEvent&)
{
    hovered = -1;
    repaint();
}

void SegmentedControl::mouseUp (const juce::MouseEvent& e)
{
    const int i = indexAt (e.position);
    if (i >= 0 && enabled[i] && i != selected)
    {
        selected = i;
        repaint();
        if (onChange) onChange (i);
    }
}

void LevelMeter::setLevelDb (float db)
{
    if (std::abs (db - levelDb) > 0.5f)
    {
        levelDb = db;
        repaint();
    }
}

void LevelMeter::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().withSizeKeepingCentre ((float) getWidth(), 6.0f);
    g.setColour (colour::surfaceHover);
    g.fillRoundedRectangle (bounds, 3.0f);

    const float fraction = juce::jlimit (0.0f, 1.0f, (levelDb + 70.0f) / 70.0f);
    if (fraction > 0.0f)
    {
        g.setColour (levelDb > -6.0f ? colour::wrong : colour::heard);
        g.fillRoundedRectangle (bounds.withWidth (juce::jmax (6.0f, bounds.getWidth() * fraction)), 3.0f);
    }
}

}
