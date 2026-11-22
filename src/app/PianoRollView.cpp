#include "app/PianoRollView.h"
#include "app/Theme.h"

#include <algorithm>
#include <cmath>

namespace app
{
using namespace theme;

namespace
{
    bool has (const std::vector<int>& v, int x) { return std::find (v.begin(), v.end(), x) != v.end(); }

    float textWidth (juce::FontOptions options, const juce::String& text)
    {
        return juce::GlyphArrangement::getStringWidth (juce::Font (options), text);
    }

    void glow (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c, float radius, float alpha)
    {
        drawGlow (g, r, radius::sm, c, radius, alpha);
    }
}

void PianoRollView::setScore (const piano::Score* s, const piano::WaitModeFollower* f,
                              std::vector<int> tracks, std::vector<juce::Colour> trackColours)
{
    score = s;
    follower = f;
    followed = std::move (tracks);
    colours = std::move (trackColours);
    repaint();
}

void PianoRollView::setLayout (const KeyLayout& newLayout)
{
    layout = newLayout;
    repaint();
}

void PianoRollView::setTime (double seconds)
{
    if (std::abs (seconds - time) > 1.0e-6)
    {
        time = seconds;
        repaint();
    }
}

juce::Colour PianoRollView::handColourFor (const piano::Step& step, int pitch) const
{
    if (score != nullptr)
        for (auto i : step.noteIndices)
            if (i < score->notes.size() && score->notes[i].pitch == pitch)
            {
                const auto track = (size_t) score->notes[i].track;
                return track < colours.size() ? colours[track] : noteColours().right;
            }
    return noteColours().toPlay;
}

void PianoRollView::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    const double visible = getHeight() / (double) pixelsPerSecond;
    double seconds = -wheel.deltaY * visible * 1.2;
    if (wheel.isReversed) seconds = -seconds;
    if (onScroll && std::abs (seconds) > 1.0e-6)
        onScroll (seconds);
}

void PianoRollView::mouseDown (const juce::MouseEvent& e)
{
    dragging = true;
    lastDragY = e.position.y;
    setMouseCursor (juce::MouseCursor::DraggingHandCursor);
}

void PianoRollView::mouseDrag (const juce::MouseEvent& e)
{
    const float dy = e.position.y - lastDragY;
    lastDragY = e.position.y;
    if (onScroll && std::abs (dy) > 0.0f)
        onScroll (dy / pixelsPerSecond);
}

void PianoRollView::mouseUp (const juce::MouseEvent&)
{
    dragging = false;
    setMouseCursor (juce::MouseCursor::NormalCursor);
    if (onScrollEnd)
        onScrollEnd();
}

void PianoRollView::setDisplay (float pps, bool other, bool names)
{
    if (std::abs (pps - pixelsPerSecond) > 0.01f || other != showOther || names != showNames)
    {
        pixelsPerSecond = pps;
        showOther = other;
        showNames = names;
        repaint();
    }
}

void PianoRollView::setPrompt (const Prompt& p)
{
    if (p.caption != prompt.caption || p.big != prompt.big || p.detail != prompt.detail)
    {
        prompt = p;
        repaint();
    }
}

void PianoRollView::setStats (const Stats& s)
{
    if (s.accuracy != stats.accuracy || s.correct != stats.correct || s.wrong != stats.wrong
        || s.streak != stats.streak || s.visible != stats.visible)
    {
        stats = s;
        repaint();
    }
}

void PianoRollView::paint (juce::Graphics& g)
{
    g.fillAll (colour::bgStage);

    const float h = (float) getHeight();
    const float w = (float) getWidth();
    const float hitY = h - 4.0f;
    const auto accent = noteColours().toPlay;

    g.setColour (colour::borderSubtle);
    for (int p = layout.lowest; p <= layout.highest; ++p)
        if (p % 12 == 0)
            g.fillRect (layout.keyX (p).getStart(), 0.0f, 1.0f, h);

    if (const auto* step = follower != nullptr ? follower->currentStep() : nullptr)
        for (int p : step->required)
            if (layout.contains (p) && ! has (follower->matchedPitches(), p))
            {
                const auto c = handColourFor (*step, p);
                const auto x = layout.keyX (p);
                g.setGradientFill ({ c.withAlpha (0.0f), 0.0f, 0.0f, c.withAlpha (0.16f), 0.0f, hitY, false });
                g.fillRect (x.getStart(), 0.0f, x.getLength(), hitY);
            }

    g.setGradientFill ({ accent.withAlpha (0.0f), 0.0f, hitY - 72.0f, accent.withAlpha (0.08f), 0.0f, hitY, false });
    g.fillRect (0.0f, hitY - 72.0f, w, 72.0f);

    contacts.clear();
    if (score != nullptr && ! score->empty())
        paintNotes (g, hitY);

    g.setGradientFill ({ accent.withAlpha (0.0f), 0.0f, hitY - 12.0f, accent.withAlpha (0.45f), 0.0f, hitY, false });
    g.fillRect (0.0f, hitY - 12.0f, w, 12.0f);
    g.setColour (colour::textPrimary.withAlpha (0.85f));
    g.fillRect (0.0f, hitY, w, 2.0f);
    paintContacts (g, hitY);

    paintPrompt (g);
    paintStats (g);
}

void PianoRollView::paintNotes (juce::Graphics& g, float hitY)
{
    const auto* step = follower != nullptr ? follower->currentStep() : nullptr;
    const std::vector<int> matched = follower != nullptr ? follower->matchedPitches() : std::vector<int> {};

    auto isFollowed = [this] (int track)
    {
        return followed.empty() || std::find (followed.begin(), followed.end(), track) != followed.end();
    };

    const auto& nc = noteColours();
    const double ahead  = hitY / pixelsPerSecond;
    const auto labelFont = font (Text::MonoSmall);

    std::vector<size_t> played;
    if (follower != nullptr)
    {
        const auto& steps = follower->allSteps();
        const auto index = follower->currentIndex();
        for (size_t k = index > 48 ? index - 48 : 0; k < index && k < steps.size(); ++k)
            played.insert (played.end(), steps[k].noteIndices.begin(), steps[k].noteIndices.end());
    }
    auto holdFor = [this] (const piano::Note& n) -> const piano::Hold*
    {
        if (follower == nullptr) return nullptr;
        for (const auto& h : follower->activeHolds())
            if (h.pitch == n.pitch && std::abs (h.start - n.start) < 1.0e-6) return &h;
        return nullptr;
    };

    for (size_t i = 0; i < score->notes.size(); ++i)
    {
        const auto& n = score->notes[i];
        if (n.start > time + ahead) break;
        if (n.start + n.duration < time || ! layout.contains (n.pitch)) continue;

        const float bottom = juce::jmin (hitY, hitY - (float) ((n.start - time) * pixelsPerSecond));
        const float top    = hitY - (float) ((n.start + n.duration - time) * pixelsPerSecond) + 4.0f;
        if (bottom - top < 2.0f) continue;

        const auto x = layout.keyX (n.pitch);
        const juce::Rectangle<float> r (x.getStart() + 2.0f, top, x.getLength() - 4.0f, bottom - top);

        const bool followedNote = isFollowed (n.track);
        if (! followedNote && ! showOther) continue;
        const bool inStep = followedNote && step != nullptr
                         && std::find (step->noteIndices.begin(), step->noteIndices.end(), i) != step->noteIndices.end();
        const auto* hold    = followedNote ? holdFor (n) : nullptr;
        const bool letGo    = hold != nullptr && hold->released;
        const bool done     = ! letGo && ((inStep && has (matched, n.pitch))
                                          || (followedNote && std::find (played.begin(), played.end(), i) != played.end()));
        const bool expected = inStep && ! done && has (step->required, n.pitch);

        juce::Colour fill, label;
        if (! followedNote) { fill = colour::noteOther; label = colour::textMuted; }
        else if (letGo)     { fill = colour::wrong;     label = colour::textOnAccent.withAlpha (0.75f); }
        else
        {
            const auto c = (size_t) n.track < colours.size() ? colours[(size_t) n.track] : nc.right;
            fill  = inStep && ! expected && ! done ? c.withMultipliedAlpha (0.55f) : c;
            label = colour::textOnAccent.withAlpha (0.75f);
        }

        const bool touching = done && n.start <= time + 1.0e-3 && n.start + n.duration > time;
        if (touching)
        {
            drawGlow (g, r, radius::sm, nc.played, 10.0f, 1.6f);
            contacts.push_back (r);
        }

        if (expected) drawGlow (g, r, radius::sm, fill, 14.0f, 1.5f);
        if (letGo)    glow (g, r, colour::wrong, 12.0f, 0.6f);

        g.setColour (fill);
        g.fillRoundedRectangle (r, radius::sm);

        if (expected)
        {
            g.setColour (colour::textPrimary);
            g.drawRoundedRectangle (r, radius::sm, 2.0f);
        }
        if (touching)
        {
            g.setColour (nc.played);
            g.drawRoundedRectangle (r.expanded (1.0f), radius::sm + 1.0f, 2.5f);
        }

        if (showNames && r.getHeight() >= 26.0f && r.getWidth() >= 18.0f)
        {
            g.setColour (label);
            g.setFont (labelFont);
            g.drawText (piano::noteName (n.pitch), r.withTrimmedBottom (6.0f).removeFromBottom (14.0f),
                        juce::Justification::centred, false);
        }
    }
}

void PianoRollView::paintContacts (juce::Graphics& g, float hitY)
{
    const auto c = noteColours().played;
    for (const auto& r : contacts)
    {
        drawGlow (g, { r.getX() - 2.0f, hitY - 3.0f, r.getWidth() + 4.0f, 7.0f }, 4.0f, c, 12.0f, 1.4f);
        g.setColour (c);
        g.fillRoundedRectangle (r.getX() - 2.0f, hitY - 1.0f, r.getWidth() + 4.0f, 4.0f, 2.0f);
    }
}

void PianoRollView::paintPrompt (juce::Graphics& g)
{
    if (prompt.big.isEmpty())
        return;

    const auto capFont = font (Text::LabelCaps), bigFont = font (Text::PromptLarge), detFont = font (Text::Body);
    const float textW = juce::jmax (textWidth (capFont, prompt.caption), textWidth (bigFont, prompt.big), textWidth (detFont, prompt.detail));
    const juce::Rectangle<float> card ((float) space::s24, (float) space::s24,
                                       textW + 2.0f * space::s24 + 2.0f, 14.0f + 34.0f + 20.0f + 2.0f * space::s4 + 2.0f * space::s16);
    drawCard (g, card);

    auto content = card.reduced ((float) space::s24, (float) space::s16);
    g.setColour (prompt.captionColour);
    g.setFont (capFont);
    g.drawText (prompt.caption, content.removeFromTop (14.0f), juce::Justification::centredLeft, false);
    content.removeFromTop ((float) space::s4);
    g.setColour (colour::textPrimary);
    g.setFont (bigFont);
    g.drawText (prompt.big, content.removeFromTop (34.0f), juce::Justification::centredLeft, false);
    content.removeFromTop ((float) space::s4);
    g.setColour (colour::textSecondary);
    g.setFont (detFont);
    g.drawText (prompt.detail, content.removeFromTop (20.0f), juce::Justification::centredLeft, false);
}

void PianoRollView::paintStats (juce::Graphics& g)
{
    if (! stats.visible)
        return;

    struct Item { const char* caption; juce::String value; juce::Colour colour; };
    const Item items[] = { { "ACCURACY", stats.accuracy, colour::textPrimary },
                           { "CORRECT",  stats.correct,  colour::correct },
                           { "WRONG",    stats.wrong,    colour::wrong },
                           { "STREAK",   stats.streak,   colour::textPrimary } };

    const auto capFont = font (Text::LabelCaps), valFont = font (Text::MonoValue);
    float widths[4], total = 0.0f;
    for (int i = 0; i < 4; ++i)
    {
        widths[i] = juce::jmax (textWidth (capFont, items[i].caption), textWidth (valFont, items[i].value)) + 2.0f;
        total += widths[i];
    }
    total += 3.0f * space::s24 + 2.0f * space::s24;

    const float h = 14.0f + space::s4 + 28.0f + 2.0f * space::s16;
    const juce::Rectangle<float> card ((float) getWidth() - space::s24 - total, (float) space::s24, total, h);
    drawCard (g, card);

    float x = card.getX() + space::s24;
    const float y = card.getY() + space::s16;
    for (int i = 0; i < 4; ++i)
    {
        g.setColour (colour::textMuted);
        g.setFont (capFont);
        g.drawText (items[i].caption, juce::Rectangle<float> (x, y, widths[i], 14.0f), juce::Justification::centredLeft, false);
        g.setColour (items[i].colour);
        g.setFont (valFont);
        g.drawText (items[i].value, juce::Rectangle<float> (x, y + 14.0f + space::s4, widths[i], 28.0f), juce::Justification::centredLeft, false);
        x += widths[i] + space::s24;
    }
}

}
