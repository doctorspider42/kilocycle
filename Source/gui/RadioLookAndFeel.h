/*
    Kilocycle - retro broadcast console channel strip
    Copyright (C) 2026  Kilocycle Audio
    Licensed under the GNU General Public License v3 or later. See LICENSE.
*/

#pragma once

#include "Skin.h"

namespace kc::gui
{

/** Bakelite knobs with brass pointers and engraved tick rings, plus
    period-correct tooltips and menus.

    Per-knob details are read from the slider's component properties, so a
    single look-and-feel instance can serve everything on the panel:

      - "ticks"   (int)  number of engraved marks around the knob, default 11
      - "detents" (bool) draw the ring as discrete positions rather than a sweep
*/
class RadioLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    RadioLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider&) override;

    juce::Font getLabelFont (juce::Label&) override;

    void drawTooltip (juce::Graphics&, const juce::String& text, int width, int height) override;

    void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RadioLookAndFeel)
};

} // namespace kc::gui
