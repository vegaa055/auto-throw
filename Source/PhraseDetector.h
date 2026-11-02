#pragma once

#include <JuceHeader.h>

/**
    Finds the ends of vocal phrases in real time.

    A phrase starts when the vocal rises above the threshold and ends once it
    has stayed below the threshold (minus some hysteresis) for the gap time.
    Because the gap has to elapse before we know the phrase is over, the end
    is confirmed late: the capture window it reports always lies in the past,
    and the caller keeps an input history to pull that audio from.

    With snap-to-word on, the capture start is moved to the last dip in level
    before the phrase end, so the throw grabs the final word or syllable
    rather than a fixed-length slice.

    Positions are absolute sample counts supplied by the caller; they must
    increase by exactly one per process() call.
*/
class PhraseDetector
{
public:
    struct Params
    {
        float thresholdDb = -30.0f;
        float gapMs = 200.0f;
        float lengthMs = 500.0f;    // capture length (the longest allowed when snapping)
        bool snapToWord = true;
    };

    struct PhraseEnd
    {
        juce::int64 phraseStart = 0;
        juce::int64 captureStart = 0;
        juce::int64 captureEnd = 0;     // exclusive
    };

    void prepare (double sampleRate);
    void reset();
    void setParams (const Params&);

    /** Feeds one mono sample. Returns true when a phrase end is confirmed on
        this sample, in which case `out` holds the window to throw. */
    bool process (float x, juce::int64 pos, PhraseEnd& out);

    /** 0 when the vocal is well below the threshold, rising to 1 at it.
        Drives the ducker. */
    float getPresence() const noexcept;

    /** Smoothed mean-square level that the threshold is compared against. */
    float getPower() const noexcept { return power; }

    bool isInPhrase() const noexcept { return inPhrase; }

private:
    juce::int64 findSoundEnd (juce::int64 phraseEnd) const;
    juce::int64 findWordStart (juce::int64 phraseEnd, juce::int64 earliest) const;

    static constexpr int historyMs = 4096;
    static constexpr float hysteresisDb = 6.0f;
    static constexpr float wordDipDb = 9.0f;
    static constexpr int minWordMs = 60;
    static constexpr int minPhraseMs = 120;
    static constexpr int tailMs = 25;
    static constexpr int preRollMs = 20;

    Params params;
    double sampleRate = 44100.0;

    // detector input high-pass, so rumble and plosive thumps don't hold phrases open
    float hpCoeff = 0.0f, hpPrevIn = 0.0f, hpPrevOut = 0.0f;

    // slow envelope decides phrase state, fast envelope finds word boundaries
    float power = 0.0f, fastPower = 0.0f;
    float attackCoeff = 0.0f, releaseCoeff = 0.0f;
    float fastAttackCoeff = 0.0f, fastReleaseCoeff = 0.0f;

    float onPower = 0.0f, offPower = 0.0f;
    juce::int64 gapSamples = 0, lengthSamples = 0;
    int samplesPerMs = 44;

    bool inPhrase = false;
    juce::int64 phraseStart = 0, lastLoud = 0;

    // fast-envelope peak of each millisecond, for snapping to the last word
    std::array<float, historyMs> history {};
    juce::int64 currentMs = 0;
    float msPeak = 0.0f;
};
