#pragma once

#include <JuceHeader.h>
#include "AutoThrowCore.h"

class AutoThrowAudioProcessor : public juce::AudioProcessor
{
public:
    AutoThrowAudioProcessor();
    ~AutoThrowAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 10.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    DisplayFeed& getDisplayFeed() noexcept { return core.getDisplayFeed(); }
    double getDelayMs() const noexcept { return core.getDelayMs(); }
    double getBpm() const noexcept { return bpm.load(); }
    bool hasHostTempo() const noexcept { return hostTempo.load(); }

private:
    struct ParamRefs
    {
        std::atomic<float>* autoDetect = nullptr;
        std::atomic<float>* manualThrow = nullptr;
        std::atomic<float>* threshold = nullptr;
        std::atomic<float>* gap = nullptr;
        std::atomic<float>* every = nullptr;
        std::atomic<float>* length = nullptr;
        std::atomic<float>* snap = nullptr;
        std::atomic<float>* time = nullptr;
        std::atomic<float>* feedback = nullptr;
        std::atomic<float>* lowCut = nullptr;
        std::atomic<float>* highCut = nullptr;
        std::atomic<float>* pingPong = nullptr;
        std::atomic<float>* reverb = nullptr;
        std::atomic<float>* size = nullptr;
        std::atomic<float>* duck = nullptr;
        std::atomic<float>* duckRelease = nullptr;
        std::atomic<float>* wet = nullptr;
        std::atomic<float>* dry = nullptr;
    };

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    AutoThrowCore::Params makeCoreParams() const;

    ParamRefs p;
    AutoThrowCore core;

    std::atomic<double> bpm { 120.0 };
    std::atomic<bool> hostTempo { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AutoThrowAudioProcessor)
};
