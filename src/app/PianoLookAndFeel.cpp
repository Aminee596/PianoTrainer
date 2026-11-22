#include "app/PianoLookAndFeel.h"
#include "app/Icons.h"
#include "app/Theme.h"

namespace app
{
using namespace theme;

namespace
{
    constexpr int padX = space::s12;
    constexpr int gap = space::s8;
    constexpr float chevronSize = 16.0f;

    juce::String captionOf (juce::ComboBox& box)
    {
        return box.getProperties()[PianoLookAndFeel::captionProperty].toString().toUpperCase();
    }

    float textWidth (juce::FontOptions options, const juce::String& text)
    {
        return juce::GlyphArrangement::getStringWidth (juce::Font (options), text);
    }
}

PianoLookAndFeel::PianoLookAndFeel()
{
    setDefaultSansSerifTypeface (theme::defaultTypeface());

    setColour (juce::ResizableWindow::backgroundColourId, colour::bgApp);
    setColour (juce::DocumentWindow::backgroundColourId, colour::bgApp);
    setColour (juce::Label::textColourId, colour::textPrimary);

    setColour (juce::ComboBox::textColourId, colour::textPrimary);
    setColour (juce::ComboBox::backgroundColourId, colour::surfaceRaised);
    setColour (juce::ComboBox::outlineColourId, colour::borderSubtle);

    setColour (juce::PopupMenu::backgroundColourId, colour::surfaceRaised);
    setColour (juce::PopupMenu::textColourId, colour::textPrimary);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, colour::surfaceHover);
    setColour (juce::PopupMenu::highlightedTextColourId, colour::textPrimary);

    setColour (juce::TextButton::buttonColourId, colour::surfaceRaised);
    setColour (juce::TextButton::textColourOffId, colour::textPrimary);
    setColour (juce::ListBox::backgroundColourId, colour::surfacePanel);
    setColour (juce::TextEditor::backgroundColourId, colour::surfaceRaised);
    setColour (juce::TextEditor::outlineColourId, colour::borderSubtle);
    setColour (juce::ScrollBar::thumbColourId, colour::borderStrong);

    auto scheme = getDarkColourScheme();
    scheme.setUIColour (ColourScheme::windowBackground, colour::surfacePanel);
    scheme.setUIColour (ColourScheme::widgetBackground, colour::surfaceRaised);
    scheme.setUIColour (ColourScheme::menuBackground, colour::surfaceRaised);
    scheme.setUIColour (ColourScheme::outline, colour::borderSubtle);
    scheme.setUIColour (ColourScheme::defaultText, colour::textPrimary);
    scheme.setUIColour (ColourScheme::defaultFill, colour::surfaceHover);
    scheme.setUIColour (ColourScheme::highlightedText, colour::textOnAccent);
    scheme.setUIColour (ColourScheme::highlightedFill, colour::expected);
    scheme.setUIColour (ColourScheme::menuText, colour::textPrimary);
    setColourScheme (scheme);
}

void PianoLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box)
{
    const auto bounds = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (0.5f);
    const bool hot = box.isMouseOver (true) || box.isPopupActive();

    g.setColour (hot ? colour::surfaceHover : colour::surfaceRaised);
    g.fillRoundedRectangle (bounds, radius::md);
    g.setColour (colour::borderSubtle);
    g.drawRoundedRectangle (bounds, radius::md, 1.0f);

    if (const auto caption = captionOf (box); caption.isNotEmpty())
    {
        g.setColour (colour::textMuted);
        g.setFont (font (Text::LabelCaps));
        g.drawText (caption, juce::Rectangle<int> (padX, 0, width, height), juce::Justification::centredLeft, false);
    }

    drawIcon (g, Icon::ChevronDown,
              { (float) width - padX - chevronSize, ((float) height - chevronSize) * 0.5f, chevronSize, chevronSize },
              colour::textMuted);
}

juce::Font PianoLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return font (Text::BodyStrong);
}

void PianoLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    const auto caption = captionOf (box);
    const int captionW = caption.isEmpty() ? 0 : (int) std::ceil (textWidth (font (Text::LabelCaps), caption)) + gap;
    const int left = padX + captionW;
    const int right = padX + (int) chevronSize + gap;

    label.setBounds (left, 0, juce::jmax (0, box.getWidth() - left - right), box.getHeight());
    label.setBorderSize (juce::BorderSize<int> (0));
    label.setFont (getComboBoxFont (box));
    label.setJustificationType (juce::Justification::centredLeft);
}

int PianoLookAndFeel::idealComboWidth (juce::ComboBox& box)
{
    const auto caption = captionOf (box);
    float widest = 0.0f;
    for (int i = 0; i < box.getNumItems(); ++i)
        widest = juce::jmax (widest, textWidth (font (Text::BodyStrong), box.getItemText (i)));

    const float captionW = caption.isEmpty() ? 0.0f : textWidth (font (Text::LabelCaps), caption) + gap;
    return (int) std::ceil (padX + captionW + widest + gap + chevronSize + padX) + 2;
}

void PianoLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button, bool highlighted, bool)
{
    const bool on = button.getToggleState();
    const auto h = (float) button.getHeight();
    const juce::Rectangle<float> track (0.0f, (h - 18.0f) * 0.5f, 32.0f, 18.0f);

    g.setColour (on ? colour::expected : (highlighted ? colour::borderStrong : colour::surfaceHover));
    g.fillRoundedRectangle (track, 9.0f);

    const float knobX = on ? track.getRight() - 16.0f : track.getX() + 2.0f;
    g.setColour (on ? colour::textOnAccent : colour::textSecondary);
    g.fillEllipse (knobX, track.getY() + 2.0f, 14.0f, 14.0f);

    g.setColour (on || highlighted ? colour::textPrimary : colour::textSecondary);
    g.setFont (font (Text::Body));
    g.drawText (button.getButtonText(), juce::Rectangle<float> (track.getRight() + gap, 0.0f, (float) button.getWidth(), h),
                juce::Justification::centredLeft, false);
}

int PianoLookAndFeel::idealToggleWidth (juce::ToggleButton& button)
{
    return 32 + gap + (int) std::ceil (textWidth (font (Text::Body), button.getButtonText())) + 2;
}

juce::Font PianoLookAndFeel::getPopupMenuFont()
{
    return font (Text::Body);
}

void PianoLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    g.fillAll (colour::surfaceRaised);
    g.setColour (colour::borderSubtle);
    g.drawRect (0, 0, width, height, 1);
}

void PianoLookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text, int width, int height)
{
    const auto area = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height);
    g.setColour (colour::surfaceHover);
    g.fillRoundedRectangle (area, radius::sm);
    g.setColour (colour::textPrimary);
    g.setFont (font (Text::Body));
    g.drawFittedText (text, area.reduced ((float) space::s12, (float) space::s8).toNearestInt(), juce::Justification::centredLeft, 3);
}

juce::Rectangle<int> PianoLookAndFeel::getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos, juce::Rectangle<int> parentArea)
{
    const int w = juce::jmin (360, (int) std::ceil (textWidth (font (Text::Body), tipText)) + 2 * space::s12 + 2);
    const int lines = (int) std::ceil (textWidth (font (Text::Body), tipText) / (float) (360 - 2 * space::s12));
    const int h = juce::jmax (1, lines) * 20 + 2 * space::s8;
    return juce::Rectangle<int> (screenPos.x - w / 2, screenPos.y + 18, w, h).constrainedWithin (parentArea);
}

}
