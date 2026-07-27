/*
    Kilocycle - retro broadcast console channel strip
    Copyright (C) 2026  Kilocycle Audio
    Licensed under the GNU General Public License v3 or later. See LICENSE.
*/

#pragma once

#include "PluginProcessor.h"

#include "gui/RadioLookAndFeel.h"
#include "gui/RadioKnob.h"
#include "gui/VuMeter.h"
#include "gui/GainReductionMeter.h"
#include "gui/TuningDial.h"
#include "gui/ValveGlow.h"
#include "gui/RockerSwitch.h"

namespace kc
{

/** Rectangles the static backdrop needs to know about. */
struct PanelLayout
{
    juce::Rectangle<float> faceplate, nameplate, headerCentre, bypassLegend,
                           input, tone, leveller, valve, toneNote, footer;
};

/** The engraved panel itself: wood, enamel, nameplate, section frames.

    Nothing here moves, so it is buffered into an image and only re-rendered when
    the window is resized.
*/
class Backdrop final : public juce::Component
{
public:
    Backdrop();

    void setLayout (const PanelLayout&);
    void paint (juce::Graphics&) override;

private:
    void paintPanel (juce::Graphics&);

    PanelLayout layout;
    juce::Image cacheImage;
};

class KilocycleEditor final : public juce::AudioProcessorEditor,
                              private juce::Timer
{
public:
    explicit KilocycleEditor (KilocycleProcessor&);
    ~KilocycleEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void layOutContent();

    static constexpr int kLogicalWidth  = 1080;
    static constexpr int kLogicalHeight = 612;

    KilocycleProcessor& processor;
    gui::RadioLookAndFeel lookAndFeel;

    /** Everything lives inside this child, which is scaled by a single transform
        so the whole panel resizes as one piece. */
    juce::Component content;
    Backdrop backdrop;

    gui::TuningDial dial;
    gui::VuMeter    vu;
    gui::ValveGlow  valve;
    gui::GainReductionMeter grMeter;

    gui::RadioKnob trim, lowCut;
    gui::RadioKnob lowGain, midGain, highGain, midQ;
    gui::RadioKnob lowFreq, midFreq, highFreq;
    gui::RadioKnob threshold, ratio, attack, release, compMix;
    gui::RadioKnob drive, output, quality;

    gui::RockerSwitch bypassSwitch { "BYP", "ON" };
    gui::RockerSwitch autoSwitch   { "AUTO", "MAN" };

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment, autoAttachment;

    juce::TooltipWindow tooltips { this, 650 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KilocycleEditor)
};

} // namespace kc
