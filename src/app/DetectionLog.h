#pragma once

#include "core/NoteListener.h"

#include <juce_core/juce_core.h>

#include <memory>
#include <vector>

namespace app
{
class DetectionLog
{
public:
    explicit DetectionLog (juce::File folder);

    void setEnabled (bool shouldLog);
    bool isEnabled() const noexcept { return enabled; }
    juce::File getFolder() const { return folder; }

    struct Context
    {
        juce::String piece;
        int step = -1;
        std::vector<int> expected;
        int inputShift = 0;
        juce::String result;
    };

    void write (const piano::NoteEvent& e, const Context& c);

private:
    void open();

    juce::File folder;
    bool enabled = false;
    std::unique_ptr<juce::FileOutputStream> out;
};

}
