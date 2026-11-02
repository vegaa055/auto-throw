#include <JuceHeader.h>
#include "PluginProcessor.h"

/*
    Renders the editor to a PNG after playing a vocal through the processor,
    so the display can be checked (and screenshotted) without a DAW.

    Usage: AutoThrowSnapshot <input.wav> <output.png> [seconds]
    Stops `seconds` into the file (default 9.5) and captures the editor.
*/

int main (int argc, char* argv[])
{
    if (argc < 3)
    {
        std::cout << "usage: AutoThrowSnapshot <input.wav> <output.png> [seconds]" << std::endl;
        return 1;
    }

    juce::ScopedJuceInitialiser_GUI gui;

    const auto cwd = juce::File::getCurrentWorkingDirectory();
    const auto inputFile = cwd.getChildFile (argv[1]);
    const auto outputFile = cwd.getChildFile (argv[2]);
    const double stopAt = argc > 3 ? juce::String (argv[3]).getDoubleValue() : 9.5;

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (inputFile));

    if (reader == nullptr)
    {
        std::cout << "couldn't read " << inputFile.getFullPathName() << std::endl;
        return 1;
    }

    const double sampleRate = reader->sampleRate;
    const int total = juce::jmin ((int) reader->lengthInSamples, (int) (stopAt * sampleRate));
    juce::AudioBuffer<float> audio (2, total);
    reader->read (&audio, 0, total, 0, true, true);

    constexpr int blockSize = 512;
    AutoThrowAudioProcessor processor;
    processor.setPlayConfigDetails (2, 2, sampleRate, blockSize);
    processor.prepareToPlay (sampleRate, blockSize);

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    juce::MidiBuffer midi;

    // Let the editor's timers drain the display feed as playback goes, the
    // way they would in a host.
    const int blocksPerFrame = juce::jmax (1, (int) (sampleRate / 30.0 / blockSize));
    int block = 0;

    for (int pos = 0; pos < total; pos += blockSize, ++block)
    {
        const int n = juce::jmin (blockSize, total - pos);
        juce::AudioBuffer<float> chunk (audio.getArrayOfWritePointers(), 2, pos, n);
        processor.processBlock (chunk, midi);

        if (block % blocksPerFrame == 0)
        {
            juce::Thread::sleep (40);
            juce::Timer::callPendingTimersSynchronously();
        }
    }

    juce::Thread::sleep (40);
    juce::Timer::callPendingTimersSynchronously();

    const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
    outputFile.deleteFile();

    if (auto stream = outputFile.createOutputStream())
    {
        juce::PNGImageFormat png;
        png.writeImageToStream (image, *stream);
        std::cout << "wrote " << outputFile.getFullPathName() << std::endl;
    }

    editor.reset();
    return 0;
}
