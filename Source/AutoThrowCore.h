#pragma once

#include <JuceHeader.h>
#include "PhraseDetector.h"
#include "ThrowBus.h"
#include "DisplayFeed.h"
#include "TempoDivisions.h"

/**
    Everything Auto Throw does to audio, independent of the plugin wrapper
    so it can be tested offline.

    Per sample: record the input, run the phrase detector, throw the last
    word when a phrase end is confirmed (every Nth phrase), feed the manual
    send while it's held, and track how much to duck the wet signal. Then
    the throw bus renders the echoes for the block and they're mixed back
    in under the dry vocal.
*/
class AutoThrowCore
{
public:
    struct Params
    {
        bool autoDetect = true;
        bool manualThrow = false;

        float thresholdDb = -30.0f;
        float gapMs = 200.0f;
        int every = 1;

        float lengthMs = 500.0f;
        bool snapToWord = true;

        int timeIndex = TempoDivisions::quarterNote;
        float feedback = 0.4f;
        float lowCutHz = 250.0f;
        float highCutHz = 7000.0f;
        bool pingPong = true;

        float reverbAmount = 0.25f;
        float reverbSize = 0.6f;

        float duckAmount = 0.6f;
        float duckReleaseMs = 200.0f;

        float wetDb = 0.0f;
        bool dryOn = true;
    };

    void prepare (double sampleRate, int maxBlockSize);
    void reset();

    /** Call once per block before process(). `bpm` is the host tempo (or a
        fallback); the "Every" phrase count restarts when playback starts. */
    void setParams (const Params&, double bpm, bool hostPlaying);

    /** Processes in place. With one input and two outputs the input is
        copied to both sides first. */
    void process (juce::AudioBuffer<float>&, int numInputChannels, int numOutputChannels);

    DisplayFeed& getDisplayFeed() noexcept { return feed; }

    /** Current echo spacing, for the editor's readout. */
    double getDelayMs() const noexcept { return delayMs.load(); }

private:
    void processChunk (juce::AudioBuffer<float>&, int start, int numSamples, bool stereo);
    void handlePhraseEnd (const PhraseDetector::PhraseEnd&, juce::int64 now);
    void pushDisplayPoint (juce::int64 pos);

    static constexpr int chunkSize = 512;

    Params params;
    double sampleRate = 44100.0;

    PhraseDetector detector;
    ThrowBus bus;
    DisplayFeed feed;

    juce::int64 pos = 0;    // absolute sample count since prepare/reset
    int phraseCount = 0;
    bool wasPlaying = false;
    bool wasSending = false;
    bool snapSmoothers = true;

    juce::SmoothedValue<float> sendGain, wetGain, dryGain;
    float duckEnv = 0.0f, duckAttackCoeff = 0.0f, duckReleaseCoeff = 0.0f;

    juce::AudioBuffer<float> wetBuffer;
    std::vector<float> duckGains, inputPower;
    std::vector<juce::uint8> pointFlags;

    // display accumulation
    int pointSamples = 240;
    float pointInputPeak = 0.0f, pointEchoPeak = 0.0f, echoPower = 0.0f, echoCoeff = 0.0f;
    juce::uint8 pointFlagsAccum = 0;

    std::atomic<double> delayMs { 500.0 };
};
