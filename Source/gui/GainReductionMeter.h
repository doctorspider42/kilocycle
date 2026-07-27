/*
    Kilocycle - retro broadcast console channel strip
    Copyright (C) 2026  Kilocycle Audio
    Licensed under the GNU General Public License v3 or later. See LICENSE.
*/

#pragma once

#include "Skin.h"
#include <functional>

namespace kc::gui
{

/** Segmented gain-reduction indicator, lit from the right like the tube ladders
    on old limiting amplifiers. Peak-hold marker included, because a compressor
    you cannot read is a compressor you cannot set. */
class GainReductionMeter final : public juce::Component,
                                 private juce::Timer
{
public:
    /** @param provider  returns the current reduction in dB (positive). */
    explicit GainReductionMeter (std::function<float()> provider);
    ~GainReductionMeter() override;

    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;

    std::function<float()> provider;

    static constexpr int   numSegments = 22;
    static constexpr float rangeDb     = 22.0f;

    float smoothed = 0.0f;
    float peakHold = 0.0f;
    int   peakAgeFrames = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GainReductionMeter)
};

} // namespace kc::gui
