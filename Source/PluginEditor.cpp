#include "PluginEditor.h"
#include "ParamIDs.h"
#include "TempoDivisions.h"

namespace
{
    juce::RangedAudioParameter& getParam (juce::AudioProcessorValueTreeState& apvts, const juce::String& id)
    {
        auto* param = apvts.getParameter (id);
        jassert (param != nullptr);
        return *param;
    }
}

//==============================================================================
ThrowButton::ThrowButton (juce::RangedAudioParameter& p) : param (p)
{
    startTimerHz (30);
}

ThrowButton::~ThrowButton()
{
    if (held)
        setHeld (false);
}

void ThrowButton::flash()
{
    flashLevel = 1.0f;
}

void ThrowButton::mouseDown (const juce::MouseEvent&)
{
    setHeld (true);
}

void ThrowButton::mouseUp (const juce::MouseEvent&)
{
    setHeld (false);
}

void ThrowButton::setHeld (bool shouldHold)
{
    if (shouldHold == held)
        return;

    held = shouldHold;

    if (held)
        param.beginChangeGesture();

    param.setValueNotifyingHost (held ? 1.0f : 0.0f);

    if (! held)
        param.endChangeGesture();

    repaint();
}

void ThrowButton::timerCallback()
{
    const bool on = param.getValue() > 0.5f;   // also lights up from host automation

    if (on != lit || flashLevel > 0.0f)
    {
        lit = on;
        flashLevel = juce::jmax (0.0f, flashLevel - 0.08f);
        repaint();
    }
}

void ThrowButton::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (2.0f);
    const float glow = lit ? 1.0f : flashLevel * 0.8f;

    if (glow > 0.0f)
    {
        g.setColour (Palette::throwHot.withAlpha (0.25f * glow));
        g.fillRoundedRectangle (bounds.expanded (2.0f), 10.0f);
    }

    g.setColour (Palette::throwHot.withAlpha (0.12f + 0.75f * glow));
    g.fillRoundedRectangle (bounds, 8.0f);
    g.setColour (Palette::throwHot.withAlpha (0.9f));
    g.drawRoundedRectangle (bounds, 8.0f, 1.5f);

    g.setColour (glow > 0.5f ? Palette::background : Palette::text);
    g.setFont (ThrowLookAndFeel::font (17.0f, true));
    g.drawText ("THROW", bounds, juce::Justification::centred, false);
}

//==============================================================================
AutoThrowAudioProcessorEditor::AutoThrowAudioProcessorEditor (AutoThrowAudioProcessor& p)
    : AudioProcessorEditor (&p),
      processorRef (p),
      display (p.getDisplayFeed(), getParam (p.apvts, ParamIDs::threshold)),
      throwButton (getParam (p.apvts, ParamIDs::manualThrow))
{
    setLookAndFeel (&lookAndFeel);

    addAndMakeVisible (display);
    display.onThrowFired = [this] { throwButton.flash(); };

    addAndMakeVisible (throwButton);
    throwButton.setTooltip ("Hold to send the vocal into the echo right now, like riding an aux send. "
                            "Automate this to place throws by hand. Flashes when Auto fires a throw.");

    addToggle (autoToggle, ParamIDs::autoDetect, "AUTO", Palette::throwHot,
               "Throw the last word of each phrase automatically.");
    addToggle (snapToggle, ParamIDs::snap, "SNAP", Palette::throwHot,
               "Snap the capture to the last word or syllable. Length becomes the longest it may grab.");
    addToggle (pingPongToggle, ParamIDs::pingPong, "PING-PONG", Palette::echo,
               "Bounce repeats left and right. Each throw starts on the opposite side to the last.");
    addToggle (dryToggle, ParamIDs::dry, "DRY", Palette::text,
               "Pass the dry vocal through. Turn off when using Auto Throw on an aux/send track.");

    addKnob (threshold, ParamIDs::threshold, "Threshold", Palette::detect,
             "Level the vocal must reach to count as singing/rapping. Set it just above the noise and breaths "
             "- or drag the line in the display.");
    addKnob (gap, ParamIDs::gap, "Gap", Palette::detect,
             "How long the vocal must stay quiet before the phrase counts as finished. "
             "Raise it if throws fire between words; lower it for faster lines.");
    addKnob (every, ParamIDs::every, "Every", Palette::detect,
             "Throw every phrase, or only 1 in N (e.g. the end of every 2nd or 4th line). Counts from the top of playback.");
    addKnob (length, ParamIDs::length, "Length", Palette::throwHot,
             "How much of the end of the phrase to throw. With Snap on, this is the longest grab.");
    addKnob (duck, ParamIDs::duck, "Amount", Palette::detect,
             "How far the echoes dip while the vocal is active, so tails stay out of the way of the next line.");
    addKnob (duckRelease, ParamIDs::duckRelease, "Release", Palette::detect,
             "How fast the echoes swell back once the vocal stops.");
    addKnob (time, ParamIDs::time, "Time", Palette::echo,
             "Echo spacing, synced to the host tempo.");
    addKnob (feedback, ParamIDs::feedback, "Feedback", Palette::echo,
             "How many repeats. Each repeat is filtered again, so it gets darker and thinner.");
    addKnob (lowCut, ParamIDs::lowCut, "Low Cut", Palette::echo,
             "High-pass on the echoes. Raise it to keep throws thin and out of the low end.");
    addKnob (highCut, ParamIDs::highCut, "High Cut", Palette::echo,
             "Low-pass on the echoes. Lower it for darker, more distant repeats.");
    addKnob (reverb, ParamIDs::reverb, "Reverb", Palette::echo,
             "Reverb on the echoes.");
    addKnob (size, ParamIDs::size, "Size", Palette::echo,
             "Reverb room size.");
    addKnob (wet, ParamIDs::wet, "Wet", Palette::text,
             "Level of the echoes and reverb.");

    tempoLabel.setJustificationType (juce::Justification::centredRight);
    tempoLabel.setFont (ThrowLookAndFeel::font (12.0f));
    tempoLabel.setColour (juce::Label::textColourId, Palette::textDim);
    addAndMakeVisible (tempoLabel);

    detectSection = { "DETECT", Palette::detect, {} };
    captureSection = { "CAPTURE", Palette::throwHot, {} };
    duckSection = { "DUCK", Palette::detect, {} };
    echoSection = { "ECHO", Palette::echo, {} };
    spaceSection = { "SPACE", Palette::echo, {} };
    outputSection = { "OUTPUT", Palette::text, {} };

    setSize (940, 600);
    startTimerHz (10);
    timerCallback();
}

AutoThrowAudioProcessorEditor::~AutoThrowAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void AutoThrowAudioProcessorEditor::addKnob (Knob& knob, const juce::String& paramID, const juce::String& name,
                                             juce::Colour tint, const juce::String& tooltip)
{
    auto& s = knob.slider;
    addAndMakeVisible (s);   // first, so the text box is built by our LookAndFeel
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 84, 18);
    s.setColour (juce::Slider::rotarySliderFillColourId, tint);
    s.setTooltip (tooltip);

    knob.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processorRef.apvts, paramID, s);

    auto& param = getParam (processorRef.apvts, paramID);
    s.setDoubleClickReturnValue (true, param.convertFrom0to1 (param.getDefaultValue()));

    knob.label.setText (name, juce::dontSendNotification);
    knob.label.setJustificationType (juce::Justification::centred);
    knob.label.setTooltip (tooltip);
    addAndMakeVisible (knob.label);
}

void AutoThrowAudioProcessorEditor::addToggle (Toggle& toggle, const juce::String& paramID, const juce::String& text,
                                               juce::Colour tint, const juce::String& tooltip)
{
    toggle.button.setButtonText (text);
    toggle.button.setColour (juce::ToggleButton::tickColourId, tint);
    toggle.button.setTooltip (tooltip);
    addAndMakeVisible (toggle.button);
    toggle.attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processorRef.apvts, paramID, toggle.button);
}

void AutoThrowAudioProcessorEditor::timerCallback()
{
    const int timeIndex = juce::roundToInt (processorRef.apvts.getRawParameterValue (ParamIDs::time)->load());
    auto text = TempoDivisions::names()[timeIndex] + " = " + juce::String (juce::roundToInt (processorRef.getDelayMs())) + " ms"
              + "   @ " + juce::String (processorRef.getBpm(), 1) + " BPM";

    if (! processorRef.hasHostTempo())
        text += " (no host tempo)";

    tempoLabel.setText (text, juce::dontSendNotification);
}

void AutoThrowAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (Palette::background);

    // title
    g.setColour (Palette::text);
    g.setFont (ThrowLookAndFeel::font (26.0f, true));
    g.drawText ("AUTO", 18, 14, 70, 32, juce::Justification::centredLeft, false);
    g.setColour (Palette::throwHot);
    g.drawText ("THROW", 82, 14, 110, 32, juce::Justification::centredLeft, false);
    g.setColour (Palette::textDim);
    g.setFont (ThrowLookAndFeel::font (11.5f));
    g.drawText ("phrase-end delay & reverb throws", 18, 44, 300, 16, juce::Justification::centredLeft, false);

    for (auto* section : { &detectSection, &captureSection, &duckSection, &echoSection, &spaceSection, &outputSection })
    {
        const auto b = section->bounds.toFloat();
        g.setColour (Palette::panel);
        g.fillRoundedRectangle (b, 8.0f);
        g.setColour (Palette::panelEdge);
        g.drawRoundedRectangle (b.reduced (0.5f), 8.0f, 1.0f);

        g.setColour (section->tint);
        g.fillRoundedRectangle (b.getX() + 12.0f, b.getY() + 13.0f, 3.0f, 12.0f, 1.5f);
        g.setColour (Palette::text);
        g.setFont (ThrowLookAndFeel::font (12.0f, true));
        g.drawText (section->title, juce::Rectangle<float> (b.getX() + 21.0f, b.getY() + 10.0f, 120.0f, 18.0f),
                    juce::Justification::centredLeft, false);
    }
}

void AutoThrowAudioProcessorEditor::layoutSection (Section& section, juce::Rectangle<int> area,
                                                   std::initializer_list<Knob*> knobs, Toggle* headerToggle)
{
    section.bounds = area;

    auto content = area.reduced (8, 6);
    auto header = content.removeFromTop (26);

    if (headerToggle != nullptr)
        headerToggle->button.setBounds (header.removeFromRight (juce::jmin (96, header.getWidth() / 2)).reduced (0, 2));

    const int cellWidth = content.getWidth() / (int) knobs.size();

    for (auto* knob : knobs)
    {
        auto cell = content.removeFromLeft (cellWidth);
        knob->label.setBounds (cell.removeFromTop (16));
        knob->slider.setBounds (cell.withSizeKeepingCentre (juce::jmin (cell.getWidth(), 96), cell.getHeight()));
    }
}

void AutoThrowAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (16);

    auto header = area.removeFromTop (52);
    throwButton.setBounds (header.removeFromRight (140).withSizeKeepingCentre (140, 46));
    header.removeFromRight (12);
    autoToggle.button.setBounds (header.removeFromRight (78).withSizeKeepingCentre (78, 26));
    header.removeFromRight (12);
    tempoLabel.setBounds (header.removeFromRight (300).withSizeKeepingCentre (300, 20));

    area.removeFromTop (8);
    display.setBounds (area.removeFromTop (200));
    area.removeFromTop (10);

    const int gapX = 10;
    const auto splitRow = [gapX] (juce::Rectangle<int> row, std::initializer_list<float> weights)
    {
        float total = 0.0f;

        for (auto w : weights)
            total += w;

        const float usable = (float) (row.getWidth() - gapX * ((int) weights.size() - 1));
        std::vector<juce::Rectangle<int>> cells;

        for (auto w : weights)
        {
            cells.push_back (row.removeFromLeft (juce::roundToInt (usable * w / total)));
            row.removeFromLeft (gapX);
        }

        return cells;
    };

    const int rowHeight = (area.getHeight() - 10) / 2;
    auto top = splitRow (area.removeFromTop (rowHeight), { 3.0f, 1.6f, 2.0f });
    area.removeFromTop (10);
    auto bottom = splitRow (area, { 4.0f, 2.0f, 1.6f });

    layoutSection (detectSection, top[0], { &threshold, &gap, &every }, nullptr);
    layoutSection (captureSection, top[1], { &length }, &snapToggle);
    layoutSection (duckSection, top[2], { &duck, &duckRelease }, nullptr);
    layoutSection (echoSection, bottom[0], { &time, &feedback, &lowCut, &highCut }, &pingPongToggle);
    layoutSection (spaceSection, bottom[1], { &reverb, &size }, nullptr);
    layoutSection (outputSection, bottom[2], { &wet }, &dryToggle);
}
