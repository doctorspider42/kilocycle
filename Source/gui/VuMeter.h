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

/** A moving-coil VU meter.

    The integration is done in the DSP (see dsp::VuFollower); what happens here is
    the *mechanical* half of the illusion - a lightly damped spring, so the needle
    overshoots a touch on transients and settles back the way a real movement does.

    The engraved face is rendered once into a cached image; each frame only the
    needle and the glass are redrawn.
*/
class VuMeter final : public juce::Component,
                      private juce::Timer
{
public:
    /** @param levelProvider  returns the level in dB relative to 0 VU. */
    explicit VuMeter (std::function<float()> levelProvider);
    ~VuMeter() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void setCaption (juce::String makerText);

private:
    void timerCallback() override;
    void paintFace (juce::Graphics&) const;
    void rebuildFaceCache();

    static float fractionForDb (float db);
    float angleForFraction (float fraction) const;
    juce::Point<float> pointAt (float angle, float radius) const;

    std::function<float()> provider;
    juce::String caption { skin::u8 ("KILOCYCLE  \xc2\xb7  TYPE 45") };

    juce::Image faceCache;

    juce::Point<float> pivot;
    float radius     = 100.0f;
    float maxAngle   = 0.62f;

    float angle    = 0.0f;
    float velocity = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VuMeter)
};

} // namespace kc::gui
