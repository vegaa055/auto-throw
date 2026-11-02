#pragma once

#include <JuceHeader.h>
#include "DisplayFeed.h"

/**
    Scrolling view of the last few seconds: the vocal level against the
    threshold, the words that were thrown (orange), where their echoes land,
    and the echo level (teal). Drag the threshold line to set it.
*/
class ThrowDisplay : public juce::Component,
                     private juce::Timer
{
public:
    ThrowDisplay (DisplayFeed&, juce::RangedAudioParameter& threshold);

    void paint (juce::Graphics&) override;
    void resized() override;

    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    /** Called on the message thread whenever an automatic throw fires. */
    std::function<void()> onThrowFired;

private:
    void timerCallback() override;

    float dbToY (float db) const noexcept;
    float yToDb (float y) const noexcept;
    float indexToX (juce::int64 index) const noexcept;
    float thresholdDb() const;
    bool nearThreshold (float y) const;
    void setThresholdFromY (float y);

    static constexpr int visiblePoints = DisplayFeed::pointsPerSecond * 5;
    static constexpr float minDb = -72.0f;
    static constexpr float maxDb = 0.0f;

    DisplayFeed& feed;
    juce::RangedAudioParameter& threshold;

    std::vector<DisplayPoint> points;   // ring indexed by point index
    std::vector<DisplayEvent> events;
    juce::int64 latest = -1;

    juce::Rectangle<float> plot;
    bool dragging = false;
};
