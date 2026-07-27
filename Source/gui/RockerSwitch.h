/*
    Kilocycle - retro broadcast console channel strip
    Copyright (C) 2026  Kilocycle Audio
    Licensed under the GNU General Public License v3 or later. See LICENSE.
*/

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Skin.h"

namespace kc::gui
{

/** An ivory rocker switch in a bakelite bezel, with a lamp that lights when the
    switch is on. Animated - the paddle tips over instead of snapping. */
class RockerSwitch final : public juce::Button,
                           private juce::Timer
{
public:
    RockerSwitch (juce::String topLabel, juce::String bottomLabel);
    ~RockerSwitch() override;

    void paintButton (juce::Graphics&, bool highlighted, bool down) override;

    /** Lamp colour when the switch is on. */
    void setLampColour (juce::Colour c) { lampColour = c; repaint(); }

    /** Inverts the lamp, for switches where "off" is the alarming state. */
    void setLampInverted (bool shouldInvert) { lampInverted = shouldInvert; repaint(); }

private:
    void timerCallback() override;
    void buttonStateChanged() override;

    juce::String top, bottom;
    juce::Colour lampColour { skin::colour::lampOn };
    bool  lampInverted = false;
    float position = 0.0f;   // 0 = off (lower), 1 = on (upper)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RockerSwitch)
};

} // namespace kc::gui
