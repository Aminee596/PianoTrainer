#pragma once

#include "design/Tokens.h"

#include <juce_graphics/juce_graphics.h>

namespace app::theme
{
    namespace colour
    {
        namespace t = design::colour;
        inline const juce::Colour bgApp { t::bgApp }, bgStage { t::bgStage }, surfacePanel { t::surfacePanel },
            surfaceRaised { t::surfaceRaised }, surfaceHover { t::surfaceHover }, borderSubtle { t::borderSubtle },
            borderStrong { t::borderStrong }, textPrimary { t::textPrimary }, textSecondary { t::textSecondary },
            textMuted { t::textMuted }, textOnAccent { t::textOnAccent }, expected { t::expected },
            correct { t::correct }, wrong { t::wrong }, heard { t::heard }, noteRight { t::noteRight },
            noteLeft { t::noteLeft }, noteOther { t::noteOther }, keyWhite { t::keyWhite },
            keyWhiteShade { t::keyWhiteShade }, keyBlack { t::keyBlack }, keyBlackTop { t::keyBlackTop };
    }

    struct NoteColours
    {
        juce::Colour toPlay  { design::colour::expected };
        juce::Colour played  { design::colour::correct };
        juce::Colour right   { design::colour::noteRight };
        juce::Colour left    { design::colour::noteLeft };
    };
    NoteColours& noteColours();

    namespace space  = design::space;
    namespace radius = design::radius;
    namespace layout = design::layout;

    enum class Text
    {
        TitlePiece,
        TitleApp,
        Body,
        BodyStrong,
        LabelCaps,
        MonoValue,
        MonoSmall,
        PromptLarge
    };

    juce::FontOptions font (Text style);

    juce::Typeface::Ptr defaultTypeface();

    void drawGlow (juce::Graphics& g, juce::Rectangle<float> area, float corner, juce::Colour colour,
                   float radius, float alpha);

    void drawCard (juce::Graphics& g, juce::Rectangle<float> area, float alpha = 0.92f);
}
