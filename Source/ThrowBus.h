#pragma once

#include <JuceHeader.h>

/**
    The wet side of Auto Throw: a feedback echo with filters in the loop,
    followed by a reverb.

    Nothing reaches the echo unless it is thrown. A throw copies a window of
    recorded input into the "pending" ring at the position where its first
    echo should play; the echo loop reads the pending ring, adds the
    feedback taps and writes the result back into the delay line, so every
    repeat lands one delay later and passes through the filters again.

    Because a phrase end is only confirmed after the gap, the window being
    thrown is already in the past. Scheduling it at start + k * delay (the
    first grid point that hasn't been played yet) keeps the echoes in time
    with the performance without adding any plugin latency.

    All positions are absolute sample counts shared with the caller.
*/
class ThrowBus
{
public:
    struct Params
    {
        double delaySamples = 22050.0;
        float feedback = 0.4f;
        float lowCutHz = 250.0f;
        float highCutHz = 7000.0f;
        bool pingPong = true;
        float reverbAmount = 0.25f;
        float reverbSize = 0.6f;
    };

    void prepare (double sampleRate, int maxBlockSize);
    void reset();
    void setParams (const Params&);

    /** Records one input frame. Call for every sample, in order, before any
        throw that could reach back to it. */
    void writeInput (juce::int64 pos, float left, float right) noexcept;

    /** Throws the recorded window [start, end). Returns where its first echo
        will start playing, or -1 if the window was empty. */
    juce::int64 scheduleThrow (juce::int64 start, juce::int64 end, juce::int64 now) noexcept;

    /** Starts a manual send. In ping-pong mode each send starts on the
        opposite side to the previous throw. */
    void beginSend() noexcept;

    /** Feeds one frame through the open manual send, so it echoes one delay
        from now — the same as riding an aux send in the mix. */
    void sendFrame (juce::int64 now, float left, float right) noexcept;

    /** Renders the wet signal for [blockStart, blockStart + numSamples). */
    void render (juce::int64 blockStart, float* left, float* right, int numSamples) noexcept;

    /** Longest capture a throw can hold. */
    static constexpr double maxCaptureSeconds = 1.2;

private:
    juce::int64 spacing() const noexcept;

    Params params;
    double sampleRate = 44100.0;
    int maxDelaySamples = 0;
    int maxCaptureSamples = 0;
    int fadeInSamples = 0, fadeOutSamples = 0;

    juce::AudioBuffer<float> history;   // input ring, indexed by absolute position
    juce::AudioBuffer<float> pending;   // first echoes waiting to play, same indexing
    int historySize = 0, pendingSize = 0;

    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> echoLine;
    juce::dsp::StateVariableTPTFilter<float> lowCut, highCut;
    juce::SmoothedValue<float> delaySmoothed, feedbackSmoothed, reverbSmoothed;

    juce::Reverb reverb;
    juce::AudioBuffer<float> reverbScratch;

    int nextSide = 0;   // ping-pong: the channel the next throw starts on
    int sendSide = 0;
    bool snapSmoothers = true;
};
