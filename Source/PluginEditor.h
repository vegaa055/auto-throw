#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ThrowDisplay.h"
#include "ThrowLookAndFeel.h"

/** Momentary THROW pad: holding it opens the send, like riding an aux fader.
    The whole hold is one automation gesture, so it records cleanly. Also
    flashes when an automatic throw fires. */
class ThrowButton : public juce::Component,
                    public juce::SettableTooltipClient,
                    private juce::Timer
{
public:
    explicit ThrowButton (juce::RangedAudioParameter&);
    ~ThrowButton() override;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    void flash();

private:
    void timerCallback() override;
    void setHeld (bool);

    juce::RangedAudioParameter& param;
    bool held = false, lit = false;
    float flashLevel = 0.0f;
};

class AutoThrowAudioProcessorEditor : public juce::AudioProcessorEditor,
                                      private juce::Timer
{
public:
    explicit AutoThrowAudioProcessorEditor (AutoThrowAudioProcessor&);
    ~AutoThrowAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    struct Toggle
    {
        juce::ToggleButton button;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
    };

    struct Section
    {
        juce::String title;
        juce::Colour tint;
        juce::Rectangle<int> bounds;
    };

    void addKnob (Knob&, const juce::String& paramID, const juce::String& name,
                  juce::Colour tint, const juce::String& tooltip);
    void addToggle (Toggle&, const juce::String& paramID, const juce::String& text,
                    juce::Colour tint, const juce::String& tooltip);
    void layoutSection (Section&, juce::Rectangle<int> area, std::initializer_list<Knob*>, Toggle* headerToggle);
    void timerCallback() override;

    AutoThrowAudioProcessor& processorRef;
    ThrowLookAndFeel lookAndFeel;
    juce::TooltipWindow tooltips { this, 600 };

    ThrowDisplay display;
    ThrowButton throwButton;
    Toggle autoToggle, snapToggle, pingPongToggle, dryToggle;
    Knob threshold, gap, every, length, duck, duckRelease;
    Knob time, feedback, lowCut, highCut, reverb, size, wet;
    juce::Label tempoLabel;

    Section detectSection, captureSection, duckSection, echoSection, spaceSection, outputSection;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AutoThrowAudioProcessorEditor)
};
