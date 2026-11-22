#include "app/TopBar.h"
#include "app/Theme.h"

namespace app
{
using namespace theme;

class TopBar::InputChip final : public juce::Component,
                                 public juce::SettableTooltipClient
{
public:
    InputChip()
    {
        addAndMakeVisible (meter);
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        setTooltip ("Audio input. Click to choose the microphone or line-in.");
    }

    void set (const juce::String& d, const juce::String& h, float levelDb)
    {
        if (d != device || h != hearing)
        {
            device = d;
            hearing = h;
            repaint();
        }
        meter.setLevelDb (levelDb);
    }

    std::function<void()> onClick;

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (isMouseOver() ? colour::surfaceHover : colour::surfaceRaised);
        g.fillRoundedRectangle (bounds, radius::md);
        g.setColour (colour::borderSubtle);
        g.drawRoundedRectangle (bounds, radius::md, 1.0f);

        drawIcon (g, Icon::Mic, { (float) space::s12, (getHeight() - 20) * 0.5f, 20.0f, 20.0f }, colour::heard);

        auto text = textArea();
        g.setColour (colour::textPrimary);
        g.setFont (font (Text::BodyStrong));
        g.drawText (device, text.removeFromTop (text.getHeight() / 2 + 2), juce::Justification::bottomLeft, true);
        g.setColour (colour::textMuted);
        g.setFont (font (Text::MonoSmall));
        g.drawText (hearing, text, juce::Justification::topLeft, true);
    }

    void resized() override
    {
        meter.setBounds (getWidth() - space::s12 - 96, 0, 96, getHeight());
    }

    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override  { repaint(); }
    void mouseUp (const juce::MouseEvent& e) override
    {
        if (onClick && getLocalBounds().contains (e.getPosition()))
            onClick();
    }

private:
    juce::Rectangle<int> textArea() const
    {
        const int left = space::s12 + 20 + space::s8;
        return { left, 0, meter.getX() - space::s8 - left, getHeight() };
    }

    LevelMeter meter;
    juce::String device, hearing;
};

TopBar::TopBar()
    : chip (std::make_unique<InputChip>())
{
    hands.setOptions ({ "Left hand", "Right hand", "Both hands" });
    hands.setSelected (1);

    for (auto* c : std::initializer_list<juce::Component*> { &home, &library, &hands, chip.get(), &settings, &open })
        addAndMakeVisible (c);

    chip->onClick = [this] { if (onInputClicked) onInputClicked(); };
}

TopBar::~TopBar() = default;

void TopBar::setPiece (const juce::String& t, const juce::String& s)
{
    title = t;
    subtitle = s;
    repaint();
}

void TopBar::setInput (const juce::String& device, const juce::String& hearing, float levelDb)
{
    chip->set (device, hearing, levelDb);
}

void TopBar::paint (juce::Graphics& g)
{
    g.fillAll (colour::surfacePanel);
    g.setColour (colour::borderSubtle);
    g.fillRect (0, getHeight() - 1, getWidth(), 1);

    g.setColour (colour::borderSubtle);
    g.fillRect (library.getRight() + space::s12, getHeight() / 2 - 12, 1, 24);

    const juce::Rectangle<float> mark ((float) (library.getRight() + space::s24 + 1), (getHeight() - 36) * 0.5f, 36.0f, 36.0f);
    g.setColour (colour::expected);
    g.fillRoundedRectangle (mark, radius::md);
    drawIcon (g, Icon::Music, mark.withSizeKeepingCentre (20.0f, 20.0f), colour::textOnAccent);

    const int x = (int) mark.getRight() + space::s12;
    const int w = hands.getX() - space::s16 - x;
    const int mid = getHeight() / 2;
    g.setColour (colour::textPrimary);
    g.setFont (font (Text::TitlePiece));
    g.drawText (title, x, mid - 22, w, 24, juce::Justification::bottomLeft, true);
    g.setColour (colour::textMuted);
    g.setFont (font (Text::Body));
    g.drawText (subtitle, x, mid + 1, w, 20, juce::Justification::topLeft, true);
}

void TopBar::resized()
{
    auto area = getLocalBounds().reduced (space::s24, space::s12);
    home.setBounds (area.getX() - space::s8, (getHeight() - 38) / 2, home.getIdealWidth(), 38);
    library.setBounds (home.getRight() + space::s4, (getHeight() - 38) / 2, library.getIdealWidth(), 38);

    open.setBounds (area.removeFromRight (open.getIdealWidth()).withSizeKeepingCentre (open.getIdealWidth(), 38));
    area.removeFromRight (space::s12);
    settings.setBounds (area.removeFromRight (38).withSizeKeepingCentre (38, 38));
    area.removeFromRight (space::s12);
    chip->setBounds (area.removeFromRight (290).withSizeKeepingCentre (290, 42));

    const int hw = hands.getIdealWidth();
    hands.setBounds ((getWidth() - hw) / 2, (getHeight() - 38) / 2, hw, 38);
}

}
