#pragma once

#include <juce_core/juce_core.h>

#include <vector>

namespace app
{
struct Piece
{
    juce::String path;
    juce::String title;
    int    hands      = 1;
    double length     = 0.0;
    int    bars       = 0;
    int    best       = -1;
    int    last       = -1;
    bool   favourite  = false;
    juce::int64 lastPlayed = 0;
    juce::int64 added      = 0;
    juce::String hash;

    juce::File file() const { return juce::File (path); }
};

class Library
{
public:
    explicit Library (juce::File storageFile);

    const std::vector<Piece>& pieces() const noexcept { return items; }
    const Piece* find (const juce::String& path) const;

    juce::String add (const juce::File& file);

    void remove (const juce::String& path);
    int removeMissing();
    int removeDuplicates();
    void markPlayed (const juce::String& path);
    void recordRun (const juce::String& path, int accuracy, bool finished);
    void toggleFavourite (const juce::String& path);

    juce::File importFolder() const;

    juce::var toVar() const;

private:
    Piece* findMutable (const juce::String& path);
    void load();
    void save() const;

    juce::File storage;
    std::vector<Piece> items;
};

}
