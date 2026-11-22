#include "app/Icons.h"

#include <map>

namespace app
{
namespace
{
    const char* pathData (Icon icon)
    {
        switch (icon)
        {
            case Icon::Mic:          return "M12 2a3 3 0 0 0-3 3v7a3 3 0 0 0 6 0V5a3 3 0 0 0-3-3Z M19 10v2a7 7 0 0 1-14 0v-2 M12 19v3";
            case Icon::Sliders:      return "M21 4h-7 M10 4H3 M21 12h-9 M8 12H3 M21 20h-5 M12 20H3 M14 2v4 M8 10v4 M16 18v4";
            case Icon::Restart:      return "M3 12a9 9 0 1 0 9-9 9.75 9.75 0 0 0-6.74 2.74L3 8 M3 3v5h5";
            case Icon::ChevronLeft:  return "m15 18-6-6 6-6";
            case Icon::ChevronRight: return "m9 18 6-6-6-6";
            case Icon::ChevronDown:  return "m6 9 6 6 6-6";
            case Icon::FolderOpen:   return "m6 14 1.5-2.9A2 2 0 0 1 9.24 10H20a2 2 0 0 1 1.94 2.5l-1.54 6a2 2 0 0 1-1.95 1.5H4a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h3.9a2 2 0 0 1 1.69.9l.81 1.2a2 2 0 0 0 1.67.9H18a2 2 0 0 1 2 2v2";
            case Icon::Play:         return "M6 3 20 12 6 21Z";
            case Icon::Stop:         return "M7 5h10a2 2 0 0 1 2 2v10a2 2 0 0 1-2 2H7a2 2 0 0 1-2-2V7a2 2 0 0 1 2-2Z";
            case Icon::Library:      return "m16 6 4 14 M12 6v14 M8 8v12 M4 4v16";
            case Icon::Music:        return "M9 18V5l12-2v13 M3 18a3 3 0 1 0 6 0a3 3 0 1 0-6 0 M15 16a3 3 0 1 0 6 0a3 3 0 1 0-6 0";
        }
        return "";
    }

    const juce::Path& cachedPath (Icon icon)
    {
        static std::map<Icon, juce::Path> cache;
        auto it = cache.find (icon);
        if (it == cache.end())
            it = cache.emplace (icon, juce::Drawable::parseSVGPath (pathData (icon))).first;
        return it->second;
    }
}

void drawIcon (juce::Graphics& g, Icon icon, juce::Rectangle<float> area, juce::Colour colour)
{
    const float scale = juce::jmin (area.getWidth(), area.getHeight()) / 24.0f;
    auto path = cachedPath (icon);
    path.applyTransform (juce::AffineTransform::scale (scale).translated (area.getX(), area.getY()));
    g.setColour (colour);
    g.strokePath (path, juce::PathStrokeType (2.0f * scale, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

}
