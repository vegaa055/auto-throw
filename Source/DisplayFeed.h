#pragma once

#include <JuceHeader.h>

/** Lock-free single-producer/single-consumer queue of trivially copyable items. */
template <typename Item, int capacity>
class SpscQueue
{
public:
    bool push (const Item& item) noexcept
    {
        if (fifo.getFreeSpace() < 1)
            return false;

        const auto scope = fifo.write (1);
        items[(size_t) (scope.blockSize1 > 0 ? scope.startIndex1 : scope.startIndex2)] = item;
        return true;
    }

    bool pop (Item& item) noexcept
    {
        if (fifo.getNumReady() < 1)
            return false;

        const auto scope = fifo.read (1);
        item = items[(size_t) (scope.blockSize1 > 0 ? scope.startIndex1 : scope.startIndex2)];
        return true;
    }

private:
    juce::AbstractFifo fifo { capacity };
    std::array<Item, (size_t) capacity> items {};
};

/** One column of the editor's scrolling display (a few milliseconds of audio). */
struct DisplayPoint
{
    enum Flags : juce::uint8
    {
        inPhrase = 1,   // detector thinks the vocal is mid-phrase
        sending  = 2    // the manual THROW send is open
    };

    juce::int64 index = 0;      // absolute point number; consecutive points differ by one
    float inputDb = -100.0f;    // detector level (what the threshold compares against)
    float echoDb = -100.0f;     // wet output level after ducking
    juce::uint8 flags = 0;
};

/** A confirmed phrase end. All positions are in display-point units. */
struct DisplayEvent
{
    juce::int64 captureStart = 0;
    juce::int64 captureEnd = 0;
    juce::int64 landing = -1;   // where the first echo starts, or -1 if nothing was thrown
    bool fired = false;         // false when skipped by "Every" or with Auto off
};

/** Audio thread -> editor. Points arrive at pointsPerSecond. */
struct DisplayFeed
{
    static constexpr int pointsPerSecond = 200;

    SpscQueue<DisplayPoint, 2048> points;
    SpscQueue<DisplayEvent, 64> events;
};
