#include "app/Theme.h"

#include <Fonts.h>

namespace app::theme
{
namespace
{
    struct Typefaces
    {
        static juce::Typeface::Ptr load (const char* data, int size)
        {
            return juce::Typeface::createSystemTypefaceFor (data, (size_t) size);
        }

        juce::Typeface::Ptr manropeMedium    = load (Fonts::ManropeMedium_ttf,       Fonts::ManropeMedium_ttfSize);
        juce::Typeface::Ptr manropeSemiBold  = load (Fonts::ManropeSemiBold_ttf,     Fonts::ManropeSemiBold_ttfSize);
        juce::Typeface::Ptr manropeBold      = load (Fonts::ManropeBold_ttf,         Fonts::ManropeBold_ttfSize);
        juce::Typeface::Ptr manropeExtraBold = load (Fonts::ManropeExtraBold_ttf,    Fonts::ManropeExtraBold_ttfSize);
        juce::Typeface::Ptr monoMedium       = load (Fonts::JetBrainsMonoMedium_ttf, Fonts::JetBrainsMonoMedium_ttfSize);
        juce::Typeface::Ptr monoBold         = load (Fonts::JetBrainsMonoBold_ttf,   Fonts::JetBrainsMonoBold_ttfSize);
    };

    const Typefaces& typefaces()
    {
        static const Typefaces t;
        return t;
    }

    juce::FontOptions make (juce::Typeface::Ptr face, float size, float trackingPercent = 0.0f)
    {
        return juce::FontOptions (face).withPointHeight (size).withKerningFactor (trackingPercent / 100.0f);
    }
}

juce::FontOptions font (Text style)
{
    namespace d = design::text;
    const auto& t = typefaces();
    auto f = [] (juce::Typeface::Ptr face, d::Style st) { return make (face, st.size, st.trackingPercent); };
    switch (style)
    {
        case Text::TitlePiece:  return f (t.manropeBold,      d::titlePiece);
        case Text::TitleApp:    return f (t.manropeExtraBold, d::titleApp);
        case Text::Body:        return f (t.manropeMedium,    d::body);
        case Text::BodyStrong:  return f (t.manropeSemiBold,  d::bodyStrong);
        case Text::LabelCaps:   return f (t.manropeBold,      d::labelCaps);
        case Text::MonoValue:   return f (t.monoBold,         d::monoValue);
        case Text::MonoSmall:   return f (t.monoMedium,       d::monoSmall);
        case Text::PromptLarge: return f (t.manropeExtraBold, d::promptLarge);
    }
    return f (t.manropeMedium, d::body);
}

NoteColours& noteColours()
{
    static NoteColours colours;
    return colours;
}

juce::Typeface::Ptr defaultTypeface()
{
    return typefaces().manropeMedium;
}

void drawGlow (juce::Graphics& g, juce::Rectangle<float> area, float corner, juce::Colour colour,
               float radius, float alpha)
{
    constexpr int layers = 6;
    for (int i = layers; i >= 1; --i)
    {
        const float t = (float) i / (float) layers;
        const float grow = radius * t;
        g.setColour (colour.withMultipliedAlpha (alpha * 0.22f * (1.0f - t * 0.85f)));
        g.fillRoundedRectangle (area.expanded (grow), corner + grow);
    }
}

void drawCard (juce::Graphics& g, juce::Rectangle<float> area, float alpha)
{
    drawGlow (g, area.translated (0.0f, 8.0f), radius::lg, juce::Colours::black, 20.0f, 0.9f);
    g.setColour (colour::surfacePanel.withAlpha (alpha));
    g.fillRoundedRectangle (area, radius::lg);
    g.setColour (colour::borderSubtle);
    g.drawRoundedRectangle (area, radius::lg, 1.0f);
}

}
