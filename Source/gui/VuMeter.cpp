/*
    Kilocycle - retro broadcast console channel strip
    Copyright (C) 2026  Kilocycle Audio
    Licensed under the GNU General Public License v3 or later. See LICENSE.
*/

#include "VuMeter.h"

namespace kc::gui
{

using juce::Colour;
using juce::ColourGradient;
using juce::Graphics;
using juce::Path;
using juce::Rectangle;

namespace
{
    /** The classic VU scale is not linear in dB - it is cramped at the bottom and
        stretched around 0. These are the printed graduations of a standard
        movement, with the fraction of the arc each one sits at. */
    struct Graduation { float db; float fraction; const char* label; bool major; };

    const Graduation graduations[]
    {
        { -20.0f, 0.000f, "20", true  },
        { -15.0f, 0.150f, nullptr, false },
        { -10.0f, 0.290f, "10", true  },
        {  -7.0f, 0.400f, "7",  false },
        {  -5.0f, 0.492f, "5",  true  },
        {  -3.0f, 0.598f, "3",  false },
        {  -2.0f, 0.660f, "2",  false },
        {  -1.0f, 0.722f, "1",  false },
        {   0.0f, 0.785f, "0",  true  },
        {   1.0f, 0.858f, nullptr, false },
        {   2.0f, 0.930f, "2",  false },
        {   3.0f, 1.000f, "3",  true  },
    };

    constexpr int numGraduations = (int) (sizeof (graduations) / sizeof (Graduation));
    constexpr float kZeroFraction = 0.785f;
} // namespace

VuMeter::VuMeter (std::function<float()> levelProvider)
    : provider (std::move (levelProvider))
{
    setInterceptsMouseClicks (false, false);

    // Start where the signal already is, rather than sweeping up from the pin.
    angle = angleForFraction (fractionForDb (provider != nullptr ? provider() : -30.0f));

    startTimerHz (50);
}

VuMeter::~VuMeter() = default;

void VuMeter::setCaption (juce::String makerText)
{
    caption = std::move (makerText);
    faceCache = {};
    repaint();
}

float VuMeter::fractionForDb (float db)
{
    if (db <= graduations[0].db)
        return juce::jmax (-0.06f, (db - graduations[0].db) * 0.012f);

    for (int i = 1; i < numGraduations; ++i)
    {
        if (db <= graduations[i].db)
        {
            const auto& a = graduations[i - 1];
            const auto& b = graduations[i];
            const auto t = (db - a.db) / (b.db - a.db);
            return a.fraction + t * (b.fraction - a.fraction);
        }
    }

    return juce::jmin (1.06f, 1.0f + (db - 3.0f) * 0.02f);
}

float VuMeter::angleForFraction (float fraction) const
{
    return -maxAngle + fraction * 2.0f * maxAngle;
}

juce::Point<float> VuMeter::pointAt (float a, float r) const
{
    return pivot.translated (std::sin (a) * r, -std::cos (a) * r);
}

void VuMeter::resized()
{
    const auto face = getLocalBounds().toFloat().reduced (4.0f);

    // The pivot sits just below the bottom edge, so the needle vanishes behind the
    // panel exactly where a real movement's bearing would be.
    pivot  = { face.getCentreX(), face.getBottom() + face.getHeight() * 0.10f };
    radius = pivot.y - face.getY() - face.getHeight() * 0.15f;

    // Fit the sweep to the available width.
    const auto halfWidth = face.getWidth() * 0.44f;
    maxAngle = juce::jlimit (0.40f, 0.85f, std::asin (juce::jmin (0.95f, halfWidth / radius)));

    faceCache = {};
}

void VuMeter::timerCallback()
{
    const auto db = provider != nullptr ? provider() : -30.0f;
    const auto target = angleForFraction (fractionForDb (db));

    // Lightly damped second-order movement: zeta ~ 0.7, so a small overshoot.
    constexpr float dt = 0.02f;
    constexpr float stiffness = 400.0f;
    constexpr float damping   = 28.0f;

    const auto accel = (target - angle) * stiffness - velocity * damping;
    velocity += accel * dt;
    angle    += velocity * dt;

    if (std::abs (velocity) > 1.0e-4f || std::abs (target - angle) > 1.0e-4f)
        repaint();
}

void VuMeter::rebuildFaceCache()
{
    constexpr int supersample = 2;
    const auto w = juce::jmax (1, getWidth()  * supersample);
    const auto h = juce::jmax (1, getHeight() * supersample);

    faceCache = juce::Image (juce::Image::ARGB, w, h, true);

    Graphics ig (faceCache);
    ig.addTransform (juce::AffineTransform::scale ((float) supersample));
    paintFace (ig);
}

void VuMeter::paintFace (Graphics& g) const
{
    const auto outer = getLocalBounds().toFloat();
    const auto face  = outer.reduced (4.0f);

    // ---- enamel dial face -------------------------------------------------
    Path facePath;
    facePath.addRoundedRectangle (face, 4.0f);

    g.setGradientFill (ColourGradient (skin::colour::vuFace.brighter (0.10f), face.getCentreX(), face.getY(),
                                       skin::colour::vuFace.darker (0.10f),  face.getCentreX(), face.getBottom(), false));
    g.fillPath (facePath);

    {
        Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (facePath);

        // Warm lamp behind the dial.
        ColourGradient lamp (skin::colour::amberLite.withAlpha (0.55f),
                             face.getCentreX(), face.getBottom(),
                             juce::Colours::transparentBlack,
                             face.getCentreX(), face.getY() - face.getHeight() * 0.2f, true);
        g.setGradientFill (lamp);
        g.fillRect (face);

        skin::applyGrain (g, face, 0.05f);
    }

    // ---- scale arc --------------------------------------------------------
    const auto arcR = radius * 0.955f;

    auto buildArc = [this, arcR] (float fromFraction, float toFraction)
    {
        Path p;
        const auto steps = 48;

        for (int i = 0; i <= steps; ++i)
        {
            const auto f = fromFraction + (toFraction - fromFraction) * (float) i / (float) steps;
            const auto pt = pointAt (angleForFraction (f), arcR);

            if (i == 0)
                p.startNewSubPath (pt);
            else
                p.lineTo (pt);
        }

        return p;
    };

    g.setColour (skin::colour::ink);
    g.strokePath (buildArc (0.0f, kZeroFraction), juce::PathStrokeType (1.4f));

    g.setColour (skin::colour::redZone);
    g.strokePath (buildArc (kZeroFraction, 1.0f), juce::PathStrokeType (2.6f));

    // ---- graduations ------------------------------------------------------
    const auto labelFont = skin::serifFont (juce::jmax (8.0f, radius * 0.075f), true);

    for (const auto& grad : graduations)
    {
        const auto a = angleForFraction (grad.fraction);
        const auto inRed = grad.db > 0.0f;

        const auto r1 = arcR;
        const auto r2 = arcR - radius * (grad.major ? 0.085f : 0.055f);

        const auto p1 = pointAt (a, r1);
        const auto p2 = pointAt (a, r2);

        g.setColour (inRed ? skin::colour::redZone : skin::colour::ink);
        g.drawLine (p1.x, p1.y, p2.x, p2.y, grad.major ? 1.9f : 1.1f);

        if (grad.label != nullptr)
        {
            const auto lp = pointAt (a, arcR - radius * 0.155f);
            const auto box = Rectangle<float> (26.0f, 14.0f).withCentre (lp);

            g.setFont (labelFont);
            g.setColour (inRed ? skin::colour::redZone : skin::colour::ink);
            g.drawText (grad.label, box, juce::Justification::centred, false);
        }
    }

    // "-" and "+" markers at either end of the scale.
    {
        const auto font = skin::serifFont (juce::jmax (9.0f, radius * 0.085f), true);
        g.setFont (font);
        g.setColour (skin::colour::ink);
        g.drawText ("-", Rectangle<float> (16.0f, 14.0f)
                             .withCentre (pointAt (angleForFraction (0.055f), arcR - radius * 0.26f)),
                    juce::Justification::centred, false);
        g.setColour (skin::colour::redZone);
        g.drawText ("+", Rectangle<float> (16.0f, 14.0f)
                             .withCentre (pointAt (angleForFraction (0.945f), arcR - radius * 0.26f)),
                    juce::Justification::centred, false);
    }

    // ---- legends ----------------------------------------------------------
    // Both corners below the ends of the sweep stay clear of the needle, so that
    // is where the printing goes - exactly as on the real thing.
    {
        g.setColour (skin::colour::ink);
        skin::drawTracked (g, "VU", skin::serifFont (juce::jmax (12.0f, face.getHeight() * 0.185f), true),
                           Rectangle<float> (face.getWidth() * 0.3f, face.getHeight() * 0.2f)
                               .withPosition (face.getX() + 8.0f,
                                              face.getBottom() - face.getHeight() * 0.26f),
                           2.5f, juce::Justification::left);

        g.setColour (skin::colour::inkSoft.withAlpha (0.85f));
        skin::drawTracked (g, caption, skin::legendFont (juce::jmax (6.5f, face.getHeight() * 0.058f)),
                           Rectangle<float> (face.getWidth() * 0.55f, 10.0f)
                               .withRightX (face.getRight() - 7.0f)
                               .withY (face.getBottom() - face.getHeight() * 0.15f),
                           1.0f, juce::Justification::right);
    }
}

void VuMeter::paint (Graphics& g)
{
    const auto outer = getLocalBounds().toFloat();
    const auto face  = outer.reduced (4.0f);

    skin::paintRecessedWindow (g, face, 4.0f);

    if (faceCache.isNull())
        rebuildFaceCache();

    g.drawImage (faceCache, outer, juce::RectanglePlacement::stretchToFit);

    // ---- needle -----------------------------------------------------------
    {
        Graphics::ScopedSaveState save (g);

        Path clip;
        clip.addRoundedRectangle (face, 4.0f);
        g.reduceClipRegion (clip);

        const auto tip  = pointAt (angle, radius * 0.985f);
        const auto tail = pointAt (angle, -radius * 0.06f);

        const auto perp = juce::Point<float> (std::cos (angle), std::sin (angle));
        const auto wBase = juce::jmax (1.1f, radius * 0.013f);
        const auto wTip  = juce::jmax (0.55f, radius * 0.005f);

        Path needle;
        needle.startNewSubPath (tail + perp * wBase);
        needle.lineTo (tip + perp * wTip);
        needle.lineTo (tip - perp * wTip);
        needle.lineTo (tail - perp * wBase);
        needle.closeSubPath();

        g.setColour (juce::Colours::black.withAlpha (0.22f));
        g.fillPath (needle, juce::AffineTransform::translation (2.0f, 2.5f));

        g.setColour (Colour (0xff241a12));
        g.fillPath (needle);

        // Hub where the needle disappears behind the panel.
        const auto hubR = radius * 0.07f;
        const auto hub  = Rectangle<float> (hubR * 2.0f, hubR * 2.0f)
                              .withCentre ({ pivot.x, face.getBottom() });

        ColourGradient hubGrad (skin::colour::brassLite, hub.getX(), hub.getY(),
                                skin::colour::brassDark, hub.getRight(), hub.getBottom(), false);
        g.setGradientFill (hubGrad);
        g.fillEllipse (hub);
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.drawEllipse (hub, 0.8f);
    }

    skin::paintGlassSheen (g, face, 4.0f);
}

} // namespace kc::gui
