#pragma once

#include <juce_graphics/juce_graphics.h>

namespace app
{
enum class Icon { Mic, Sliders, Restart, ChevronLeft, ChevronRight, ChevronDown, FolderOpen, Music, Play, Stop, Library };

void drawIcon (juce::Graphics& g, Icon icon, juce::Rectangle<float> area, juce::Colour colour);

}
