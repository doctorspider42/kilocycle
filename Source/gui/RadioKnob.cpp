/*
    Kilocycle - retro broadcast console channel strip
    Copyright (C) 2026  Kilocycle Audio
    Licensed under the GNU General Public License v3 or later. See LICENSE.
*/

#include "RadioKnob.h"

namespace kc::gui
{

RadioKnob::RadioKnob (juce::AudioProcessorValueTreeState& state,
                      const juce::String& parameterID,
                      juce::String captionToUse,
                      int ticks,
                      bool detents)
    : caption (std::move (captionToUse))
{
    slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                juce::MathConstants<float>::pi * 2.75f,
                                true);
    slider.setMouseDragSensitivity (detents ? 90 : 230);
    slider.setScrollWheelEnabled (true);
    slider.getProperties().set ("ticks", ticks);
    slider.getProperties().set ("detents", detents);
    slider.onValueChange = [this] { repaint(); };
    slider.onDragStart   = [this] { repaint(); };
    slider.onDragEnd     = [this] { repaint(); };
    slider.addMouseListener (this, false);

    if (auto* param = state.getParameter (parameterID))
        slider.setTooltip (param->getName (64));

    addAndMakeVisible (slider);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        state, parameterID, slider);
}

RadioKnob::~RadioKnob() = default;

void RadioKnob::setTextHeights (float legend, float readout)
{
    legendHeight  = legend;
    readoutHeight = readout;
    resized();
    repaint();
}

void RadioKnob::resized()
{
    auto area = getLocalBounds().toFloat();
    area.removeFromTop (legendHeight);
    area.removeFromBottom (readoutHeight);

    const auto side = juce::jmin (area.getWidth(), area.getHeight());
    slider.setBounds (area.withSizeKeepingCentre (side, side).toNearestInt());
}

void RadioKnob::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();

    skin::drawEngraved (g, caption.toUpperCase(), skin::legendFont (legendHeight * 0.78f, true),
                        area.removeFromTop (legendHeight), juce::Justification::centred,
                        skin::colour::ink, 1.3f);

    const auto readoutArea = getLocalBounds().toFloat().removeFromBottom (readoutHeight);
    const auto text = slider.getTextFromValue (slider.getValue());

    skin::drawEngraved (g, text, skin::legendFont (readoutHeight * 0.76f),
                        readoutArea, juce::Justification::centred,
                        slider.isMouseOverOrDragging() ? skin::colour::amberDeep
                                                       : skin::colour::inkSoft);
}

} // namespace kc::gui
