/*
    Kilocycle - retro broadcast console channel strip
    Copyright (C) 2026  Kilocycle Audio
    Licensed under the GNU General Public License v3 or later. See LICENSE.
*/

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "Skin.h"
#include "../dsp/ConsoleEQ.h"

namespace kc::gui
{

/** The illuminated tuning window - the centrepiece of the panel.

    It is a real control, not decoration: the amber trace is the live response of
    the tone section, and the red cursor tunes the mid band. Drag it, or scroll
    over it for fine adjustment.

    The x axis is logarithmic across the whole audio band, so the shelves and the
    bell all read at their true positions; the cursor itself is limited to the mid
    band's own range.
*/
class TuningDial final : public juce::Component
{
public:
    TuningDial (juce::RangedAudioParameter& midFrequencyParameter);
    ~TuningDial() override;

    /** Called from the editor's refresh timer. Repaints only on a real change. */
    void setEqSettings (const dsp::EqSettings&, double sampleRate);

    void paint (juce::Graphics&) override;
    void resized() override;

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    static constexpr float kDisplayLowHz  = 20.0f;
    static constexpr float kDisplayHighHz = 20000.0f;
    static constexpr float kDisplayDb     = 20.0f;

    float frequencyForX (float x) const;
    float xForFrequency (float hz) const;
    float yForDb (float db) const;
    void  setFrequencyFromMouse (const juce::MouseEvent&);

    void paintScale (juce::Graphics&) const;
    void rebuildScaleCache();

    juce::RangedAudioParameter& parameter;
    std::unique_ptr<juce::ParameterAttachment> attachment;

    juce::Rectangle<float> glass, trace;
    juce::Image scaleCache;

    dsp::EqSettings settings;
    double sampleRate  = 44100.0;
    float  cursorHz    = 1100.0f;
    bool   dragging    = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TuningDial)
};

} // namespace kc::gui
