#include "PhraseDetector.h"

namespace
{
    float onePoleCoeff (double timeMs, double sampleRate)
    {
        return (float) (1.0 - std::exp (-1.0 / (timeMs * 0.001 * sampleRate)));
    }

    float dbToPower (float db)
    {
        return std::pow (10.0f, db * 0.1f);
    }
}

void PhraseDetector::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    samplesPerMs = juce::jmax (1, juce::roundToInt (sampleRate * 0.001));

    hpCoeff = (float) (1.0 / (1.0 + juce::MathConstants<double>::twoPi * 100.0 / sampleRate));

    attackCoeff = onePoleCoeff (3.0, sampleRate);
    releaseCoeff = onePoleCoeff (15.0, sampleRate);
    fastAttackCoeff = onePoleCoeff (1.0, sampleRate);
    fastReleaseCoeff = onePoleCoeff (8.0, sampleRate);

    setParams (params);
    reset();
}

void PhraseDetector::reset()
{
    hpPrevIn = hpPrevOut = 0.0f;
    power = fastPower = 0.0f;
    inPhrase = false;
    phraseStart = lastLoud = 0;
    history.fill (0.0f);
    currentMs = 0;
    msPeak = 0.0f;
}

void PhraseDetector::setParams (const Params& newParams)
{
    params = newParams;
    onPower = dbToPower (params.thresholdDb);
    offPower = dbToPower (params.thresholdDb - hysteresisDb);
    gapSamples = (juce::int64) (params.gapMs * 0.001 * sampleRate);
    lengthSamples = (juce::int64) (params.lengthMs * 0.001 * sampleRate);
}

bool PhraseDetector::process (float x, juce::int64 pos, PhraseEnd& out)
{
    const float hp = hpCoeff * (hpPrevOut + x - hpPrevIn);
    hpPrevIn = x;
    hpPrevOut = hp;

    const float p = hp * hp;
    power += (p - power) * (p > power ? attackCoeff : releaseCoeff);
    fastPower += (p - fastPower) * (p > fastPower ? fastAttackCoeff : fastReleaseCoeff);

    const auto ms = pos / samplesPerMs;

    if (ms != currentMs)
    {
        history[(size_t) (currentMs % historyMs)] = msPeak;
        currentMs = ms;
        msPeak = 0.0f;
    }

    msPeak = juce::jmax (msPeak, fastPower);

    if (! inPhrase)
    {
        if (power >= onPower)
        {
            inPhrase = true;
            phraseStart = lastLoud = pos;
        }

        return false;
    }

    if (power >= offPower)
    {
        lastLoud = pos;
        return false;
    }

    if (pos - lastLoud < gapSamples)
        return false;

    inPhrase = false;

    const auto msToSamples = [this] (int msCount) { return (juce::int64) msCount * samplesPerMs; };

    if (lastLoud - phraseStart < msToSamples (minPhraseMs))
        return false;   // a click or a breath, not a phrase

    out.phraseStart = phraseStart;
    out.captureEnd = findSoundEnd (lastLoud) + msToSamples (tailMs);

    const auto earliest = juce::jmax (out.captureEnd - lengthSamples,
                                      phraseStart - msToSamples (preRollMs));

    out.captureStart = params.snapToWord ? findWordStart (lastLoud, earliest) : earliest;
    return true;
}

juce::int64 PhraseDetector::findSoundEnd (juce::int64 phraseEnd) const
{
    // The slow envelope drops below the threshold well after the sound
    // stops; the fast one shows where the vocal actually ended.
    const auto endMs = phraseEnd / samplesPerMs;
    const auto firstMs = juce::jmax (endMs - 250, currentMs - historyMs + 1);

    for (auto ms = endMs; ms >= firstMs; --ms)
        if (history[(size_t) (ms % historyMs)] >= offPower)
            return juce::jmin (phraseEnd, (ms + 1) * samplesPerMs);

    return phraseEnd;
}

juce::int64 PhraseDetector::findWordStart (juce::int64 phraseEnd, juce::int64 earliest) const
{
    // Walk back from the end through the decaying tail, up over the last
    // word, and stop at the first dip well below that word's peak.
    const float dipRatio = dbToPower (-wordDipDb);
    const auto endMs = phraseEnd / samplesPerMs;
    const auto firstMs = juce::jmax (earliest / samplesPerMs, currentMs - historyMs + 1);

    float peak = 0.0f;

    for (auto ms = endMs; ms >= firstMs; --ms)
    {
        const float v = history[(size_t) (ms % historyMs)];

        if (v > peak)
            peak = v;
        else if (peak >= onPower && v < peak * dipRatio && endMs - ms >= minWordMs)
            return juce::jmax (earliest, ms * samplesPerMs);
    }

    return earliest;
}

float PhraseDetector::getPresence() const noexcept
{
    constexpr float rangeDb = 12.0f;
    const float db = 10.0f * std::log10 (power + 1.0e-12f);
    return juce::jlimit (0.0f, 1.0f, (db - (params.thresholdDb - rangeDb)) / rangeDb);
}
