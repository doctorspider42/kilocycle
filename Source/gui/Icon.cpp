/*
    Kilocycle - retro broadcast console channel strip
    Copyright (C) 2026  Kilocycle Audio
    Licensed under the GNU General Public License v3 or later. See LICENSE.
*/

#include "Icon.h"
#include "Skin.h"

namespace kc::gui
{

using juce::Colour;
using juce::ColourGradient;
using juce::Graphics;
using juce::Path;
using juce::Point;
using juce::Rectangle;

namespace
{
    /** The printed graduations of a standard VU movement, as a fraction of the
        sweep. Same numbers as the panel meter; only the majors survive on the
        small icon. */
    struct Graduation { float fraction; bool major; const char* label; };

    const Graduation graduations[]
    {
        { 0.000f, true,  "20" },
        { 0.150f, false, nullptr },
        { 0.290f, true,  "10" },
        { 0.400f, false, nullptr },
        { 0.492f, true,  "5"  },
        { 0.598f, false, nullptr },
        { 0.660f, false, nullptr },
        { 0.722f, false, nullptr },
        { 0.785f, true,  "0"  },
        { 0.858f, false, nullptr },
        { 0.930f, false, nullptr },
        { 1.000f, true,  "3"  },
    };

    constexpr float kZeroFraction = 0.785f;

    /** Where the needle is posed: a whisker under 0 VU, so it sits just short of
        the red without ever reading as an overload. */
    constexpr float kNeedleFraction = 0.715f;

    Path roundedPath (Rectangle<float> area, float corner)
    {
        Path p;
        p.addRoundedRectangle (area, corner);
        return p;
    }

    /** Walnut, with grain spaced by icon size rather than in pixels - the panel's
        version is tuned for one scale, and this has to survive 16 px to 1024. */
    void paintCase (Graphics& g, Rectangle<float> body, float corner, float s)
    {
        const auto path = roundedPath (body, corner);

        g.setGradientFill (ColourGradient (skin::colour::walnutMid,  body.getCentreX(), body.getY(),
                                           skin::colour::walnutDeep, body.getCentreX(), body.getBottom(), false));
        g.fillPath (path);

        {
            Graphics::ScopedSaveState save (g);
            g.reduceClipRegion (path);

            juce::Random random (0xc0ffee);
            const auto step = juce::jmax (1.0f, s / 46.0f);

            for (auto y = body.getY(); y < body.getBottom(); y += step)
            {
                Path line;
                const auto amp   = step * (0.3f + random.nextFloat() * 1.1f);
                const auto phase = random.nextFloat() * juce::MathConstants<float>::twoPi;
                const auto freq  = (4.0f + random.nextFloat() * 6.0f) / s;

                line.startNewSubPath (body.getX(), y + std::sin (phase) * amp);

                for (auto x = body.getX(); x <= body.getRight(); x += step)
                    line.lineTo (x, y + std::sin (phase + x * freq) * amp);

                g.setColour ((random.nextBool() ? skin::colour::walnutLite : skin::colour::walnutDeep)
                                 .withAlpha (0.06f + random.nextFloat() * 0.10f));
                g.strokePath (line, juce::PathStrokeType (step * (0.15f + random.nextFloat() * 0.35f)));
            }

            // Lacquer: the case catches the light along its top edge.
            g.setGradientFill (ColourGradient (juce::Colours::white.withAlpha (0.13f),
                                               body.getCentreX(), body.getY(),
                                               juce::Colours::transparentWhite,
                                               body.getCentreX(), body.getCentreY(), false));
            g.fillRect (body);

            skin::applyGrain (g, body, 0.05f);
        }

        g.setColour (skin::colour::walnutLite.withAlpha (0.45f));
        g.strokePath (roundedPath (body.reduced (s * 0.006f), corner - s * 0.006f),
                      juce::PathStrokeType (s * 0.006f));

        g.setColour (skin::colour::walnutDeep);
        g.strokePath (path, juce::PathStrokeType (s * 0.008f));
    }
} // namespace

void paintIcon (Graphics& g, Rectangle<float> area)
{
    const auto side = juce::jmin (area.getWidth(), area.getHeight());
    const auto body = Rectangle<float> (side, side).withCentre (area.getCentre())
                          .reduced (side * 0.014f);
    const auto s = body.getWidth();

    // Level of detail. Every threshold is the point below which a feature stops
    // being a feature and starts being dirt on the glass.
    const auto showMinorTicks = s >= 64.0f;
    const auto showVuLegend   = s >= 96.0f;
    const auto showNumerals   = s >= 300.0f;
    const auto showMaker      = s >= 420.0f;

    paintCase (g, body, s * 0.215f, s);

    // ---- the dial, inset concentrically into the case ---------------------
    const auto face       = body.reduced (s * 0.085f);
    const auto faceCorner = s * 0.13f;
    const auto facePath   = roundedPath (face, faceCorner);

    {
        juce::DropShadow shadow (juce::Colours::black.withAlpha (0.7f),
                                 juce::roundToInt (juce::jmax (2.0f, s * 0.05f)),
                                 { 0, juce::roundToInt (juce::jmax (1.0f, s * 0.012f)) });
        shadow.drawForPath (g, facePath);
    }

    g.setGradientFill (ColourGradient (skin::colour::vuFace.brighter (0.10f), face.getCentreX(), face.getY(),
                                       skin::colour::vuFace.darker (0.12f),   face.getCentreX(), face.getBottom(), false));
    g.fillPath (facePath);

    // The movement: pivot just below the face, exactly where a real bearing sits.
    // Kept close to the bottom edge on purpose - a longer arm would give a truer
    // sweep radius but a narrower fan, and the fan is what carries the icon.
    const auto pivot    = Point<float> (face.getCentreX(), face.getBottom() + face.getHeight() * 0.02f);
    const auto radius   = pivot.y - face.getY() - face.getHeight() * 0.11f;
    const auto maxAngle = juce::jlimit (0.34f, 0.85f,
                                        std::asin (juce::jmin (0.95f, face.getWidth() * 0.45f / radius)));

    auto angleFor = [maxAngle] (float fraction) { return -maxAngle + fraction * 2.0f * maxAngle; };
    auto pointAt  = [pivot]   (float angle, float r)
    {
        return pivot.translated (std::sin (angle) * r, -std::cos (angle) * r);
    };

    {
        Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (facePath);

        // Warm lamp behind the dial, brightest at the bearing.
        ColourGradient lamp (skin::colour::amberLite.withAlpha (0.6f),
                             pivot.x, face.getBottom(),
                             juce::Colours::transparentBlack,
                             pivot.x, face.getY() - face.getHeight() * 0.25f, true);
        g.setGradientFill (lamp);
        g.fillRect (face);

        skin::applyGrain (g, face, 0.045f);
    }

    // ---- scale ------------------------------------------------------------
    const auto arcR = radius * 0.95f;

    auto buildArc = [&] (float from, float to)
    {
        Path p;
        constexpr int steps = 64;

        for (int i = 0; i <= steps; ++i)
        {
            const auto pt = pointAt (angleFor (from + (to - from) * (float) i / (float) steps), arcR);

            if (i == 0)
                p.startNewSubPath (pt);
            else
                p.lineTo (pt);
        }

        return p;
    };

    // Everything on the dial has a floor in whole pixels: at 32 px a stroke of
    // s * 0.008 is a quarter of a pixel, which the rasteriser turns into a grey
    // suggestion rather than a line.
    g.setColour (skin::colour::ink);
    g.strokePath (buildArc (0.0f, kZeroFraction), juce::PathStrokeType (juce::jmax (1.0f, s * 0.008f)));

    g.setColour (skin::colour::redZone);
    g.strokePath (buildArc (kZeroFraction, 1.0f), juce::PathStrokeType (juce::jmax (1.7f, s * 0.017f)));

    for (const auto& grad : graduations)
    {
        if (! grad.major && ! showMinorTicks)
            continue;

        const auto angle = angleFor (grad.fraction);
        const auto inRed = grad.fraction > kZeroFraction;
        const auto p1    = pointAt (angle, arcR);
        const auto p2    = pointAt (angle, arcR - radius * (grad.major ? 0.10f : 0.062f));

        g.setColour (inRed ? skin::colour::redZone : skin::colour::ink);
        g.drawLine (p1.x, p1.y, p2.x, p2.y,
                    grad.major ? juce::jmax (1.2f, s * 0.012f) : juce::jmax (0.8f, s * 0.006f));

        if (showNumerals && grad.label != nullptr)
        {
            const auto centre = pointAt (angle, arcR - radius * 0.145f);
            const auto box    = Rectangle<float> (radius * 0.24f, radius * 0.16f).withCentre (centre);

            g.setFont (skin::serifFont (radius * 0.09f, true));
            g.setColour (inRed ? skin::colour::redZone : skin::colour::ink);
            g.drawText (grad.label, box, juce::Justification::centred, false);
        }
    }

    // ---- legends ----------------------------------------------------------
    if (showVuLegend)
    {
        g.setColour (skin::colour::ink);
        skin::drawTracked (g, "VU", skin::serifFont (face.getHeight() * 0.17f, true),
                           Rectangle<float> (face.getWidth() * 0.4f, face.getHeight() * 0.2f)
                               .withPosition (face.getX() + face.getWidth() * 0.075f,
                                              face.getBottom() - face.getHeight() * 0.27f),
                           face.getWidth() * 0.022f, juce::Justification::left);
    }

    if (showMaker)
    {
        g.setColour (skin::colour::inkSoft.withAlpha (0.8f));
        skin::drawTracked (g, "KILOCYCLE", skin::legendFont (face.getHeight() * 0.05f),
                           Rectangle<float> (face.getWidth() * 0.6f, face.getHeight() * 0.08f)
                               .withRightX (face.getRight() - face.getWidth() * 0.075f)
                               .withY (face.getBottom() - face.getHeight() * 0.2f),
                           face.getWidth() * 0.012f, juce::Justification::right);
    }

    // ---- needle -----------------------------------------------------------
    {
        Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (facePath);

        const auto angle = angleFor (kNeedleFraction);
        const auto tip   = pointAt (angle,  radius * 0.985f);
        const auto tail  = pointAt (angle, -radius * 0.06f);
        const auto perp  = Point<float> (std::cos (angle), std::sin (angle));

        const auto wBase = juce::jmax (1.3f, s * 0.011f);
        const auto wTip  = juce::jmax (0.6f, s * 0.0035f);

        Path needle;
        needle.startNewSubPath (tail + perp * wBase);
        needle.lineTo (tip + perp * wTip);
        needle.lineTo (tip - perp * wTip);
        needle.lineTo (tail - perp * wBase);
        needle.closeSubPath();

        // Barely-there shadow: the needle floats a millimetre above the dial, and
        // anything heavier here reads as a second needle rather than a cast one.
        g.setColour (juce::Colours::black.withAlpha (0.13f));
        g.fillPath (needle, juce::AffineTransform::translation (s * 0.004f, s * 0.005f));

        g.setColour (Colour (0xff241a12));
        g.fillPath (needle);

        // Bearing, half swallowed by the bottom edge of the face.
        const auto hub = Rectangle<float> (radius * 0.16f, radius * 0.16f)
                             .withCentre ({ pivot.x, face.getBottom() });

        g.setGradientFill (ColourGradient (skin::colour::brassLite, hub.getX(), hub.getY(),
                                           skin::colour::brassDark, hub.getRight(), hub.getBottom(), false));
        g.fillEllipse (hub);
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.drawEllipse (hub, s * 0.004f);
    }

    // ---- brass bezel and glass -------------------------------------------
    {
        g.setColour (skin::colour::brassDark.withAlpha (0.9f));
        g.strokePath (facePath, juce::PathStrokeType (juce::jmax (1.0f, s * 0.012f)));

        ColourGradient rim (skin::colour::brassLite, face.getX(), face.getY(),
                            skin::colour::brass,     face.getRight(), face.getBottom(), false);
        rim.addColour (0.5, skin::colour::brassLite.withMultipliedBrightness (0.75f));
        g.setGradientFill (rim);
        g.strokePath (roundedPath (face.expanded (s * 0.011f), faceCorner + s * 0.011f),
                      juce::PathStrokeType (juce::jmax (1.2f, s * 0.014f)));
    }

    {
        Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (facePath);

        g.setGradientFill (ColourGradient (juce::Colours::white.withAlpha (0.11f),
                                           face.getCentreX(), face.getY(),
                                           juce::Colours::transparentWhite,
                                           face.getCentreX(), face.getY() + face.getHeight() * 0.55f, false));
        g.fillRect (face);
    }
}

juce::Image renderIcon (int sizeInPixels)
{
    const auto side = juce::jmax (16, sizeInPixels);

    juce::Image image (juce::Image::ARGB, side, side, true);
    Graphics g (image);
    g.setImageResamplingQuality (Graphics::highResamplingQuality);

    paintIcon (g, Rectangle<float> (0.0f, 0.0f, (float) side, (float) side));

    return image;
}

} // namespace kc::gui
