/*
    Kilocycle - retro broadcast console channel strip
    Copyright (C) 2026  Kilocycle Audio
    Licensed under the GNU General Public License v3 or later. See LICENSE.
*/

#include "RockerSwitch.h"

namespace kc::gui
{

using juce::Colour;
using juce::ColourGradient;
using juce::Graphics;
using juce::Path;
using juce::Rectangle;

RockerSwitch::RockerSwitch (juce::String topLabel, juce::String bottomLabel)
    : juce::Button ("rocker"), top (std::move (topLabel)), bottom (std::move (bottomLabel))
{
    setClickingTogglesState (true);
    position = getToggleState() ? 1.0f : 0.0f;
}

RockerSwitch::~RockerSwitch() = default;

void RockerSwitch::buttonStateChanged()
{
    if (! isTimerRunning())
        startTimerHz (60);
}

void RockerSwitch::timerCallback()
{
    const auto target = getToggleState() ? 1.0f : 0.0f;
    position += 0.28f * (target - position);

    if (std::abs (target - position) < 0.004f)
    {
        position = target;
        stopTimer();
    }

    repaint();
}

void RockerSwitch::paintButton (Graphics& g, bool highlighted, bool down)
{
    auto area = getLocalBounds().toFloat();

    // Ensure the animation starts even if the state was set programmatically.
    const auto target = getToggleState() ? 1.0f : 0.0f;
    if (! juce::approximatelyEqual (position, target) && ! isTimerRunning())
        startTimerHz (60);

    const auto lampArea = area.removeFromTop (area.getHeight() * 0.24f).reduced (area.getWidth() * 0.30f, 1.0f);
    const auto body = area.reduced (1.0f);

    // ---- lamp -------------------------------------------------------------
    {
        const auto on = lampInverted ? position < 0.5f : position >= 0.5f;
        const auto brightness = lampInverted ? 1.0f - position : position;
        const auto lamp = lampArea.withSizeKeepingCentre (juce::jmin (lampArea.getWidth(), lampArea.getHeight()),
                                                          juce::jmin (lampArea.getWidth(), lampArea.getHeight()));

        if (brightness > 0.02f)
        {
            ColourGradient halo (lampColour.withAlpha (0.55f * brightness), lamp.getCentreX(), lamp.getCentreY(),
                                 juce::Colours::transparentBlack, lamp.getCentreX(),
                                 lamp.getCentreY() + lamp.getHeight() * 1.6f, true);
            g.setGradientFill (halo);
            g.fillEllipse (lamp.expanded (lamp.getWidth() * 1.4f));
        }

        g.setColour (on ? lampColour.brighter (0.2f * brightness)
                        : skin::colour::lampOff);
        g.fillEllipse (lamp);

        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.drawEllipse (lamp, 0.9f);
    }

    // ---- bezel ------------------------------------------------------------
    Path bezel;
    bezel.addRoundedRectangle (body, 4.0f);

    g.setGradientFill (ColourGradient (skin::colour::bakeliteMid, body.getCentreX(), body.getY(),
                                       Colour (0xff0f0c0a),       body.getCentreX(), body.getBottom(), false));
    g.fillPath (bezel);

    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.strokePath (bezel, juce::PathStrokeType (1.0f));

    // ---- paddle -----------------------------------------------------------
    const auto paddle = body.reduced (body.getWidth() * 0.14f, body.getHeight() * 0.10f);
    const auto splitY = paddle.getY() + paddle.getHeight() * juce::jmap (position, 0.62f, 0.38f);

    Path paddlePath;
    paddlePath.addRoundedRectangle (paddle, 3.0f);

    // The half that is pressed in sits in shadow; the raised half catches light.
    const auto upperRaised = position >= 0.5f;

    {
        Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (paddlePath);

        const auto upper = paddle.withBottom (splitY);
        const auto lower = paddle.withTop (splitY);

        auto paintHalf = [&g] (Rectangle<float> r, bool raised, bool fromTop)
        {
            const auto a = raised ? skin::colour::creamLite : skin::colour::creamDark.darker (0.25f);
            const auto b = raised ? skin::colour::cream     : skin::colour::creamDark.darker (0.55f);

            g.setGradientFill (ColourGradient (fromTop ? a : b, r.getCentreX(), r.getY(),
                                               fromTop ? b : a, r.getCentreX(), r.getBottom(), false));
            g.fillRect (r);
        };

        paintHalf (upper, upperRaised, true);
        paintHalf (lower, ! upperRaised, false);

        // Crease where the paddle folds.
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.drawLine (paddle.getX(), splitY, paddle.getRight(), splitY, 1.0f);

        skin::applyGrain (g, paddle, 0.05f);
    }

    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.strokePath (paddlePath, juce::PathStrokeType (0.9f));

    // ---- markings ---------------------------------------------------------
    {
        const auto font = skin::legendFont (juce::jmax (7.0f, paddle.getHeight() * 0.16f), true);

        g.setColour (skin::colour::ink.withAlpha (upperRaised ? 0.85f : 0.45f));
        skin::drawTracked (g, top, font, paddle.withBottom (splitY).reduced (0.0f, 1.0f), 0.9f,
                           juce::Justification::centred);

        g.setColour (skin::colour::ink.withAlpha (upperRaised ? 0.45f : 0.85f));
        skin::drawTracked (g, bottom, font, paddle.withTop (splitY).reduced (0.0f, 1.0f), 0.9f,
                           juce::Justification::centred);
    }

    if (highlighted || down)
    {
        g.setColour (skin::colour::amberLite.withAlpha (down ? 0.18f : 0.09f));
        g.fillPath (paddlePath);
    }
}

} // namespace kc::gui
