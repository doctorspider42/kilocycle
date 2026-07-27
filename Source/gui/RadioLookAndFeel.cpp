/*
    Kilocycle - retro broadcast console channel strip
    Copyright (C) 2026  Kilocycle Audio
    Licensed under the GNU General Public License v3 or later. See LICENSE.
*/

#include "RadioLookAndFeel.h"

namespace kc::gui
{

using juce::Colour;
using juce::ColourGradient;
using juce::Graphics;
using juce::Path;
using juce::Rectangle;

namespace
{
    constexpr float kPi = 3.14159265358979f;
}

RadioLookAndFeel::RadioLookAndFeel()
{
    setColour (juce::Label::textColourId,            skin::colour::ink);
    setColour (juce::TooltipWindow::backgroundColourId, skin::colour::glass);
    setColour (juce::TooltipWindow::textColourId,       skin::colour::amberLite);
    setColour (juce::TooltipWindow::outlineColourId,    skin::colour::brass);
    setColour (juce::PopupMenu::backgroundColourId,     skin::colour::glass);
    setColour (juce::PopupMenu::textColourId,           skin::colour::amberLite);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, skin::colour::amberDeep);
    setColour (juce::PopupMenu::highlightedTextColourId,       skin::colour::creamLite);
}

juce::Font RadioLookAndFeel::getLabelFont (juce::Label& label)
{
    return skin::legendFont (juce::jlimit (9.0f, 15.0f, (float) label.getHeight() * 0.7f));
}

void RadioLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int width, int height,
                                         float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                                         juce::Slider& slider)
{
    const auto bounds = Rectangle<int> (x, y, width, height).toFloat();
    const auto square = bounds.withSizeKeepingCentre (juce::jmin (bounds.getWidth(), bounds.getHeight()),
                                                      juce::jmin (bounds.getWidth(), bounds.getHeight()));
    const auto centre = square.getCentre();
    const auto outer  = square.getWidth() * 0.5f - 1.0f;
    const auto knobR  = outer * 0.70f;

    const auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    const auto ticks   = (int) slider.getProperties().getWithDefault ("ticks", 11);
    const auto detents = (bool) slider.getProperties().getWithDefault ("detents", false);

    // ---- engraved tick ring ----------------------------------------------
    for (int i = 0; i < ticks; ++i)
    {
        const auto t  = ticks > 1 ? (float) i / (float) (ticks - 1) : 0.0f;
        const auto a  = rotaryStartAngle + t * (rotaryEndAngle - rotaryStartAngle);
        const auto s  = std::sin (a);
        const auto c  = std::cos (a);
        const auto major = (i == 0 || i == ticks - 1 || (! detents && i == ticks / 2));

        const auto r1 = outer * (major ? 0.985f : 0.965f);
        const auto r2 = outer * (major ? 0.855f : 0.895f);

        const auto p1 = centre.translated (s * r1, -c * r1);
        const auto p2 = centre.translated (s * r2, -c * r2);

        const auto reached = detents ? std::abs (a - angle) < 0.06f : a <= angle + 1.0e-4f;

        g.setColour (skin::colour::creamLite.withAlpha (0.8f));
        g.drawLine (p1.x, p1.y + 1.0f, p2.x, p2.y + 1.0f, major ? 1.6f : 1.1f);

        g.setColour (reached ? skin::colour::amberDeep.withAlpha (0.95f)
                             : skin::colour::inkSoft.withAlpha (0.75f));
        g.drawLine (p1.x, p1.y, p2.x, p2.y, major ? 1.6f : 1.1f);
    }

    // ---- shadow ----------------------------------------------------------
    const auto knobBounds = Rectangle<float> (knobR * 2.0f, knobR * 2.0f).withCentre (centre);

    {
        Path p;
        p.addEllipse (knobBounds.translated (0.0f, 1.5f));
        juce::DropShadow (juce::Colours::black.withAlpha (0.5f), (int) (knobR * 0.45f), { 0, 2 })
            .drawForPath (g, p);
    }

    // ---- fluted bakelite skirt -------------------------------------------
    {
        ColourGradient skirt (skin::colour::bakeliteLite, centre.x - knobR * 0.5f, centre.y - knobR * 0.7f,
                              skin::colour::bakelite,     centre.x + knobR * 0.6f, centre.y + knobR * 0.9f, false);
        skirt.addColour (0.55, skin::colour::bakeliteMid);
        g.setGradientFill (skirt);
        g.fillEllipse (knobBounds);

        const auto flutes = 22;
        for (int i = 0; i < flutes; ++i)
        {
            const auto a = (float) i / (float) flutes * kPi * 2.0f;
            const auto s = std::sin (a);
            const auto c = std::cos (a);

            const auto p1 = centre.translated (s * knobR * 0.995f, -c * knobR * 0.995f);
            const auto p2 = centre.translated (s * knobR * 0.80f,  -c * knobR * 0.80f);

            g.setColour (juce::Colours::black.withAlpha (0.35f));
            g.drawLine (p1.x, p1.y, p2.x, p2.y, knobR * 0.075f);

            const auto a2 = a + kPi / (float) flutes;
            const auto p3 = centre.translated (std::sin (a2) * knobR * 0.99f, -std::cos (a2) * knobR * 0.99f);
            const auto p4 = centre.translated (std::sin (a2) * knobR * 0.82f, -std::cos (a2) * knobR * 0.82f);

            g.setColour (skin::colour::bakeliteLite.withAlpha (0.30f));
            g.drawLine (p3.x, p3.y, p4.x, p4.y, knobR * 0.05f);
        }
    }

    // ---- brass ring ------------------------------------------------------
    {
        ColourGradient ring (skin::colour::brassLite, centre.x - knobR, centre.y - knobR,
                             skin::colour::brassDark, centre.x + knobR, centre.y + knobR, false);
        ring.addColour (0.5, skin::colour::brass);
        g.setGradientFill (ring);
        g.drawEllipse (knobBounds.reduced (knobR * 0.155f), knobR * 0.055f);
    }

    // ---- cap -------------------------------------------------------------
    const auto cap = knobBounds.reduced (knobR * 0.20f);
    {
        ColourGradient capGrad (skin::colour::bakeliteMid, cap.getX() + cap.getWidth() * 0.25f, cap.getY(),
                                Colour (0xff141110),       cap.getCentreX(), cap.getBottom(), false);
        g.setGradientFill (capGrad);
        g.fillEllipse (cap);

        // Specular sheen, upper left.
        ColourGradient sheen (juce::Colours::white.withAlpha (0.22f),
                              cap.getX() + cap.getWidth() * 0.30f, cap.getY() + cap.getHeight() * 0.16f,
                              juce::Colours::transparentWhite,
                              cap.getCentreX(), cap.getCentreY() + cap.getHeight() * 0.35f, true);
        g.setGradientFill (sheen);
        g.fillEllipse (cap);
    }

    // ---- pointer ---------------------------------------------------------
    {
        const auto s = std::sin (angle);
        const auto c = std::cos (angle);

        const auto from = centre.translated (s * knobR * 0.22f, -c * knobR * 0.22f);
        const auto to   = centre.translated (s * knobR * 0.87f, -c * knobR * 0.87f);

        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.drawLine (from.x + 0.8f, from.y + 1.0f, to.x + 0.8f, to.y + 1.0f,
                    juce::jmax (1.8f, knobR * 0.115f));

        g.setColour (slider.isEnabled() ? skin::colour::brassLite : skin::colour::inkSoft);
        g.drawLine (from.x, from.y, to.x, to.y, juce::jmax (1.6f, knobR * 0.10f));

        g.setColour (skin::colour::creamLite.withAlpha (0.85f));
        g.drawLine (from.x, from.y, to.x, to.y, juce::jmax (0.7f, knobR * 0.035f));
    }

    // ---- centre boss -----------------------------------------------------
    const auto boss = knobBounds.reduced (knobR * 0.80f);
    g.setColour (skin::colour::brass.withAlpha (0.9f));
    g.fillEllipse (boss);
    g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.drawEllipse (boss, 0.6f);
}

void RadioLookAndFeel::drawTooltip (Graphics& g, const juce::String& text, int width, int height)
{
    const auto area = Rectangle<float> (0.0f, 0.0f, (float) width, (float) height);

    g.setColour (skin::colour::glass);
    g.fillRoundedRectangle (area, 4.0f);

    g.setColour (skin::colour::brass.withAlpha (0.8f));
    g.drawRoundedRectangle (area.reduced (0.5f), 4.0f, 1.0f);

    g.setColour (skin::colour::amberLite);
    g.setFont (skin::legendFont (12.5f));
    g.drawFittedText (text, area.reduced (7.0f, 4.0f).toNearestInt(),
                      juce::Justification::centred, 3);
}

void RadioLookAndFeel::drawPopupMenuBackground (Graphics& g, int width, int height)
{
    const auto area = Rectangle<float> (0.0f, 0.0f, (float) width, (float) height);

    g.setColour (skin::colour::glass);
    g.fillRoundedRectangle (area, 4.0f);

    g.setColour (skin::colour::brass.withAlpha (0.7f));
    g.drawRoundedRectangle (area.reduced (0.5f), 4.0f, 1.0f);
}

} // namespace kc::gui
