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

/** A bakelite control knob with an engraved legend above and a readout below.

    Wraps a juce::Slider plus its parameter attachment so the panel layout code
    stays declarative.
*/
class RadioKnob final : public juce::Component
{
public:
    RadioKnob (juce::AudioProcessorValueTreeState& state,
               const juce::String& parameterID,
               juce::String caption,
               int ticks = 11,
               bool detents = false);

    ~RadioKnob() override;

    void resized() override;
    void paint (juce::Graphics&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit  (const juce::MouseEvent&) override { repaint(); }

    juce::Slider& getSlider() noexcept { return slider; }

    /** Height reserved for the legend and the readout. */
    void setTextHeights (float legend, float readout);

private:
    juce::Slider slider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    juce::String caption;

    float legendHeight  = 14.0f;
    float readoutHeight = 15.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RadioKnob)
};

} // namespace kc::gui
