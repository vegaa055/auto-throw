#include <JuceHeader.h>
#include "AutoThrowCore.h"

#include <iostream>

/*
    Offline checks for the Auto Throw core, run against a synthetic vocal:
    harmonic "syllables" with short dips between them, grouped into phrases
    separated by longer gaps.

    Usage: AutoThrowTests [output-folder]
    Exits non-zero if any check fails, and renders demo WAVs to the folder
    (default: ./test-output).
*/

namespace
{
    int failures = 0;

    void check (bool ok, const juce::String& what)
    {
        std::cout << (ok ? "  PASS  " : "  FAIL  ") << what << std::endl;

        if (! ok)
            ++failures;
    }

    struct Phrase
    {
        double start = 0.0;
        std::vector<double> syllables;  // lengths in seconds

        // filled in by makeVocal()
        double end = 0.0;
        double lastSyllableStart = 0.0;
    };

    constexpr double syllableGap = 0.045;

    std::vector<Phrase> standardPhrases()
    {
        return { { 0.5, { 0.18, 0.15, 0.22, 0.16, 0.30 } },
                 { 3.0, { 0.20, 0.20, 0.25, 0.35 } },
                 { 5.5, { 0.12, 0.14, 0.12, 0.18, 0.15, 0.28 } },
                 { 8.0, { 0.22, 0.30, 0.40 } } };
    }

    juce::AudioBuffer<float> makeVocal (double sampleRate, double seconds, std::vector<Phrase>& phrases, int numChannels = 2)
    {
        juce::AudioBuffer<float> buffer (numChannels, (int) (seconds * sampleRate));
        auto* out = buffer.getWritePointer (0);
        juce::Random rng (1234);

        for (int i = 0; i < buffer.getNumSamples(); ++i)
            out[i] = (rng.nextFloat() * 2.0f - 1.0f) * 0.0005f;

        for (auto& phrase : phrases)
        {
            double t = phrase.start;

            for (size_t s = 0; s < phrase.syllables.size(); ++s)
            {
                const double len = phrase.syllables[s];

                if (s + 1 == phrase.syllables.size())
                    phrase.lastSyllableStart = t;

                const double f0 = 150.0 + 80.0 * rng.nextDouble();
                const int first = (int) (t * sampleRate);
                const int count = (int) (len * sampleRate);
                double phase = 0.0;

                for (int i = 0; i < count && first + i < buffer.getNumSamples(); ++i)
                {
                    const double tt = i / sampleRate;
                    const double env = juce::jmin (1.0, tt / 0.012, (len - tt) / 0.035);
                    const double f = f0 * (1.0 + 0.01 * std::sin (juce::MathConstants<double>::twoPi * 5.5 * tt));
                    phase += juce::MathConstants<double>::twoPi * f / sampleRate;

                    double v = 0.0;

                    for (int h = 1; h <= 12; ++h)
                        v += std::sin (phase * h) / h;

                    out[first + i] += (float) (0.18 * env * v);
                }

                t += len + syllableGap;
            }

            phrase.end = t - syllableGap;
        }

        for (int ch = 1; ch < numChannels; ++ch)
            buffer.copyFrom (ch, 0, buffer, 0, 0, buffer.getNumSamples());

        return buffer;
    }

    /** Settings that leave only the throws audible: no dry, no reverb, one echo. */
    AutoThrowCore::Params wetOnly()
    {
        AutoThrowCore::Params p;
        p.dryOn = false;
        p.reverbAmount = 0.0f;
        p.feedback = 0.0f;
        p.duckAmount = 0.0f;
        p.pingPong = false;
        p.lowCutHz = 20.0f;
        p.highCutHz = 20000.0f;
        return p;
    }

    struct RenderResult
    {
        juce::AudioBuffer<float> audio;
        std::vector<DisplayEvent> events;
    };

    using ParamsAt = std::function<void (AutoThrowCore::Params&, double seconds)>;

    RenderResult render (const juce::AudioBuffer<float>& input, double sampleRate, AutoThrowCore::Params params,
                         double bpm = 90.0, int blockSize = 256, int numInputs = 2, int numOutputs = 2,
                         ParamsAt paramsAt = {})
    {
        AutoThrowCore core;
        core.prepare (sampleRate, blockSize);

        RenderResult result;
        result.audio.makeCopyOf (input);

        juce::Random rng (99);
        int pos = 0;

        while (pos < result.audio.getNumSamples())
        {
            // blockSize 0 means "random block sizes", like some hosts
            const int size = blockSize > 0 ? blockSize : 1 + rng.nextInt (1500);
            const int n = juce::jmin (size, result.audio.getNumSamples() - pos);

            if (paramsAt)
                paramsAt (params, pos / sampleRate);

            core.setParams (params, bpm, true);

            juce::AudioBuffer<float> block (result.audio.getArrayOfWritePointers(), result.audio.getNumChannels(), pos, n);
            core.process (block, numInputs, numOutputs);

            DisplayEvent e;
            while (core.getDisplayFeed().events.pop (e))
                result.events.push_back (e);

            DisplayPoint dp;
            while (core.getDisplayFeed().points.pop (dp)) {}

            pos += n;
        }

        return result;
    }

    double pointsToSeconds (juce::int64 points)
    {
        return (double) points / DisplayFeed::pointsPerSecond;
    }

    float rmsDb (const juce::AudioBuffer<float>& b, int channel, double from, double to, double sampleRate)
    {
        const int s = juce::jlimit (0, b.getNumSamples(), (int) (from * sampleRate));
        const int e = juce::jlimit (0, b.getNumSamples(), (int) (to * sampleRate));
        return e > s ? juce::Decibels::gainToDecibels (b.getRMSLevel (channel, s, e - s), -150.0f) : -150.0f;
    }

    float peakDb (const juce::AudioBuffer<float>& b, double from, double to, double sampleRate)
    {
        const int s = juce::jlimit (0, b.getNumSamples(), (int) (from * sampleRate));
        const int e = juce::jlimit (0, b.getNumSamples(), (int) (to * sampleRate));
        return e > s ? juce::Decibels::gainToDecibels (b.getMagnitude (s, e - s), -150.0f) : -150.0f;
    }

    bool allFinite (const juce::AudioBuffer<float>& b)
    {
        for (int ch = 0; ch < b.getNumChannels(); ++ch)
            for (int i = 0; i < b.getNumSamples(); ++i)
                if (! std::isfinite (b.getSample (ch, i)))
                    return false;

        return true;
    }

    void writeWav (const juce::File& file, const juce::AudioBuffer<float>& audio, double sampleRate)
    {
        file.deleteFile();
        std::unique_ptr<juce::OutputStream> stream = file.createOutputStream();

        if (stream == nullptr)
            return;

        juce::WavAudioFormat wav;
        auto writer = wav.createWriterFor (stream, juce::AudioFormatWriterOptions().withSampleRate (sampleRate)
                                                                                    .withNumChannels (audio.getNumChannels())
                                                                                    .withBitsPerSample (24));
        if (writer != nullptr)
            writer->writeFromAudioSampleBuffer (audio, 0, audio.getNumSamples());
    }

    //==============================================================================
    constexpr double sr = 48000.0;
    constexpr double testBpm = 90.0;
    const double quarterSeconds = 60.0 / testBpm;

    void testDetection()
    {
        std::cout << "Phrase detection" << std::endl;

        auto phrases = standardPhrases();
        const auto input = makeVocal (sr, 11.0, phrases);
        const auto r = render (input, sr, wetOnly());

        check (r.events.size() == phrases.size(),
               "found " + juce::String ((int) r.events.size()) + " phrase ends for " + juce::String ((int) phrases.size()) + " phrases");

        for (size_t i = 0; i < juce::jmin (r.events.size(), phrases.size()); ++i)
        {
            const auto& e = r.events[i];
            const auto& ph = phrases[i];
            const double endErr = pointsToSeconds (e.captureEnd) - ph.end;
            const double startErr = pointsToSeconds (e.captureStart) - ph.lastSyllableStart;

            check (e.fired, "phrase " + juce::String ((int) i + 1) + " was thrown");
            check (endErr > -0.01 && endErr < 0.15,
                   "phrase " + juce::String ((int) i + 1) + " capture ends " + juce::String (endErr * 1000.0, 0) + " ms after the vocal");
            check (startErr > -0.07 && startErr < 0.015,
                   "phrase " + juce::String ((int) i + 1) + " snaps to the last syllable (" + juce::String (startErr * 1000.0, 0) + " ms)");
        }

        auto params = wetOnly();
        params.lengthMs = 200.0f;
        const auto capped = render (input, sr, params);
        bool allCapped = ! capped.events.empty();

        for (const auto& e : capped.events)
            allCapped = allCapped && pointsToSeconds (e.captureEnd - e.captureStart) <= 0.2 + 0.006;

        check (allCapped, "Length caps the capture at 200 ms, even when snapping");
    }

    void testEchoTiming()
    {
        std::cout << "Echo timing" << std::endl;

        auto phrases = standardPhrases();
        const auto input = makeVocal (sr, 11.0, phrases);
        const auto r = render (input, sr, wetOnly());

        if (r.events.empty())
        {
            check (false, "no throws to time");
            return;
        }

        const double firstLanding = pointsToSeconds (r.events.front().landing);
        check (peakDb (r.audio, 0.0, firstLanding - 0.01, sr) < -90.0f, "silent before the first echo");

        for (size_t i = 0; i < r.events.size(); ++i)
        {
            const auto& e = r.events[i];
            const double offset = pointsToSeconds (e.landing - e.captureStart);
            const double beats = offset / quarterSeconds;
            const double gridErr = std::abs (beats - std::round (beats)) * quarterSeconds;
            const double landing = pointsToSeconds (e.landing);

            // The phrase end is only known a gap (200 ms) after the capture ends.
            const double detectedAt = pointsToSeconds (e.captureEnd) - 0.015 + 0.2;

            check (gridErr < 0.011 && std::round (beats) >= 1.0,
                   "echo " + juce::String ((int) i + 1) + " lands " + juce::String (std::round (beats), 0) + " beat(s) after its word");
            check (landing >= detectedAt - 0.011, "echo " + juce::String ((int) i + 1) + " is never scheduled in the past");

            const float echoLevel = rmsDb (r.audio, 0, landing, landing + 0.15, sr);
            check (echoLevel > -30.0f, "echo " + juce::String ((int) i + 1) + " is audible (" + juce::String (echoLevel, 1) + " dB)");

            const double clipLength = pointsToSeconds (e.captureEnd - e.captureStart);
            const double quietFrom = landing + clipLength + 0.05;
            const double quietTo = i + 1 < r.events.size() ? pointsToSeconds (r.events[i + 1].landing) - 0.01 : 11.0;
            check (peakDb (r.audio, quietFrom, quietTo, sr) < -60.0f,
                   "echo " + juce::String ((int) i + 1) + " repeats once with feedback at 0");
        }
    }

    void testEvery()
    {
        std::cout << "Every N phrases" << std::endl;

        auto phrases = standardPhrases();
        const auto input = makeVocal (sr, 11.0, phrases);
        auto params = wetOnly();
        params.every = 2;
        const auto r = render (input, sr, params);

        int fired = 0;

        for (auto& e : r.events)
            fired += e.fired ? 1 : 0;

        check (r.events.size() == 4 && fired == 2, juce::String (fired) + " of " + juce::String ((int) r.events.size()) + " phrases thrown with Every = 1 in 2");
        check (r.events.size() == 4 && ! r.events[0].fired && r.events[1].fired && ! r.events[2].fired && r.events[3].fired,
               "throws land on phrases 2 and 4");
    }

    void testAutoOff()
    {
        std::cout << "Auto off" << std::endl;

        auto phrases = standardPhrases();
        const auto input = makeVocal (sr, 11.0, phrases);
        auto params = wetOnly();
        params.autoDetect = false;
        const auto r = render (input, sr, params);

        check (peakDb (r.audio, 0.0, 11.0, sr) < -120.0f, "no echoes when Auto is off and THROW isn't pressed");
    }

    void testManualThrow()
    {
        std::cout << "Manual THROW" << std::endl;

        auto phrases = standardPhrases();
        const auto input = makeVocal (sr, 4.0, phrases);
        auto params = wetOnly();
        params.autoDetect = false;

        const auto r = render (input, sr, params, testBpm, 256, 2, 2,
                               [] (AutoThrowCore::Params& p, double t) { p.manualThrow = t >= 0.6 && t < 0.9; });

        const double d = quarterSeconds;
        check (peakDb (r.audio, 0.0, 0.6 + d - 0.01, sr) < -120.0f, "nothing before the send's first echo");
        check (rmsDb (r.audio, 0, 0.6 + d + 0.02, 0.9 + d - 0.02, sr) > -30.0f, "held send echoes one delay later");
        check (peakDb (r.audio, 0.9 + d + 0.02, 4.0, sr) < -60.0f, "echo stops when the send closes");
    }

    void testPingPong()
    {
        std::cout << "Ping-pong" << std::endl;

        auto phrases = standardPhrases();
        const auto input = makeVocal (sr, 11.0, phrases);
        auto params = wetOnly();
        params.pingPong = true;
        const auto r = render (input, sr, params);

        if (r.events.size() < 2)
        {
            check (false, "need two throws");
            return;
        }

        for (int i = 0; i < 2; ++i)
        {
            const double landing = pointsToSeconds (r.events[(size_t) i].landing);
            const float l = rmsDb (r.audio, 0, landing, landing + 0.2, sr);
            const float rr = rmsDb (r.audio, 1, landing, landing + 0.2, sr);
            const bool leftFirst = (i % 2) == 0;
            check (leftFirst ? l > rr + 40.0f : rr > l + 40.0f,
                   "throw " + juce::String (i + 1) + " starts on the " + (leftFirst ? "left" : "right"));
        }
    }

    void testDucking()
    {
        std::cout << "Ducking" << std::endl;

        auto phrases = standardPhrases();
        const auto input = makeVocal (sr, 11.0, phrases);
        auto params = wetOnly();
        params.feedback = 0.85f;

        params.duckAmount = 0.0f;
        const auto open = render (input, sr, params);
        params.duckAmount = 1.0f;
        const auto ducked = render (input, sr, params);

        // phrase 2's body, while phrase 1's echoes are still ringing
        const double from = phrases[1].start + 0.1, to = phrases[1].end - 0.1;
        const float openDb = rmsDb (open.audio, 0, from, to, sr);
        const float duckedDb = rmsDb (ducked.audio, 0, from, to, sr);
        check (duckedDb < openDb - 12.0f,
               "echoes duck under the vocal (" + juce::String (openDb, 1) + " dB -> " + juce::String (duckedDb, 1) + " dB)");
    }

    void testBlockSizeIndependence()
    {
        std::cout << "Block sizes" << std::endl;

        auto phrases = standardPhrases();
        const auto input = makeVocal (sr, 11.0, phrases);
        AutoThrowCore::Params params;   // defaults: dry, reverb, feedback, ping-pong, duck all on

        const auto a = render (input, sr, params, testBpm, 64);
        const auto b = render (input, sr, params, testBpm, 2048);
        const auto c = render (input, sr, params, testBpm, 0);

        float diff = 0.0f;

        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < a.audio.getNumSamples(); ++i)
                diff = juce::jmax (diff, std::abs (a.audio.getSample (ch, i) - b.audio.getSample (ch, i)),
                                         std::abs (a.audio.getSample (ch, i) - c.audio.getSample (ch, i)));

        check (diff < 1.0e-5f, "64, 2048 and random block sizes render the same (max diff " + juce::String (diff, 8) + ")");
    }

    void testStability()
    {
        std::cout << "Stability" << std::endl;

        for (double rate : { 44100.0, 96000.0 })
        {
            std::vector<Phrase> phrases;

            for (double t = 0.2; t < 28.0; t += 1.4)
                phrases.push_back ({ t, { 0.2, 0.25, 0.3 } });

            const auto input = makeVocal (rate, 30.0, phrases);
            AutoThrowCore::Params params;
            params.feedback = 0.95f;
            params.reverbAmount = 1.0f;
            params.reverbSize = 1.0f;
            params.duckAmount = 0.0f;
            params.wetDb = 6.0f;
            params.gapMs = 60.0f;
            params.highCutHz = 20000.0f;
            params.lowCutHz = 20.0f;

            const auto r = render (input, rate, params, 300.0, 0);
            const float peak = peakDb (r.audio, 0.0, 30.0, rate);
            check (allFinite (r.audio) && peak < 24.0f,
                   "max feedback at " + juce::String (rate / 1000.0, 1) + " kHz stays bounded (peak " + juce::String (peak, 1) + " dB)");
        }
    }

    void testChannelLayouts()
    {
        std::cout << "Channel layouts" << std::endl;

        auto phrases = standardPhrases();
        const auto mono = makeVocal (sr, 5.0, phrases, 1);
        const auto r = render (mono, sr, wetOnly(), testBpm, 256, 1, 1);
        const double landing = r.events.empty() ? 0.0 : pointsToSeconds (r.events.front().landing);
        check (! r.events.empty() && allFinite (r.audio) && rmsDb (r.audio, 0, landing, landing + 0.15, sr) > -30.0f,
               "mono in, mono out throws");

        // Mono in, stereo out: the second channel arrives as junk and must be replaced by the input.
        auto stereo = makeVocal (sr, 5.0, phrases, 2);

        for (int i = 0; i < stereo.getNumSamples(); ++i)
            stereo.setSample (1, i, 0.25f);

        auto params = wetOnly();
        params.dryOn = true;
        const auto s = render (stereo, sr, params, testBpm, 256, 1, 2);
        const int before = (int) ((landing - 0.05) * sr);
        float maxDiff = 0.0f;

        for (int i = 0; i < before; ++i)
            maxDiff = juce::jmax (maxDiff, std::abs (s.audio.getSample (0, i) - s.audio.getSample (1, i)));

        check (maxDiff < 1.0e-6f, "mono in, stereo out puts the dry vocal on both sides");
    }

    void renderDemo (const juce::File& folder)
    {
        std::cout << "Demo render" << std::endl;

        folder.createDirectory();

        std::vector<Phrase> phrases { { 0.5, { 0.18, 0.15, 0.22, 0.16, 0.30 } },
                                      { 3.2, { 0.20, 0.20, 0.25, 0.35 } },
                                      { 6.0, { 0.12, 0.14, 0.12, 0.18, 0.15, 0.28 } },
                                      { 8.7, { 0.22, 0.30, 0.40 } },
                                      { 11.4, { 0.15, 0.15, 0.20, 0.15, 0.25, 0.45 } } };

        const auto input = makeVocal (sr, 16.0, phrases);
        const auto out = render (input, sr, AutoThrowCore::Params(), testBpm);

        writeWav (folder.getChildFile ("demo-dry.wav"), input, sr);
        writeWav (folder.getChildFile ("demo-auto-throw.wav"), out.audio, sr);

        std::cout << "  wrote " << folder.getFullPathName() << std::endl;
    }
}

int main (int argc, char* argv[])
{
    const auto outputFolder = argc > 1 ? juce::File::getCurrentWorkingDirectory().getChildFile (argv[1])
                                       : juce::File::getCurrentWorkingDirectory().getChildFile ("test-output");

    testDetection();
    testEchoTiming();
    testEvery();
    testAutoOff();
    testManualThrow();
    testPingPong();
    testDucking();
    testBlockSizeIndependence();
    testStability();
    testChannelLayouts();
    renderDemo (outputFolder);

    std::cout << std::endl << (failures == 0 ? "All checks passed" : juce::String (failures) + " check(s) failed") << std::endl;
    return failures == 0 ? 0 : 1;
}
