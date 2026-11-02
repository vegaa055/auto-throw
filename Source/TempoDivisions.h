#pragma once

#include <JuceHeader.h>

/** Echo spacing choices for the throw delay, shortest to longest.
    Indices are stable — append only. */
namespace TempoDivisions
{
    inline const juce::StringArray& names()
    {
        static const juce::StringArray n { "1/16", "1/8T", "1/8", "1/4T", "1/8D",
                                           "1/4", "1/2T", "1/4D", "1/2", "1 bar" };
        return n;
    }

    inline double beats (int index)
    {
        static const double b[] = { 0.25, 1.0 / 3.0, 0.5, 2.0 / 3.0, 0.75,
                                    1.0, 4.0 / 3.0, 1.5, 2.0, 4.0 };
        return b[juce::jlimit (0, 9, index)];
    }

    inline double toMs (int index, double bpm)
    {
        return beats (index) * 60000.0 / bpm;
    }

    constexpr int quarterNote = 5;
    constexpr double longestBeats = 4.0;

    // Host tempos are clamped to this range, which bounds the delay buffers.
    constexpr double minBpm = 40.0;
    constexpr double maxBpm = 300.0;
}
