/*
    Kilocycle - retro broadcast console channel strip
    Copyright (C) 2026  Kilocycle Audio
    Licensed under the GNU General Public License v3 or later. See LICENSE.
*/

#include "ValveGlow.h"

namespace kc::gui
{

using juce::Colour;
using juce::ColourGradient;
using juce::Graphics;
using juce::Path;
using juce::Rectangle;

ValveGlow::ValveGlow (juce::RangedAudioParameter& valveParameter,
                      std::function<float()> heatProvider)
    : parameter (valveParameter), provider (std::move (heatProvider))
{
    attachment = std::make_unique<juce::ParameterAttachment> (
        parameter,
        [this] (float newValue)
        {
            modelIndex = juce::jlimit (0, dsp::numValveModels - 1, juce::roundToInt (newValue));
            setTooltip (juce::String (model().code) + skin::u8 (" \xe2\x80\x93 ") + model().description
                        + ".  Click to fit a different valve.");
            repaint();
        });

    attachment->sendInitialUpdate();

    heat = provider != nullptr ? juce::jlimit (0.0f, 1.0f, provider()) : 0.0f;

    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    startTimerHz (30);
}

ValveGlow::~ValveGlow() = default;

void ValveGlow::mouseEnter (const juce::MouseEvent&) { hovered = true;  repaint(); }
void ValveGlow::mouseExit  (const juce::MouseEvent&) { hovered = false; repaint(); }

void ValveGlow::mouseDown (const juce::MouseEvent&)
{
    showValveMenu();
}

void ValveGlow::showValveMenu()
{
    juce::PopupMenu menu;
    menu.addSectionHeader ("Fit a valve");

    for (int i = 0; i < dsp::numValveModels; ++i)
    {
        const auto& m = dsp::valveModel (i);

        juce::PopupMenu::Item item;
        item.itemID = i + 1;
        item.text = juce::String (m.code) + "   -   " + m.description;
        item.isTicked = i == modelIndex;
        menu.addItem (item);
    }

    menu.showMenuAsync (juce::PopupMenu::Options()
                            .withTargetComponent (this)
                            .withMinimumWidth (260),
                        [this] (int result)
                        {
                            if (result > 0)
                                attachment->setValueAsCompleteGesture ((float) (result - 1));
                        });
}

void ValveGlow::timerCallback()
{
    const auto target = provider != nullptr ? juce::jlimit (0.0f, 1.0f, provider()) : 0.0f;

    // Heaters have thermal inertia: rising is quicker than cooling.
    heat += (target > heat ? 0.12f : 0.045f) * (target - heat);

    phase += 0.033f;
    flicker = 1.0f + 0.035f * std::sin (phase * 2.3f)
                   + 0.022f * std::sin (phase * 5.7f + 1.3f)
                   + 0.014f * std::sin (phase * 11.1f + 2.7f);

    repaint();
}

void ValveGlow::paint (Graphics& g)
{
    const auto& valve = model();
    const auto isOctal = valve.bottle == dsp::Bottle::octal;

    auto area = getLocalBounds().toFloat();

    const auto legend = area.removeFromBottom (12.0f);

    // An octal has a deep bakelite base and a fat bottle; a miniature is all glass
    // and slim, with the pins straight out of the bottom.
    auto base = area.removeFromBottom (area.getHeight() * (isOctal ? 0.26f : 0.13f));
    const auto env = area.reduced (area.getWidth() * (isOctal ? 0.07f : 0.19f), 0.0f)
                         .withTrimmedTop (isOctal ? 6.0f : 2.0f);

    const auto lit = juce::jlimit (0.0f, 1.0f, heat * flicker);

    // ---- halo -------------------------------------------------------------
    if (lit > 0.01f)
    {
        const auto centre = juce::Point<float> (env.getCentreX(), env.getCentreY() + env.getHeight() * 0.12f);
        const auto r = env.getHeight() * 0.95f;

        ColourGradient halo (skin::colour::amber.withAlpha (0.34f * lit), centre.x, centre.y,
                             juce::Colours::transparentBlack, centre.x, centre.y + r, true);
        halo.addColour (0.45, skin::colour::amberDeep.withAlpha (0.16f * lit));
        g.setGradientFill (halo);
        g.fillEllipse (Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (centre));
    }

    // ---- glass envelope ---------------------------------------------------
    Path glass;
    {
        const auto r = env.getWidth() * 0.5f;
        const auto shoulder = r * (isOctal ? 0.80f : 1.00f);

        glass.startNewSubPath (env.getX(), env.getBottom());
        glass.lineTo (env.getX(), env.getY() + shoulder);
        glass.quadraticTo (env.getX(), env.getY(), env.getCentreX(), env.getY());
        glass.quadraticTo (env.getRight(), env.getY(), env.getRight(), env.getY() + shoulder);
        glass.lineTo (env.getRight(), env.getBottom());
        glass.closeSubPath();
    }

    g.setGradientFill (ColourGradient (Colour (0xff3a332c), env.getX(), env.getY(),
                                       Colour (0xff14100c), env.getRight(), env.getBottom(), false));
    g.fillPath (glass);

    {
        Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (glass);

        // ---- internal structure -------------------------------------------
        const auto plate = env.reduced (env.getWidth() * 0.22f, env.getHeight() * 0.26f);
        const auto barW = plate.getWidth() * (valve.electrodes >= 3 ? 0.26f : 0.36f);

        g.setColour (Colour (0xff2a2622));
        g.fillRect (plate.withWidth (barW));
        g.fillRect (plate.withWidth (barW).withX (plate.getRight() - barW));

        // A pentode's extra grid reads as a third, narrower element.
        if (valve.electrodes >= 3)
        {
            g.setColour (Colour (0xff221f1b));
            g.fillRect (plate.withWidth (barW * 0.5f)
                             .withX (plate.getCentreX() - barW * 0.25f)
                             .reduced (0.0f, plate.getHeight() * 0.14f));
        }

        // Mica spacers.
        g.setColour (Colour (0xff574c3e).withAlpha (0.8f));
        g.fillRect (plate.withHeight (2.4f).withY (plate.getY() - 3.0f));
        g.fillRect (plate.withHeight (2.4f).withY (plate.getBottom() + 1.0f));

        // ---- heater -------------------------------------------------------
        if (lit > 0.005f)
        {
            Path filament;
            const auto x0 = plate.getCentreX();
            const auto top = plate.getY() + 2.0f;
            const auto bottom = plate.getBottom() - 2.0f;
            const auto amp = plate.getWidth() * (valve.electrodes >= 3 ? 0.07f : 0.11f);
            const auto zigs = valve.electrodes >= 3 ? 6 : 5;

            filament.startNewSubPath (x0, top);

            for (int i = 1; i <= zigs; ++i)
            {
                const auto y = top + (bottom - top) * (float) i / (float) zigs;
                filament.lineTo (x0 + (i % 2 == 0 ? -amp : amp), y);
            }

            g.setColour (skin::colour::amber.withAlpha (0.30f * lit));
            g.strokePath (filament, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved,
                                                          juce::PathStrokeType::rounded));

            g.setColour (Colour (0xffff8a2a).withMultipliedAlpha (juce::jlimit (0.0f, 1.0f, lit * 1.3f)));
            g.strokePath (filament, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved,
                                                          juce::PathStrokeType::rounded));

            g.setColour (Colour (0xffffe0a8).withMultipliedAlpha (juce::jlimit (0.0f, 1.0f, lit * lit * 1.5f)));
            g.strokePath (filament, juce::PathStrokeType (1.1f, juce::PathStrokeType::curved,
                                                          juce::PathStrokeType::rounded));

            // Warm bloom filling the bottle.
            ColourGradient bloom (skin::colour::amber.withAlpha (0.28f * lit),
                                  plate.getCentreX(), plate.getCentreY(),
                                  juce::Colours::transparentBlack,
                                  plate.getCentreX(), plate.getCentreY() + env.getHeight() * 0.55f, true);
            g.setGradientFill (bloom);
            g.fillRect (env);
        }

        // ---- glass highlights --------------------------------------------
        g.setGradientFill (ColourGradient (juce::Colours::white.withAlpha (0.20f),
                                           env.getX() + env.getWidth() * 0.22f, env.getY(),
                                           juce::Colours::transparentWhite,
                                           env.getX() + env.getWidth() * 0.45f, env.getY(), false));
        g.fillRect (env);

        g.setColour (juce::Colours::white.withAlpha (0.16f));
        g.fillRoundedRectangle (env.getX() + env.getWidth() * 0.17f, env.getY() + env.getHeight() * 0.13f,
                                juce::jmax (1.2f, env.getWidth() * 0.045f), env.getHeight() * 0.62f, 1.0f);
    }

    g.setColour (hovered ? skin::colour::amberLite.withAlpha (0.55f)
                         : Colour (0xff0d0a07).withAlpha (0.8f));
    g.strokePath (glass, juce::PathStrokeType (hovered ? 1.4f : 1.1f));

    // ---- base -------------------------------------------------------------
    {
        base = isOctal ? base.reduced (base.getWidth() * 0.05f, 0.0f)
                       : base.reduced (base.getWidth() * 0.20f, 0.0f);

        Path basePath;
        basePath.addRoundedRectangle (base.getX(), base.getY(), base.getWidth(),
                                      base.getHeight(), 3.0f, 3.0f, false, false, true, true);

        if (isOctal)
        {
            g.setGradientFill (ColourGradient (Colour (0xff3d2a1c), base.getX(), base.getY(),
                                               Colour (0xff140e09), base.getRight(), base.getBottom(), false));
            g.fillPath (basePath);

            g.setColour (juce::Colours::white.withAlpha (0.10f));
            g.drawLine (base.getX() + 1.0f, base.getY() + 1.0f, base.getRight() - 1.0f, base.getY() + 1.0f, 1.0f);

            // Locating key in the middle of the base.
            g.setColour (Colour (0xff0c0806));
            g.fillRoundedRectangle (base.withSizeKeepingCentre (base.getWidth() * 0.16f,
                                                                base.getHeight() * 0.62f), 1.5f);
        }
        else
        {
            // Miniature valves have only a pinch of glass here.
            g.setGradientFill (ColourGradient (Colour (0xff2a241d), base.getX(), base.getY(),
                                               Colour (0xff100c09), base.getRight(), base.getBottom(), false));
            g.fillPath (basePath);
        }

        // Pins.
        const auto pinW = juce::jmax (1.1f, base.getWidth() * (isOctal ? 0.055f : 0.07f));

        for (int i = 0; i < valve.pins; ++i)
        {
            const auto t = (float) (i + 1) / (float) (valve.pins + 1);
            const auto x = base.getX() + base.getWidth() * t;

            g.setColour (skin::colour::brass.withAlpha (0.85f));
            g.fillRect (x - pinW * 0.5f, base.getBottom() - 1.0f, pinW, isOctal ? 3.5f : 3.0f);
        }
    }

    // ---- type number, on a little plate you can click ---------------------
    {
        const auto font = skin::legendFont (8.5f, true);
        const auto width = juce::GlyphArrangement::getStringWidth (font, valve.code) + 22.0f;
        const auto plate = Rectangle<float> (juce::jmin (legend.getWidth(), width), 11.5f)
                               .withCentre (legend.getCentre());

        skin::paintBrassPlate (g, plate, 2.0f);

        if (hovered)
        {
            g.setColour (skin::colour::amberLite.withAlpha (0.35f));
            g.fillRoundedRectangle (plate, 2.0f);
        }

        g.setColour (skin::colour::brassDark);
        skin::drawTracked (g, valve.code, font, plate.withTrimmedRight (7.0f), 1.2f,
                           juce::Justification::centred);

        // A small chevron, so it looks like something you can open.
        Path chevron;
        const auto cx = plate.getRight() - 5.5f;
        const auto cy = plate.getCentreY();
        chevron.addTriangle (cx - 2.6f, cy - 1.2f, cx + 2.6f, cy - 1.2f, cx, cy + 2.0f);
        g.fillPath (chevron);
    }
}

} // namespace kc::gui
