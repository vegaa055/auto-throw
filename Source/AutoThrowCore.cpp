#include "AutoThrowCore.h"

namespace
{
    float onePoleCoeff (double timeMs, double sampleRate)
    {
        return (float) (1.0 - std::exp (-1.0 / (timeMs * 0.001 * sampleRate)));
    }

    float powerToDb (float p)
    {
        return 10.0f * std::log10 (p + 1.0e-12f);
    }
}

void AutoThrowCore::prepare (double newSampleRate, int maxBlockSize)
{
    sampleRate = newSampleRate;
    juce::ignoreUnused (maxBlockSize);   // processing is chunked, so buffers only need chunkSize

    detector.prepare (sampleRate);
    bus.prepare (sampleRate, chunkSize);

    wetBuffer.setSize (2, chunkSize);
    duckGains.assign ((size_t) chunkSize, 1.0f);
    inputPower.assign ((size_t) chunkSize, 0.0f);
    pointFlags.assign ((size_t) chunkSize, 0);

    sendGain.reset (sampleRate, 0.005);
    wetGain.reset (sampleRate, 0.03);
    dryGain.reset (sampleRate, 0.02);

    duckAttackCoeff = onePoleCoeff (5.0, sampleRate);
    echoCoeff = onePoleCoeff (25.0, sampleRate);
    pointSamples = juce::jmax (1, juce::roundToInt (sampleRate / DisplayFeed::pointsPerSecond));

    setParams (params, 120.0, false);
    reset();
}

void AutoThrowCore::reset()
{
    detector.reset();
    bus.reset();

    pos = 0;
    phraseCount = 0;
    wasSending = false;
    duckEnv = 0.0f;

    sendGain.setCurrentAndTargetValue (0.0f);
    wetGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (params.wetDb));
    dryGain.setCurrentAndTargetValue (params.dryOn ? 1.0f : 0.0f);

    pointInputPeak = pointEchoPeak = echoPower = 0.0f;
    pointFlagsAccum = 0;
    snapSmoothers = true;
}

void AutoThrowCore::setParams (const Params& newParams, double bpm, bool hostPlaying)
{
    params = newParams;
    params.every = juce::jmax (1, params.every);

    if (hostPlaying && ! wasPlaying)
        phraseCount = 0;   // so "1 in 4" counts from the top of playback

    wasPlaying = hostPlaying;

    bpm = juce::jlimit (TempoDivisions::minBpm, TempoDivisions::maxBpm, bpm);
    const double ms = TempoDivisions::toMs (params.timeIndex, bpm);
    delayMs.store (ms);

    detector.setParams ({ params.thresholdDb, params.gapMs, params.lengthMs, params.snapToWord });

    ThrowBus::Params busParams;
    busParams.delaySamples = ms * 0.001 * sampleRate;
    busParams.feedback = params.feedback;
    busParams.lowCutHz = params.lowCutHz;
    busParams.highCutHz = params.highCutHz;
    busParams.pingPong = params.pingPong;
    busParams.reverbAmount = params.reverbAmount;
    busParams.reverbSize = params.reverbSize;
    bus.setParams (busParams);

    const float send = params.manualThrow ? 1.0f : 0.0f;
    const float wetLevel = juce::Decibels::decibelsToGain (params.wetDb, -60.0f);
    const float dryLevel = params.dryOn ? 1.0f : 0.0f;

    // The first update after a reset jumps straight to the real settings
    // rather than gliding from the defaults.
    if (snapSmoothers)
    {
        sendGain.setCurrentAndTargetValue (send);
        wetGain.setCurrentAndTargetValue (wetLevel);
        dryGain.setCurrentAndTargetValue (dryLevel);
        snapSmoothers = false;
    }
    else
    {
        sendGain.setTargetValue (send);
        wetGain.setTargetValue (wetLevel);
        dryGain.setTargetValue (dryLevel);
    }

    duckReleaseCoeff = onePoleCoeff (juce::jmax (1.0f, params.duckReleaseMs), sampleRate);
}

void AutoThrowCore::process (juce::AudioBuffer<float>& buffer, int numInputChannels, int numOutputChannels)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const bool stereo = numOutputChannels > 1 && buffer.getNumChannels() > 1;

    if (stereo && numInputChannels == 1)
        buffer.copyFrom (1, 0, buffer, 0, 0, numSamples);

    for (int start = 0; start < numSamples; start += chunkSize)
        processChunk (buffer, start, juce::jmin (chunkSize, numSamples - start), stereo);
}

void AutoThrowCore::processChunk (juce::AudioBuffer<float>& buffer, int start, int numSamples, bool stereo)
{
    auto* left = buffer.getWritePointer (0, start);
    auto* right = stereo ? buffer.getWritePointer (1, start) : left;
    const auto blockStart = pos;

    if (params.manualThrow && ! wasSending)
        bus.beginSend();

    wasSending = params.manualThrow;

    for (int i = 0; i < numSamples; ++i, ++pos)
    {
        const float l = left[i];
        const float r = right[i];

        bus.writeInput (pos, l, r);

        PhraseDetector::PhraseEnd end;

        if (detector.process (0.5f * (l + r), pos, end))
            handlePhraseEnd (end, pos);

        const float send = sendGain.getNextValue();

        if (send > 0.0f)
            bus.sendFrame (pos, l * send, r * send);

        const float presence = detector.getPresence();
        duckEnv += (presence - duckEnv) * (presence > duckEnv ? duckAttackCoeff : duckReleaseCoeff);
        duckGains[(size_t) i] = 1.0f - params.duckAmount * duckEnv;

        inputPower[(size_t) i] = detector.getPower();
        pointFlags[(size_t) i] = (juce::uint8) ((detector.isInPhrase() ? DisplayPoint::inPhrase : 0)
                                              | (send > 0.0f ? DisplayPoint::sending : 0));
    }

    auto* wetL = wetBuffer.getWritePointer (0);
    auto* wetR = wetBuffer.getWritePointer (1);
    bus.render (blockStart, wetL, wetR, numSamples);

    for (int i = 0; i < numSamples; ++i)
    {
        const float wg = wetGain.getNextValue() * duckGains[(size_t) i];
        const float dg = dryGain.getNextValue();

        float echoL = wetL[i] * wg;
        float echoR = wetR[i] * wg;

        if (stereo)
        {
            left[i] = left[i] * dg + echoL;
            right[i] = right[i] * dg + echoR;
        }
        else
        {
            echoL = echoR = 0.5f * (echoL + echoR);
            left[i] = left[i] * dg + echoL;
        }

        const float e = 0.5f * (echoL * echoL + echoR * echoR);
        echoPower += (e - echoPower) * (e > echoPower ? 1.0f : echoCoeff);

        pointInputPeak = juce::jmax (pointInputPeak, inputPower[(size_t) i]);
        pointEchoPeak = juce::jmax (pointEchoPeak, echoPower);
        pointFlagsAccum |= pointFlags[(size_t) i];

        const auto p = blockStart + i;

        if ((p + 1) % pointSamples == 0)
            pushDisplayPoint (p);
    }
}

void AutoThrowCore::handlePhraseEnd (const PhraseDetector::PhraseEnd& end, juce::int64 now)
{
    DisplayEvent event;
    event.captureStart = end.captureStart / pointSamples;
    event.captureEnd = end.captureEnd / pointSamples;

    if (params.autoDetect)
    {
        ++phraseCount;

        if (phraseCount % params.every == 0)
        {
            const auto landing = bus.scheduleThrow (end.captureStart, end.captureEnd, now);
            event.fired = landing >= 0;
            event.landing = landing >= 0 ? landing / pointSamples : -1;
        }
    }

    feed.events.push (event);
}

void AutoThrowCore::pushDisplayPoint (juce::int64 p)
{
    DisplayPoint point;
    point.index = p / pointSamples;
    point.inputDb = powerToDb (pointInputPeak);
    point.echoDb = powerToDb (pointEchoPeak);
    point.flags = pointFlagsAccum;
    feed.points.push (point);

    pointInputPeak = pointEchoPeak = 0.0f;
    pointFlagsAccum = 0;
}
