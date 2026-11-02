#include "ThrowLookAndFeel.h"

ThrowLookAndFeel::ThrowLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, Palette::background);
    setColour (juce::Label::textColourId, Palette::textDim);
    setColour (juce::Slider::textBoxTextColourId, Palette::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId, Palette::throwHot.withAlpha (0.4f));
    setColour (juce::Slider::rotarySliderFillColourId, Palette::throwHot);
    setColour (juce::ToggleButton::tickColourId, Palette::throwHot);
    setColour (juce::TextEditor::textColourId, Palette::text);
    setColour (juce::TextEditor::highlightColourId, Palette::throwHot.withAlpha (0.4f));
    setColour (juce::CaretComponent::caretColourId, Palette::text);
    setColour (juce::TooltipWindow::backgroundColourId, Palette::panelEdge);
    setColour (juce::TooltipWindow::textColourId, Palette::text);
    setColour (juce::TooltipWindow::outlineColourId, Palette::track);
}

juce::Font ThrowLookAndFeel::font (float height, bool bold)
{
    return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain));
}

void ThrowLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                         float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                                         juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (4.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    const auto tint = slider.findColour (juce::Slider::rotarySliderFillColourId);
    const float arcRadius = radius - 3.0f;
    const juce::PathStrokeType arcStroke (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (Palette::track);
    g.strokePath (track, arcStroke);

    if (slider.isEnabled() && sliderPos > 0.0f)
    {
        juce::Path value;
        value.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, rotaryStartAngle, angle, true);
        g.setColour (tint.withAlpha (0.25f));
        g.strokePath (value, juce::PathStrokeType (8.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (tint);
        g.strokePath (value, arcStroke);
    }

    const float bodyRadius = radius - 9.0f;
    const auto body = juce::Rectangle<float> (bodyRadius * 2.0f, bodyRadius * 2.0f).withCentre (centre);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2b2b37), centre.x, body.getY(),
                                             juce::Colour (0xff17171e), centre.x, body.getBottom(), false));
    g.fillEllipse (body);
    g.setColour (Palette::panelEdge.brighter (0.15f));
    g.drawEllipse (body, 1.0f);

    const auto tip = centre.getPointOnCircumference (bodyRadius * 0.82f, angle);
    const auto base = centre.getPointOnCircumference (bodyRadius * 0.35f, angle);
    g.setColour (slider.isMouseOverOrDragging() ? tint : Palette::text);
    g.drawLine ({ base, tip }, 2.5f);
}

void ThrowLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                                         bool shouldDrawButtonAsHighlighted, bool)
{
    const auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);
    const float corner = bounds.getHeight() * 0.5f;
    const bool on = button.getToggleState();
    const auto tint = button.findColour (juce::ToggleButton::tickColourId);

    g.setColour (on ? tint.withAlpha (0.16f) : Palette::panelEdge.withAlpha (0.5f));
    g.fillRoundedRectangle (bounds, corner);
    g.setColour (on ? tint.withAlpha (0.7f) : Palette::panelEdge.brighter (shouldDrawButtonAsHighlighted ? 0.5f : 0.2f));
    g.drawRoundedRectangle (bounds, corner, 1.0f);

    const auto led = juce::Point<float> (bounds.getX() + corner, bounds.getCentreY());

    if (on)
    {
        g.setColour (tint.withAlpha (0.3f));
        g.fillEllipse (juce::Rectangle<float> (12.0f, 12.0f).withCentre (led));
    }

    g.setColour (on ? tint : Palette::textDim.withAlpha (0.5f));
    g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre (led));

    g.setColour (on ? Palette::text : Palette::textDim);
    g.setFont (font (11.0f, true));
    g.drawText (button.getButtonText(), bounds.withTrimmedLeft (corner * 2.0f).withTrimmedRight (corner * 0.5f),
                juce::Justification::centred, false);
}

juce::Label* ThrowLookAndFeel::createSliderTextBox (juce::Slider& slider)
{
    auto* label = LookAndFeel_V4::createSliderTextBox (slider);
    label->setFont (font (12.0f));
    label->setJustificationType (juce::Justification::centred);
    return label;
}

juce::Font ThrowLookAndFeel::getLabelFont (juce::Label&)
{
    return font (12.0f);
}
