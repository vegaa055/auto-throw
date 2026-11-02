#include "ThrowDisplay.h"
#include "ThrowLookAndFeel.h"

ThrowDisplay::ThrowDisplay (DisplayFeed& feedToUse, juce::RangedAudioParameter& thresholdParam)
    : feed (feedToUse), threshold (thresholdParam)
{
    points.resize ((size_t) visiblePoints);
    setOpaque (false);
    startTimerHz (30);
}

void ThrowDisplay::resized()
{
    plot = getLocalBounds().toFloat().reduced (12.0f, 10.0f).withTrimmedLeft (22.0f).withTrimmedTop (14.0f);
}

void ThrowDisplay::timerCallback()
{
    DisplayPoint p;

    while (feed.points.pop (p))
    {
        points[(size_t) (p.index % visiblePoints)] = p;
        latest = juce::jmax (latest, p.index);
    }

    DisplayEvent e;
    bool fired = false;

    while (feed.events.pop (e))
    {
        events.push_back (e);
        fired = fired || e.fired;
    }

    const auto oldest = latest - visiblePoints;
    events.erase (std::remove_if (events.begin(), events.end(),
                                  [oldest] (const DisplayEvent& ev) { return juce::jmax (ev.captureEnd, ev.landing) < oldest; }),
                  events.end());

    if (fired && onThrowFired)
        onThrowFired();

    repaint();
}

float ThrowDisplay::dbToY (float db) const noexcept
{
    return juce::jmap (juce::jlimit (minDb, maxDb, db), minDb, maxDb, plot.getBottom(), plot.getY());
}

float ThrowDisplay::yToDb (float y) const noexcept
{
    return juce::jmap (y, plot.getBottom(), plot.getY(), minDb, maxDb);
}

float ThrowDisplay::indexToX (juce::int64 index) const noexcept
{
    return plot.getRight() - (float) (latest - index) * plot.getWidth() / (float) (visiblePoints - 1);
}

float ThrowDisplay::thresholdDb() const
{
    return threshold.convertFrom0to1 (threshold.getValue());
}

void ThrowDisplay::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (Palette::panel);
    g.fillRoundedRectangle (bounds, 8.0f);
    g.setColour (Palette::panelEdge);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 8.0f, 1.0f);

    // level grid
    g.setFont (ThrowLookAndFeel::font (10.0f));

    for (float db = -12.0f; db >= -60.0f; db -= 12.0f)
    {
        const float y = dbToY (db);
        g.setColour (Palette::panelEdge.withAlpha (0.7f));
        g.drawHorizontalLine ((int) y, plot.getX(), plot.getRight());
        g.setColour (Palette::textDim.withAlpha (0.6f));
        g.drawText (juce::String ((int) db), juce::Rectangle<float> (plot.getX() - 26.0f, y - 6.0f, 22.0f, 12.0f),
                    juce::Justification::centredRight, false);
    }

    // legend
    const auto legend = bounds.reduced (12.0f, 6.0f).withHeight (14.0f).withTrimmedLeft (22.0f);
    const auto drawKey = [&] (juce::Colour c, const juce::String& text, float x)
    {
        g.setColour (c);
        g.fillRoundedRectangle (x, legend.getCentreY() - 3.0f, 10.0f, 6.0f, 2.0f);
        g.setColour (Palette::textDim);
        g.drawText (text, juce::Rectangle<float> (x + 14.0f, legend.getY(), 90.0f, legend.getHeight()),
                    juce::Justification::centredLeft, false);
    };

    drawKey (Palette::text.withAlpha (0.5f), "VOCAL", legend.getX());
    drawKey (Palette::throwHot, "THROWN", legend.getX() + 70.0f);
    drawKey (Palette::echo, "ECHO", legend.getX() + 150.0f);
    g.setColour (Palette::textDim.withAlpha (0.7f));
    g.drawText ("drag the line to set the threshold", legend, juce::Justification::centredRight, false);

    if (latest < 0)
        return;

    g.saveState();
    g.reduceClipRegion (plot.toNearestInt());

    // thrown words, skipped phrase ends, and where the echoes land
    for (const auto& e : events)
    {
        const float x1 = indexToX (e.captureStart);
        const float x2 = indexToX (e.captureEnd);
        const auto region = juce::Rectangle<float>::leftTopRightBottom (x1, plot.getY(), juce::jmax (x1 + 1.0f, x2), plot.getBottom());

        if (e.fired)
        {
            g.setColour (Palette::throwHot.withAlpha (0.22f));
            g.fillRect (region);
            g.setColour (Palette::throwHot.withAlpha (0.8f));
            g.fillRect (region.withHeight (3.0f));

            const float lx = indexToX (e.landing);

            if (lx <= plot.getRight())
            {
                const float dashes[] = { 4.0f, 3.0f };
                g.setColour (Palette::echo.withAlpha (0.8f));
                g.drawDashedLine ({ x2, plot.getY() + 1.5f, lx, plot.getY() + 1.5f }, dashes, 2, 1.0f);
                g.fillRect (lx - 1.0f, plot.getY(), 2.0f, plot.getHeight());
            }
        }
        else
        {
            g.setColour (Palette::textDim.withAlpha (0.08f));
            g.fillRect (region);
            g.setColour (Palette::textDim.withAlpha (0.4f));
            g.drawVerticalLine ((int) x2, plot.getY(), plot.getBottom());
        }
    }

    // vocal level, with held manual sends shaded orange
    juce::Path vocal, echoLine;
    bool echoStarted = false;
    vocal.startNewSubPath (indexToX (latest - visiblePoints + 1), plot.getBottom());

    for (auto idx = latest - visiblePoints + 1; idx <= latest; ++idx)
    {
        const auto& p = points[(size_t) (idx % visiblePoints)];
        const bool valid = p.index == idx;
        const float x = indexToX (idx);

        vocal.lineTo (x, dbToY (valid ? p.inputDb : minDb));

        if (valid && (p.flags & DisplayPoint::sending) != 0)
        {
            g.setColour (Palette::throwHot.withAlpha (0.22f));
            g.fillRect (x - 1.5f, plot.getY(), 3.0f, plot.getHeight());
        }

        const float ey = dbToY (valid ? p.echoDb : minDb);

        if (! echoStarted)
        {
            echoLine.startNewSubPath (x, ey);
            echoStarted = true;
        }
        else
        {
            echoLine.lineTo (x, ey);
        }
    }

    vocal.lineTo (plot.getRight(), plot.getBottom());
    vocal.closeSubPath();

    g.setColour (Palette::text.withAlpha (0.14f));
    g.fillPath (vocal);
    g.setColour (Palette::text.withAlpha (0.45f));
    g.strokePath (vocal, juce::PathStrokeType (1.0f));

    g.setColour (Palette::echo.withAlpha (0.2f));
    g.strokePath (echoLine, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (Palette::echo);
    g.strokePath (echoLine, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.restoreState();

    // threshold
    const float ty = dbToY (thresholdDb());
    const float dashes[] = { 6.0f, 4.0f };
    g.setColour (Palette::detect.withAlpha (dragging ? 1.0f : 0.8f));
    g.drawDashedLine ({ plot.getX(), ty, plot.getRight(), ty }, dashes, 2, dragging ? 2.0f : 1.2f);
    g.setFont (ThrowLookAndFeel::font (10.5f, true));
    g.drawText ("THRESHOLD " + juce::String (thresholdDb(), 1) + " dB",
                juce::Rectangle<float> (plot.getRight() - 160.0f, ty - 15.0f, 156.0f, 13.0f),
                juce::Justification::centredRight, false);
}

bool ThrowDisplay::nearThreshold (float y) const
{
    return std::abs (y - dbToY (thresholdDb())) < 10.0f;
}

void ThrowDisplay::mouseMove (const juce::MouseEvent& e)
{
    setMouseCursor (nearThreshold ((float) e.y) ? juce::MouseCursor::UpDownResizeCursor
                                                : juce::MouseCursor::NormalCursor);
}

void ThrowDisplay::mouseDown (const juce::MouseEvent& e)
{
    if (! nearThreshold ((float) e.y))
        return;

    dragging = true;
    threshold.beginChangeGesture();
    setThresholdFromY ((float) e.y);
}

void ThrowDisplay::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging)
        setThresholdFromY ((float) e.y);
}

void ThrowDisplay::mouseUp (const juce::MouseEvent&)
{
    if (! dragging)
        return;

    dragging = false;
    threshold.endChangeGesture();
    repaint();
}

void ThrowDisplay::setThresholdFromY (float y)
{
    const auto& range = threshold.getNormalisableRange();
    const float db = range.snapToLegalValue (juce::jlimit (range.start, range.end, yToDb (y)));
    threshold.setValueNotifyingHost (threshold.convertTo0to1 (db));
    repaint();
}
