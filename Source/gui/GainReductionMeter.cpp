/*
    Kilocycle - retro broadcast console channel strip
    Copyright (C) 2026  Kilocycle Audio
    Licensed under the GNU General Public License v3 or later. See LICENSE.
*/

#include "GainReductionMeter.h"

namespace kc::gui
{

using juce::Graphics;
using juce::Rectangle;

GainReductionMeter::GainReductionMeter (std::function<float()> providerToUse)
    : provider (std::move (providerToUse))
{
    setInterceptsMouseClicks (false, false);
    smoothed = peakHold = provider != nullptr ? provider() : 0.0f;
    startTimerHz (30);
}

GainReductionMeter::~GainReductionMeter() = default;

void GainReductionMeter::timerCallback()
{
    const auto target = provider != nullptr ? provider() : 0.0f;

    // Fast up, gentle down - the eye wants to see the grab and the recovery.
    smoothed += (target > smoothed ? 0.55f : 0.16f) * (target - smoothed);

    if (smoothed >= peakHold)
    {
        peakHold = smoothed;
        peakAgeFrames = 0;
    }
    else if (++peakAgeFrames > 45)
    {
        peakHold += 0.12f * (smoothed - peakHold);
    }

    repaint();
}

void GainReductionMeter::paint (Graphics& g)
{
    auto area = getLocalBounds().toFloat();

    const auto legend = area.removeFromBottom (11.0f);
    const auto window = area.reduced (1.0f);

    skin::paintRecessedWindow (g, window, 3.0f);

    const auto inner = window.reduced (3.0f);
    const auto gap   = 1.6f;
    const auto segW  = (inner.getWidth() - gap * (numSegments - 1)) / (float) numSegments;

    const auto litFraction  = juce::jlimit (0.0f, 1.0f, smoothed / rangeDb);
    const auto peakFraction = juce::jlimit (0.0f, 1.0f, peakHold / rangeDb);

    for (int i = 0; i < numSegments; ++i)
    {
        // Segment 0 sits at the right-hand edge: reduction grows leftwards.
        const auto fromRight = (float) i / (float) numSegments;
        const auto seg = Rectangle<float> (inner.getRight() - (segW + gap) * (float) (i + 1) + gap,
                                           inner.getY(), segW, inner.getHeight());

        const auto lit  = fromRight < litFraction;
        const auto isPeak = ! lit && peakFraction > fromRight
                                  && peakFraction <= fromRight + 1.0f / (float) numSegments;

        auto colour = juce::Colour (skin::colour::amber);

        if (fromRight > 0.70f)
            colour = skin::colour::redZone.brighter (0.25f);
        else if (fromRight > 0.42f)
            colour = skin::colour::amber.interpolatedWith (skin::colour::redZone, 0.4f);

        if (lit)
        {
            g.setColour (colour.withAlpha (0.95f));
            g.fillRoundedRectangle (seg, 1.2f);

            g.setColour (colour.brighter (0.5f).withAlpha (0.5f));
            g.fillRoundedRectangle (seg.reduced (segW * 0.28f, seg.getHeight() * 0.30f), 1.0f);
        }
        else
        {
            g.setColour (juce::Colours::black.withAlpha (0.42f));
            g.fillRoundedRectangle (seg, 1.2f);

            if (isPeak)
            {
                g.setColour (colour.withAlpha (0.55f));
                g.fillRoundedRectangle (seg, 1.2f);
            }
        }
    }

    // Faint amber bloom over the lit part - the glow of real indicator lamps.
    if (litFraction > 0.01f)
    {
        const auto glow = Rectangle<float> (inner.getWidth() * litFraction, inner.getHeight())
                              .withRightX (inner.getRight())
                              .withY (inner.getY())
                              .expanded (2.0f, 3.0f);

        g.setGradientFill (juce::ColourGradient (skin::colour::amber.withAlpha (0.22f),
                                                 glow.getRight(), glow.getCentreY(),
                                                 juce::Colours::transparentBlack,
                                                 glow.getX(), glow.getCentreY(), false));
        g.fillRect (glow);
    }

    skin::paintGlassSheen (g, window, 3.0f);

    // ---- legend -----------------------------------------------------------
    const auto font = skin::legendFont (8.5f, true);
    g.setColour (skin::colour::inkSoft);

    skin::drawTracked (g, "20", font, legend.withWidth (24.0f), 0.6f, juce::Justification::left);
    skin::drawTracked (g, "GAIN REDUCTION dB", font, legend, 1.1f, juce::Justification::centred);
    skin::drawTracked (g, "0", font, legend.withTrimmedLeft (legend.getWidth() - 14.0f), 0.6f,
                       juce::Justification::right);
}

} // namespace kc::gui
