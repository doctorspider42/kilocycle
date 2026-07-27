/*
    Kilocycle - retro broadcast console channel strip
    Copyright (C) 2026  Kilocycle Audio
    Licensed under the GNU General Public License v3 or later. See LICENSE.
*/

#include "Skin.h"

namespace kc::skin
{

using juce::Colour;
using juce::ColourGradient;
using juce::Graphics;
using juce::Path;
using juce::Rectangle;

namespace
{
    constexpr float kPi = 3.14159265358979f;

    /** A small tile of monochrome noise, generated once and reused as a fill.
        Cheaper than per-pixel work on every repaint and gives the panels the
        faint tooth of painted metal. */
    const juce::Image& grainTile()
    {
        static const juce::Image tile = []
        {
            constexpr int size = 128;
            juce::Image image (juce::Image::ARGB, size, size, true);
            juce::Random random (0x9e3779b9);

            const juce::Image::BitmapData data (image, juce::Image::BitmapData::writeOnly);

            for (int y = 0; y < size; ++y)
            {
                for (int x = 0; x < size; ++x)
                {
                    const auto v = (juce::uint8) random.nextInt (256);
                    const auto a = (juce::uint8) (random.nextInt (60) + 20);
                    data.setPixelColour (x, y, Colour (v, v, v).withAlpha (a / 255.0f));
                }
            }

            return image;
        }();

        return tile;
    }

    Path roundedPath (Rectangle<float> area, float corner)
    {
        Path p;
        p.addRoundedRectangle (area, corner);
        return p;
    }
} // namespace

juce::Font legendFont (float height, bool bold)
{
    return juce::Font (juce::FontOptions (juce::Font::getDefaultSansSerifFontName(),
                                          height,
                                          bold ? juce::Font::bold : juce::Font::plain))
               .withHorizontalScale (0.93f);
}

juce::Font serifFont (float height, bool bold)
{
    return juce::Font (juce::FontOptions (juce::Font::getDefaultSerifFontName(),
                                          height,
                                          bold ? juce::Font::bold : juce::Font::plain));
}

void drawTracked (Graphics& g, const juce::String& text, const juce::Font& font,
                  Rectangle<float> area, float tracking, juce::Justification justification)
{
    if (text.isEmpty())
        return;

    g.setFont (font);

    juce::Array<float> widths;
    auto total = 0.0f;

    for (int i = 0; i < text.length(); ++i)
    {
        const auto w = juce::GlyphArrangement::getStringWidth (font, juce::String::charToString (text[i]));
        widths.add (w);
        total += w + tracking;
    }

    total -= tracking;

    auto x = area.getX();

    if (justification.testFlags (juce::Justification::horizontallyCentred))
        x = area.getCentreX() - total * 0.5f;
    else if (justification.testFlags (juce::Justification::right))
        x = area.getRight() - total;

    const auto baseline = justification.testFlags (juce::Justification::top)
                              ? area.getY() + font.getAscent()
                              : area.getCentreY() + font.getAscent() * 0.5f - font.getDescent() * 0.35f;

    for (int i = 0; i < text.length(); ++i)
    {
        g.drawSingleLineText (juce::String::charToString (text[i]),
                              juce::roundToInt (x), juce::roundToInt (baseline));
        x += widths[i] + tracking;
    }
}

void drawEngraved (Graphics& g, const juce::String& text, const juce::Font& font,
                   Rectangle<float> area, juce::Justification justification,
                   Colour ink, float tracking)
{
    // Light rim first, one pixel down: reads as a groove cut into the panel.
    g.setColour (colour::creamLite.withAlpha (0.75f));

    if (tracking > 0.0f)
        drawTracked (g, text, font, area.translated (0.0f, 1.0f), tracking, justification);
    else
    {
        g.setFont (font);
        g.drawText (text, area.translated (0.0f, 1.0f), justification, false);
    }

    g.setColour (ink);

    if (tracking > 0.0f)
        drawTracked (g, text, font, area, tracking, justification);
    else
    {
        g.setFont (font);
        g.drawText (text, area, justification, false);
    }
}

void applyGrain (Graphics& g, Rectangle<float> area, float alpha)
{
    Graphics::ScopedSaveState save (g);
    g.setTiledImageFill (grainTile(), 0, 0, alpha);
    g.fillRect (area);
}

void paintWoodBezel (Graphics& g, Rectangle<float> area, float corner)
{
    const auto path = roundedPath (area, corner);

    g.setGradientFill (ColourGradient (colour::walnutMid,  area.getCentreX(), area.getY(),
                                       colour::walnutDeep, area.getCentreX(), area.getBottom(), false));
    g.fillPath (path);

    // ---- grain ------------------------------------------------------------
    {
        Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (path);

        juce::Random random (0xc0ffee);

        for (auto y = area.getY(); y < area.getBottom(); y += 2.4f)
        {
            Path line;
            const auto amp   = random.nextFloat() * 3.0f + 0.6f;
            const auto phase = random.nextFloat() * kPi * 2.0f;
            const auto freq  = 0.006f + random.nextFloat() * 0.01f;

            line.startNewSubPath (area.getX(), y + std::sin (phase) * amp);

            for (auto x = area.getX(); x <= area.getRight(); x += 12.0f)
                line.lineTo (x, y + std::sin (phase + x * freq) * amp);

            const auto light = random.nextBool();
            g.setColour ((light ? colour::walnutLite : colour::walnutDeep)
                             .withAlpha (0.05f + random.nextFloat() * 0.09f));
            g.strokePath (line, juce::PathStrokeType (random.nextFloat() * 1.4f + 0.4f));
        }

        applyGrain (g, area, 0.05f);
    }

    // ---- edges ------------------------------------------------------------
    g.setColour (colour::walnutLite.withAlpha (0.5f));
    g.strokePath (roundedPath (area.reduced (1.0f), corner - 1.0f), juce::PathStrokeType (1.2f));

    g.setColour (colour::walnutDeep);
    g.strokePath (path, juce::PathStrokeType (1.5f));
}

void paintFaceplate (Graphics& g, Rectangle<float> area, float corner)
{
    const auto path = roundedPath (area, corner);

    // Drop shadow into the bezel.
    {
        juce::DropShadow shadow (juce::Colours::black.withAlpha (0.65f), 14, { 0, 3 });
        shadow.drawForPath (g, path);
    }

    g.setGradientFill (ColourGradient (colour::creamLite, area.getCentreX(), area.getY(),
                                       colour::cream,     area.getCentreX(), area.getBottom(), false));
    g.fillPath (path);

    // Vignette - old enamel is never evenly lit.
    {
        Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (path);

        ColourGradient vignette (Colour (0x00000000), area.getCentreX(), area.getCentreY(),
                                 colour::creamDark.withAlpha (0.55f), area.getCentreX(),
                                 area.getY() - area.getHeight() * 0.15f, true);
        vignette.addColour (0.62, Colour (0x00000000));
        g.setGradientFill (vignette);
        g.fillRect (area);

        applyGrain (g, area, 0.055f);
    }

    g.setColour (colour::creamDark.withAlpha (0.9f));
    g.strokePath (path, juce::PathStrokeType (1.0f));

    g.setColour (juce::Colours::white.withAlpha (0.35f));
    g.strokePath (roundedPath (area.reduced (1.2f), corner - 1.0f), juce::PathStrokeType (1.0f));
}

void paintBrassPlate (Graphics& g, Rectangle<float> area, float corner)
{
    const auto path = roundedPath (area, corner);

    ColourGradient grad (colour::brassLite, area.getX(), area.getY(),
                         colour::brassDark, area.getRight(), area.getBottom(), false);
    grad.addColour (0.35, colour::brass);
    grad.addColour (0.62, colour::brassLite.withMultipliedBrightness (0.92f));
    g.setGradientFill (grad);
    g.fillPath (path);

    {
        Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (path);
        applyGrain (g, area, 0.07f);
    }

    g.setColour (colour::brassDark.withAlpha (0.85f));
    g.strokePath (path, juce::PathStrokeType (0.9f));
}

void paintScrew (Graphics& g, juce::Point<float> centre, float radius, float angle)
{
    const auto bounds = Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre);

    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillEllipse (bounds.translated (0.0f, 1.0f));

    ColourGradient grad (Colour (0xffbdb2a2), bounds.getX(), bounds.getY(),
                         Colour (0xff5d5348), bounds.getRight(), bounds.getBottom(), false);
    g.setGradientFill (grad);
    g.fillEllipse (bounds);

    g.setColour (Colour (0xff3a332b));
    g.drawEllipse (bounds, 0.7f);

    // Slot.
    Path slot;
    slot.addRectangle (-radius * 0.72f, -radius * 0.13f, radius * 1.44f, radius * 0.26f);
    g.setColour (Colour (0xff2b2620));
    g.fillPath (slot, juce::AffineTransform::rotation (angle).translated (centre));

    g.setColour (juce::Colours::white.withAlpha (0.28f));
    g.drawEllipse (bounds.reduced (radius * 0.22f).translated (-0.4f, -0.4f), 0.7f);
}

void paintGrilleCloth (Graphics& g, Rectangle<float> area, float corner)
{
    const auto path = roundedPath (area, corner);

    g.setGradientFill (ColourGradient (Colour (0xff5b4a34), area.getCentreX(), area.getY(),
                                       Colour (0xff2e2419), area.getCentreX(), area.getBottom(), false));
    g.fillPath (path);

    Graphics::ScopedSaveState save (g);
    g.reduceClipRegion (path);

    // A simple diagonal weave.
    g.setColour (Colour (0xff8a7250).withAlpha (0.30f));
    for (auto x = area.getX() - area.getHeight(); x < area.getRight(); x += 3.5f)
        g.drawLine (x, area.getBottom(), x + area.getHeight(), area.getY(), 0.9f);

    g.setColour (Colour (0xff1b140c).withAlpha (0.32f));
    for (auto x = area.getX(); x < area.getRight() + area.getHeight(); x += 3.5f)
        g.drawLine (x, area.getBottom(), x - area.getHeight(), area.getY(), 0.9f);

    applyGrain (g, area, 0.1f);
}

void paintRecessedWindow (Graphics& g, Rectangle<float> area, float corner)
{
    const auto path = roundedPath (area, corner);

    g.setGradientFill (ColourGradient (colour::glass,     area.getCentreX(), area.getY(),
                                       colour::glassLite, area.getCentreX(), area.getBottom(), false));
    g.fillPath (path);

    // Inner shadow along the top edge.
    {
        Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (path);
        g.setGradientFill (ColourGradient (juce::Colours::black.withAlpha (0.75f), area.getCentreX(), area.getY(),
                                           juce::Colours::transparentBlack,        area.getCentreX(), area.getY() + area.getHeight() * 0.4f, false));
        g.fillRect (area);
    }

    // Brass bezel.
    g.setColour (colour::brassDark);
    g.strokePath (path, juce::PathStrokeType (2.6f));

    ColourGradient rim (colour::brassLite, area.getX(), area.getY(),
                        colour::brass,     area.getRight(), area.getBottom(), false);
    g.setGradientFill (rim);
    g.strokePath (roundedPath (area.expanded (1.2f), corner + 1.0f), juce::PathStrokeType (1.6f));
}

void paintGlassSheen (Graphics& g, Rectangle<float> area, float corner)
{
    Graphics::ScopedSaveState save (g);
    g.reduceClipRegion (roundedPath (area, corner));

    // A soft wash across the upper part of the glass. Deliberately edgeless - a
    // hard-edged highlight reads as a printed shape rather than a reflection.
    g.setGradientFill (ColourGradient (juce::Colours::white.withAlpha (0.085f),
                                       area.getCentreX(), area.getY(),
                                       juce::Colours::transparentWhite,
                                       area.getCentreX(), area.getY() + area.getHeight() * 0.55f, false));
    g.fillRect (area);

    g.setColour (juce::Colours::white.withAlpha (0.14f));
    g.drawLine (area.getX() + 3.0f, area.getY() + 2.0f, area.getRight() - 3.0f, area.getY() + 2.0f, 1.0f);
}

void paintSectionFrame (Graphics& g, Rectangle<float> area, const juce::String& title)
{
    const auto corner = 7.0f;
    const auto path   = roundedPath (area, corner);

    g.setColour (colour::creamDark.withAlpha (0.20f));
    g.fillPath (path);

    g.setColour (colour::creamLite.withAlpha (0.85f));
    g.strokePath (roundedPath (area.translated (0.0f, 1.0f), corner), juce::PathStrokeType (1.0f));

    g.setColour (colour::inkSoft.withAlpha (0.55f));
    g.strokePath (path, juce::PathStrokeType (1.0f));

    if (title.isNotEmpty())
    {
        const auto font  = legendFont (11.0f, true);
        const auto width = juce::GlyphArrangement::getStringWidth (font, title) + title.length() * 1.6f + 22.0f;
        const auto plate = Rectangle<float> (width, 16.0f)
                               .withCentre ({ area.getCentreX(), area.getY() });

        // Break the frame line where the caption plate sits.
        g.setColour (colour::cream);
        g.fillRect (plate.expanded (2.0f, 1.0f));

        paintBrassPlate (g, plate, 3.0f);

        g.setColour (colour::brassDark.withAlpha (0.95f));
        drawTracked (g, title, font, plate, 1.6f, juce::Justification::centred);
    }
}

} // namespace kc::skin
