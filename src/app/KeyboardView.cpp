#include "app/KeyboardView.h"
#include "app/Theme.h"
#include "core/Score.h"

#include <algorithm>

namespace app
{
using namespace theme;

namespace
{
    constexpr double flashMs = 450.0;
    constexpr double pulseMs = 110.0;
    constexpr float blackHeightRatio = 124.0f / 200.0f;

    bool has (const std::vector<int>& v, int x) { return std::find (v.begin(), v.end(), x) != v.end(); }
}

int KeyboardView::idealHeight (float whiteKeyWidth)
{
    return juce::jlimit (120, 220, juce::roundToInt (whiteKeyWidth * 5.0f));
}

void KeyboardView::setLayout (const KeyLayout& newLayout)
{
    layout = newLayout;
    repaint();
}

void KeyboardView::setExpected (std::vector<int> r, std::vector<int> c, std::vector<int> d)
{
    if (r != required || c != chord || d != done)
    {
        required = std::move (r);
        chord    = std::move (c);
        done     = std::move (d);
        repaint();
    }
}

void KeyboardView::setHeardPitch (int pitch)
{
    if (heardPitch != pitch)
    {
        heardPitch = pitch;
        repaint();
    }
}

void KeyboardView::flash (int pitch, bool correct)
{
    flashes.push_back ({ pitch, correct, juce::Time::getMillisecondCounterHiRes() });
    repaint();
}

void KeyboardView::pulse (int pitch)
{
    if (! juce::isPositiveAndBelow (pitch, 128))
        return;
    pulses[(size_t) pitch] = lastPulseMs = juce::Time::getMillisecondCounterHiRes();
    repaint();
}

void KeyboardView::tick()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    if (now - lastPulseMs < pulseMs + 20.0)
        repaint();

    if (flashes.empty())
        return;
    flashes.erase (std::remove_if (flashes.begin(), flashes.end(),
                                   [now] (const Flash& f) { return now - f.startMs > flashMs; }),
                   flashes.end());
    repaint();
}

KeyboardView::KeyLook KeyboardView::lookFor (int pitch, bool black) const
{
    KeyLook look;
    look.fill = black ? colour::keyBlack : colour::keyWhite;

    const auto& nc = noteColours();
    if (has (done, pitch))          { look.fill = nc.played; look.glow = nc.played; look.coloured = true; }
    else if (has (required, pitch)) { look.fill = nc.toPlay; look.glow = nc.toPlay; look.coloured = true; }
    else if (has (chord, pitch))    { look.overlay = nc.toPlay.withAlpha (0.38f); }

    const double now = juce::Time::getMillisecondCounterHiRes();

    if (look.coloured && juce::isPositiveAndBelow (pitch, 128))
        if (const double since = now - pulses[(size_t) pitch]; since < pulseMs)
        {
            const float lit = (float) (since / pulseMs);
            const auto plain = black ? colour::keyBlack : colour::keyWhite;
            look.fill = plain.interpolatedWith (look.fill, lit * lit);
            look.glow = look.glow.withMultipliedAlpha (lit);
        }

    for (const auto& f : flashes)
        if (f.pitch == pitch)
        {
            const float amount = juce::jlimit (0.0f, 1.0f, 1.0f - (float) ((now - f.startMs) / flashMs));
            const auto c = f.correct ? nc.played : colour::wrong;
            look.fill = look.fill.interpolatedWith (c, amount);
            look.glow = c.withMultipliedAlpha (amount);
            look.coloured = look.coloured || amount > 0.5f;
        }

    return look;
}

void KeyboardView::paintKey (juce::Graphics& g, int pitch, juce::Rectangle<float> r, bool black) const
{
    const auto look = lookFor (pitch, black);
    const float corner = black ? 4.0f : 6.0f;

    juce::Path shape;
    shape.addRoundedRectangle (r.getX(), r.getY(), r.getWidth(), r.getHeight(), corner, corner, false, false, true, true);

    if (! look.glow.isTransparent())
        drawGlow (g, r, corner, look.glow, 14.0f, 0.7f);

    g.setColour (look.fill);
    g.fillPath (shape);
    if (! look.overlay.isTransparent())
    {
        g.setColour (look.overlay);
        g.fillPath (shape);
    }

    const float lipH = black ? 10.0f : 8.0f;
    juce::Path lip;
    lip.addRoundedRectangle (r.getX(), r.getBottom() - lipH, r.getWidth(), lipH, corner, corner, false, false, true, true);
    g.setColour ((black ? colour::keyBlackTop : colour::keyWhiteShade).withAlpha (look.coloured ? 0.3f : 1.0f));
    g.fillPath (lip);

    if (! black)
    {
        g.setColour (colour::keyWhiteShade);
        g.strokePath (shape, juce::PathStrokeType (1.0f));

        if (showNames && (pitch % 12 == 0 || look.coloured))
        {
            g.setColour (look.coloured ? colour::textOnAccent : colour::textMuted);
            g.setFont (font (Text::MonoSmall));
            g.drawText (piano::noteName (pitch), r.withTrimmedBottom (16.0f).removeFromBottom (14.0f),
                        juce::Justification::centred, false);
        }
    }
}

void KeyboardView::paint (juce::Graphics& g)
{
    g.fillAll (colour::bgApp);
    const float h = (float) getHeight();
    const float blackH = std::round (h * blackHeightRatio);

    for (int p = layout.lowest; p <= layout.highest; ++p)
        if (! isBlackKey (p))
        {
            const auto x = layout.keyX (p);
            paintKey (g, p, { x.getStart(), 0.0f, x.getLength(), h }, false);
        }

    for (int p = layout.lowest; p <= layout.highest; ++p)
        if (isBlackKey (p))
        {
            const auto x = layout.keyX (p);
            paintKey (g, p, { x.getStart(), 0.0f, x.getLength(), blackH }, true);
        }

    if (layout.contains (heardPitch))
    {
        const auto x = layout.keyX (heardPitch);
        const float y = isBlackKey (heardPitch) ? blackH - 26.0f : h * 0.73f;
        const juce::Rectangle<float> dot ((x.getStart() + x.getEnd()) * 0.5f - 5.0f, y, 10.0f, 10.0f);
        drawGlow (g, dot, 5.0f, colour::heard, 6.0f, 0.8f);
        g.setColour (colour::heard);
        g.fillEllipse (dot);
    }
}

}
