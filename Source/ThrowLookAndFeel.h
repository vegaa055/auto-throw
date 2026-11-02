#pragma once

#include <JuceHeader.h>

/** Shared colours. Amber = listening to the vocal, orange = the throw,
    teal = the echoes. */
namespace Palette
{
    inline const juce::Colour background { 0xff111116 };
    inline const juce::Colour panel      { 0xff1a1a22 };
    inline const juce::Colour panelEdge  { 0xff2a2a37 };
    inline const juce::Colour track      { 0xff2c2c3a };
    inline const juce::Colour text       { 0xffe9e6f2 };
    inline const juce::Colour textDim    { 0xff8b88a0 };
    inline const juce::Colour detect     { 0xffffc861 };
    inline const juce::Colour throwHot   { 0xffff6a3d };
    inline const juce::Colour echo       { 0xff3dd6c6 };
}

class ThrowLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ThrowLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider&) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&,
                           bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    juce::Label* createSliderTextBox (juce::Slider&) override;
    juce::Font getLabelFont (juce::Label&) override;

    static juce::Font font (float height, bool bold = false);
};
