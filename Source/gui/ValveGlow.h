/*
    Kilocycle - retro broadcast console channel strip
    Copyright (C) 2026  Kilocycle Audio
    Licensed under the GNU General Public License v3 or later. See LICENSE.
*/

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "Skin.h"
#include "../dsp/ValveModels.h"
#include <functional>

namespace kc::gui
{

/** The valve socket.

    Two jobs. It shows the valve working - the heater comes up with the drive
    setting and breathes with the programme, so you can see the stage doing
    something before you can hear it. The flicker is a sum of slow incommensurate
    sines, which looks organic without needing a random generator per frame.

    And it is the socket: click it to pull the valve and fit a different one. The
    bottle is redrawn for the type that is in - a fat octal with a bakelite base
    looks nothing like a slim all-glass miniature, and that difference is most of
    the fun of swapping.
*/
class ValveGlow final : public juce::Component,
                        public juce::SettableTooltipClient,
                        private juce::Timer
{
public:
    /** @param valveParameter  the choice parameter selecting the valve type
        @param heatProvider    returns 0 .. 1 */
    ValveGlow (juce::RangedAudioParameter& valveParameter,
               std::function<float()> heatProvider);
    ~ValveGlow() override;

    void paint (juce::Graphics&) override;

    void mouseDown (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void showValveMenu();

    const dsp::ValveModel& model() const noexcept { return dsp::valveModel (modelIndex); }

    juce::RangedAudioParameter& parameter;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    std::function<float()> provider;

    int   modelIndex = 0;
    float heat    = 0.0f;
    float flicker = 1.0f;
    float phase   = 0.0f;
    bool  hovered = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ValveGlow)
};

} // namespace kc::gui
