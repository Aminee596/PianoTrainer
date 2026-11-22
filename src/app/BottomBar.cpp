#include "app/BottomBar.h"
#include "app/PianoLookAndFeel.h"
#include "app/Theme.h"

namespace app
{
using namespace theme;

BottomBar::BottomBar()
{
    chords.getProperties().set (PianoLookAndFeel::captionProperty, "Chords");
    hear.getProperties().set (PianoLookAndFeel::captionProperty, "Hear");
    speed.getProperties().set (PianoLookAndFeel::captionProperty, "Speed");
    for (int percent : speeds)
        speed.addItem (juce::String (percent) + "%", percent);
    speed.setSelectedId (100, juce::dontSendNotification);
    speed.setTooltip ("Practice and listen slower or faster (- and = keys). The pitch stays the same.");
    chords.setTooltip ("Which notes of a chord you have to play.");
    hear.setTooltip ("Which hand listen mode plays. Only the hand you practise lights up.");
    listen.setTooltip ("Hear the piece from here (Space). Stop to play it yourself from where it stopped.");

    for (auto* c : std::initializer_list<juce::Component*> { &restart, &prev, &next, &listen, &hear, &speed, &chords })
        addAndMakeVisible (c);
}

void BottomBar::setListening (bool isListening)
{
    listen.setButtonText (isListening ? "Stop" : "Listen");
    listen.setIcon (isListening ? Icon::Stop : Icon::Play);
}

void BottomBar::setDetectionText (const juce::String& text)
{
    if (text != detection)
    {
        detection = text;
        repaint (detectionArea);
    }
}

void BottomBar::paint (juce::Graphics& g)
{
    g.fillAll (colour::surfacePanel);
    g.setColour (colour::borderSubtle);
    g.fillRect (0, 0, getWidth(), 1);

    g.setColour (colour::textMuted);
    g.setFont (font (Text::MonoSmall));
    g.drawText (detection, detectionArea, juce::Justification::centredRight, true);
}

void BottomBar::resized()
{
    auto area = getLocalBounds().reduced (space::s24, space::s8);

    for (auto* b : { &restart, &prev, &next })
    {
        b->setBounds (area.removeFromLeft (38).withSizeKeepingCentre (38, 38));
        area.removeFromLeft (space::s8);
    }
    const int listenW = juce::jmax (listen.getIdealWidth(), 104);
    listen.setBounds (area.removeFromLeft (listenW).withSizeKeepingCentre (listenW, 38));
    area.removeFromLeft (space::s8);
    const int wHear = PianoLookAndFeel::idealComboWidth (hear);
    hear.setBounds (area.removeFromLeft (wHear).withSizeKeepingCentre (wHear, 38));
    area.removeFromLeft (space::s16);
    const int wSpeed = PianoLookAndFeel::idealComboWidth (speed);
    speed.setBounds (area.removeFromLeft (wSpeed).withSizeKeepingCentre (wSpeed, 38));
    detectionArea = area.removeFromRight (190);

    const int wChords = PianoLookAndFeel::idealComboWidth (chords);
    chords.setBounds (area.withSizeKeepingCentre (juce::jmin (wChords, area.getWidth()), 38));
}

}
