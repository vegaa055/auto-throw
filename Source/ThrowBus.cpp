#include "ThrowBus.h"
#include "TempoDivisions.h"

void ThrowBus::prepare (double newSampleRate, int maxBlockSize)
{
    sampleRate = newSampleRate;

    const double longestDelaySeconds = TempoDivisions::longestBeats * 60.0 / TempoDivisions::minBpm;
    maxDelaySamples = (int) std::ceil (longestDelaySeconds * sampleRate);
    maxCaptureSamples = (int) (maxCaptureSeconds * sampleRate);
    fadeInSamples = juce::jmax (1, (int) (0.005 * sampleRate));
    fadeOutSamples = juce::jmax (1, (int) (0.025 * sampleRate));

    // History must reach back past the longest gap plus the longest capture;
    // pending must reach forward one full delay plus a capture.
    historySize = (int) (4.0 * sampleRate) + maxBlockSize;
    pendingSize = maxDelaySamples + maxCaptureSamples + maxBlockSize + 16;
    history.setSize (2, historySize);
    pending.setSize (2, pendingSize);

    const juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) maxBlockSize, 2 };
    echoLine.prepare (spec);
    echoLine.setMaximumDelayInSamples (maxDelaySamples + 4);

    lowCut.prepare (spec);
    lowCut.setType (juce::dsp::StateVariableTPTFilterType::highpass);
    highCut.prepare (spec);
    highCut.setType (juce::dsp::StateVariableTPTFilterType::lowpass);

    delaySmoothed.reset (sampleRate, 0.08);
    feedbackSmoothed.reset (sampleRate, 0.03);
    reverbSmoothed.reset (sampleRate, 0.03);

    reverb.setSampleRate (sampleRate);
    reverbScratch.setSize (2, maxBlockSize);

    setParams (params);
    reset();
}

void ThrowBus::reset()
{
    history.clear();
    pending.clear();
    echoLine.reset();
    lowCut.reset();
    highCut.reset();
    reverb.reset();

    nextSide = sendSide = 0;
    snapSmoothers = true;
}

void ThrowBus::setParams (const Params& newParams)
{
    params = newParams;
    params.delaySamples = juce::jlimit (1.0, (double) maxDelaySamples, params.delaySamples);

    // The first update after a reset jumps straight to the real settings.
    if (snapSmoothers)
    {
        delaySmoothed.setCurrentAndTargetValue ((float) params.delaySamples);
        feedbackSmoothed.setCurrentAndTargetValue (params.feedback);
        reverbSmoothed.setCurrentAndTargetValue (params.reverbAmount);
        snapSmoothers = false;
    }
    else
    {
        delaySmoothed.setTargetValue ((float) params.delaySamples);
        feedbackSmoothed.setTargetValue (params.feedback);
        reverbSmoothed.setTargetValue (params.reverbAmount);
    }

    const float nyquistSafe = (float) sampleRate * 0.45f;
    lowCut.setCutoffFrequency (juce::jlimit (10.0f, nyquistSafe, params.lowCutHz));
    highCut.setCutoffFrequency (juce::jlimit (10.0f, nyquistSafe, params.highCutHz));

    juce::Reverb::Parameters rp;
    rp.roomSize = params.reverbSize;
    rp.damping = 0.45f;
    rp.wetLevel = 0.33f;
    rp.dryLevel = 0.0f;
    rp.width = 1.0f;
    reverb.setParameters (rp);
}

juce::int64 ThrowBus::spacing() const noexcept
{
    return juce::jmax ((juce::int64) 1, (juce::int64) std::llround (params.delaySamples));
}

void ThrowBus::writeInput (juce::int64 pos, float left, float right) noexcept
{
    const auto i = (int) (pos % historySize);
    history.setSample (0, i, left);
    history.setSample (1, i, right);
}

juce::int64 ThrowBus::scheduleThrow (juce::int64 start, juce::int64 end, juce::int64 now) noexcept
{
    start = juce::jmax (start, now - historySize + 1);
    end = juce::jmin (end, now + 1, start + maxCaptureSamples);

    const auto length = (int) (end - start);

    if (length <= 0)
        return -1;

    // First grid point (start + k * delay, k >= 1) that hasn't played yet.
    const auto step = spacing();
    const auto steps = juce::jmax ((juce::int64) 1, (now - start + step - 1) / step);
    const auto landing = start + steps * step;

    const int fadeIn = juce::jmax (1, juce::jmin (length / 4, fadeInSamples));
    const int fadeOut = juce::jmax (1, juce::jmin (length / 3, fadeOutSamples));

    const int side = nextSide;

    if (params.pingPong)
        nextSide ^= 1;

    const auto* histL = history.getReadPointer (0);
    const auto* histR = history.getReadPointer (1);
    auto* pendL = pending.getWritePointer (0);
    auto* pendR = pending.getWritePointer (1);
    auto* pendSide = pending.getWritePointer (side);

    for (int j = 0; j < length; ++j)
    {
        float w = 1.0f;

        if (j < fadeIn)
            w = (float) j / (float) fadeIn;
        else if (j >= length - fadeOut)
            w = (float) (length - j) / (float) fadeOut;

        const auto h = (int) ((start + j) % historySize);
        const auto p = (int) ((landing + j) % pendingSize);

        if (params.pingPong)
        {
            pendSide[p] += 0.5f * (histL[h] + histR[h]) * w;
        }
        else
        {
            pendL[p] += histL[h] * w;
            pendR[p] += histR[h] * w;
        }
    }

    return landing;
}

void ThrowBus::beginSend() noexcept
{
    sendSide = nextSide;

    if (params.pingPong)
        nextSide ^= 1;
}

void ThrowBus::sendFrame (juce::int64 now, float left, float right) noexcept
{
    const auto p = (int) ((now + spacing()) % pendingSize);

    if (params.pingPong)
    {
        pending.getWritePointer (sendSide)[p] += 0.5f * (left + right);
    }
    else
    {
        pending.getWritePointer (0)[p] += left;
        pending.getWritePointer (1)[p] += right;
    }
}

void ThrowBus::render (juce::int64 blockStart, float* left, float* right, int numSamples) noexcept
{
    auto* pendL = pending.getWritePointer (0);
    auto* pendR = pending.getWritePointer (1);
    const bool pingPong = params.pingPong;

    for (int i = 0; i < numSamples; ++i)
    {
        const auto p = (int) ((blockStart + i) % pendingSize);
        const float inL = pendL[p];
        const float inR = pendR[p];
        pendL[p] = pendR[p] = 0.0f;

        const float delay = delaySmoothed.getNextValue();
        const float fb = feedbackSmoothed.getNextValue();
        const float tapL = echoLine.popSample (0, delay);
        const float tapR = echoLine.popSample (1, delay);

        float l = inL + fb * (pingPong ? tapR : tapL);
        float r = inR + fb * (pingPong ? tapL : tapR);

        // Filtering inside the loop darkens and thins each repeat a little more.
        l = highCut.processSample (0, lowCut.processSample (0, l));
        r = highCut.processSample (1, lowCut.processSample (1, r));

        echoLine.pushSample (0, l);
        echoLine.pushSample (1, r);

        left[i] = l;
        right[i] = r;
    }

    auto* verbL = reverbScratch.getWritePointer (0);
    auto* verbR = reverbScratch.getWritePointer (1);
    juce::FloatVectorOperations::copy (verbL, left, numSamples);
    juce::FloatVectorOperations::copy (verbR, right, numSamples);
    reverb.processStereo (verbL, verbR, numSamples);

    for (int i = 0; i < numSamples; ++i)
    {
        const float amount = reverbSmoothed.getNextValue();
        left[i] += amount * verbL[i];
        right[i] += amount * verbR[i];
    }
}
