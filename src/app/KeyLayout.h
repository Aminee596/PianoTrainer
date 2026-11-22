#pragma once

#include <juce_graphics/juce_graphics.h>

namespace app
{
inline bool isBlackKey (int pitch) noexcept
{
    switch (((pitch % 12) + 12) % 12)
    {
        case 1: case 3: case 6: case 8: case 10: return true;
        default: return false;
    }
}

struct KeyLayout
{
    int   keys    = 61;
    int   lowest  = 36;
    int   highest = 96;
    float width   = 1000.0f;

    void setRange (int lo, int hi)
    {
        const int first = keys == 88 ? 21 : 36, last = keys == 88 ? 108 : 96;
        lowest  = lo >= first ? first : lo - ((lo % 12) + 12) % 12;
        highest = hi <= last  ? last  : hi + (12 - ((hi % 12) + 12) % 12) % 12;
    }

    int whiteKeysBelow (int pitch) const noexcept
    {
        int count = 0;
        for (int p = lowest; p < pitch; ++p)
            if (! isBlackKey (p)) ++count;
        return count;
    }

    int numWhiteKeys() const noexcept { return whiteKeysBelow (highest + 1); }
    float whiteWidth() const noexcept { return width / (float) juce::jmax (1, numWhiteKeys()); }

    bool contains (int pitch) const noexcept { return pitch >= lowest && pitch <= highest; }

    juce::Range<float> keyX (int pitch) const noexcept
    {
        const float w = whiteWidth();
        const float x = (float) whiteKeysBelow (pitch) * w;
        if (isBlackKey (pitch))
            return { x - 0.3f * w, x + 0.3f * w };
        return { x, x + w };
    }
};

}
