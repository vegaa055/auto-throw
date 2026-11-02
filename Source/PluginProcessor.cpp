#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ParamIDs.h"

AutoThrowAudioProcessor::AutoThrowAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "AutoThrow", createParameterLayout())
{
    p.autoDetect = apvts.getRawParameterValue (ParamIDs::autoDetect);
    p.manualThrow = apvts.getRawParameterValue (ParamIDs::manualThrow);
    p.threshold = apvts.getRawParameterValue (ParamIDs::threshold);
    p.gap = apvts.getRawParameterValue (ParamIDs::gap);
    p.every = apvts.getRawParameterValue (ParamIDs::every);
    p.length = apvts.getRawParameterValue (ParamIDs::length);
    p.snap = apvts.getRawParameterValue (ParamIDs::snap);
    p.time = apvts.getRawParameterValue (ParamIDs::time);
    p.feedback = apvts.getRawParameterValue (ParamIDs::feedback);
    p.lowCut = apvts.getRawParameterValue (ParamIDs::lowCut);
    p.highCut = apvts.getRawParameterValue (ParamIDs::highCut);
    p.pingPong = apvts.getRawParameterValue (ParamIDs::pingPong);
    p.reverb = apvts.getRawParameterValue (ParamIDs::reverb);
    p.size = apvts.getRawParameterValue (ParamIDs::size);
    p.duck = apvts.getRawParameterValue (ParamIDs::duck);
    p.duckRelease = apvts.getRawParameterValue (ParamIDs::duckRelease);
    p.wet = apvts.getRawParameterValue (ParamIDs::wet);
    p.dry = apvts.getRawParameterValue (ParamIDs::dry);
}

juce::AudioProcessorValueTreeState::ParameterLayout AutoThrowAudioProcessor::createParameterLayout()
{
    using namespace juce;

    const auto skewed = [] (float lo, float hi, float centre)
    {
        NormalisableRange<float> r (lo, hi, 1.0f);
        r.setSkewForCentre (centre);
        return r;
    };

    const auto dbText = AudioParameterFloatAttributes().withStringFromValueFunction (
        [] (float v, int) { return String (v, 1) + " dB"; });

    const auto msText = AudioParameterFloatAttributes().withStringFromValueFunction (
        [] (float v, int) { return String (roundToInt (v)) + " ms"; });

    const auto hzText = AudioParameterFloatAttributes().withStringFromValueFunction (
        [] (float v, int) { return v >= 1000.0f ? String (v / 1000.0f, 1) + " kHz"
                                                : String (roundToInt (v)) + " Hz"; });

    const auto percentText = AudioParameterFloatAttributes().withStringFromValueFunction (
        [] (float v, int) { return String (roundToInt (v * 100.0f)) + "%"; });

    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::autoDetect, 1 }, "Auto", true));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::manualThrow, 1 }, "Throw", false));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::threshold, 1 }, "Threshold",
                                                       NormalisableRange<float> (-60.0f, 0.0f, 0.1f), -30.0f, dbText));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::gap, 1 }, "Gap",
                                                       skewed (60.0f, 800.0f, 220.0f), 200.0f, msText));
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { ParamIDs::every, 1 }, "Every", 1, 8, 1,
                                                     AudioParameterIntAttributes().withStringFromValueFunction (
                                                         [] (int v, int) { return v == 1 ? String ("All") : "1 in " + String (v); })));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::length, 1 }, "Length",
                                                       skewed (60.0f, 1000.0f, 400.0f), 500.0f, msText));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::snap, 1 }, "Snap", true));

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::time, 1 }, "Time",
                                                        TempoDivisions::names(), TempoDivisions::quarterNote));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::feedback, 1 }, "Feedback",
                                                       NormalisableRange<float> (0.0f, 0.95f, 0.01f), 0.4f, percentText));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::lowCut, 1 }, "Low Cut",
                                                       skewed (20.0f, 2000.0f, 250.0f), 250.0f, hzText));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::highCut, 1 }, "High Cut",
                                                       skewed (1000.0f, 20000.0f, 5000.0f), 7000.0f, hzText));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::pingPong, 1 }, "Ping-Pong", true));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::reverb, 1 }, "Reverb",
                                                       NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.25f, percentText));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::size, 1 }, "Size",
                                                       NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.6f, percentText));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::duck, 1 }, "Duck",
                                                       NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.6f, percentText));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::duckRelease, 1 }, "Duck Release",
                                                       skewed (20.0f, 1000.0f, 200.0f), 200.0f, msText));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::wet, 1 }, "Wet",
                                                       NormalisableRange<float> (-24.0f, 6.0f, 0.1f), 0.0f, dbText));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::dry, 1 }, "Dry", true));

    return layout;
}

void AutoThrowAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    core.prepare (sampleRate, samplesPerBlock);
}

bool AutoThrowAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    const auto monoOrStereo = [] (const juce::AudioChannelSet& set)
    {
        return set == juce::AudioChannelSet::mono() || set == juce::AudioChannelSet::stereo();
    };

    if (! monoOrStereo (in) || ! monoOrStereo (out))
        return false;

    return in.size() <= out.size();
}

AutoThrowCore::Params AutoThrowAudioProcessor::makeCoreParams() const
{
    AutoThrowCore::Params cp;
    cp.autoDetect = p.autoDetect->load() > 0.5f;
    cp.manualThrow = p.manualThrow->load() > 0.5f;
    cp.thresholdDb = p.threshold->load();
    cp.gapMs = p.gap->load();
    cp.every = juce::roundToInt (p.every->load());
    cp.lengthMs = p.length->load();
    cp.snapToWord = p.snap->load() > 0.5f;
    cp.timeIndex = juce::roundToInt (p.time->load());
    cp.feedback = p.feedback->load();
    cp.lowCutHz = p.lowCut->load();
    cp.highCutHz = p.highCut->load();
    cp.pingPong = p.pingPong->load() > 0.5f;
    cp.reverbAmount = p.reverb->load();
    cp.reverbSize = p.size->load();
    cp.duckAmount = p.duck->load();
    cp.duckReleaseMs = p.duckRelease->load();
    cp.wetDb = p.wet->load();
    cp.dryOn = p.dry->load() > 0.5f;
    return cp;
}

void AutoThrowAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    double hostBpm = 120.0;
    bool playing = false;
    bool known = false;

    if (auto* head = getPlayHead())
    {
        if (auto position = head->getPosition())
        {
            playing = position->getIsPlaying();

            if (auto b = position->getBpm())
            {
                hostBpm = *b;
                known = true;
            }
        }
    }

    bpm.store (hostBpm);
    hostTempo.store (known);

    core.setParams (makeCoreParams(), hostBpm, playing);
    core.process (buffer, getTotalNumInputChannels(), getTotalNumOutputChannels());
}

juce::AudioProcessorEditor* AutoThrowAudioProcessor::createEditor()
{
    return new AutoThrowAudioProcessorEditor (*this);
}

void AutoThrowAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void AutoThrowAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AutoThrowAudioProcessor();
}
