/*
    Kilocycle - retro broadcast console channel strip
    Copyright (C) 2026  Kilocycle Audio
    Licensed under the GNU General Public License v3 or later. See LICENSE.
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/** Everything that makes the plug-in look like a 1950s broadcast desk:
    the palette, the fonts and a handful of painting primitives (wood, bakelite,
    brass, cream enamel, engraved lettering, glass).

    All of it is drawn with vectors and one small cached noise tile - there are no
    binary image assets, so the UI stays crisp at any scale and the plug-in stays
    a few hundred kilobytes.
*/
namespace kc::skin
{

namespace colour
{
    inline const juce::Colour walnutDeep  { 0xff1a0f07 };
    inline const juce::Colour walnutDark  { 0xff32200f };
    inline const juce::Colour walnutMid   { 0xff593a1f };
    inline const juce::Colour walnutLite  { 0xff7d5530 };

    inline const juce::Colour cream       { 0xffe4d5b3 };
    inline const juce::Colour creamLite   { 0xfff4e9cf };
    inline const juce::Colour creamDark   { 0xffc4b28b };

    inline const juce::Colour ink         { 0xff31271c };
    inline const juce::Colour inkSoft     { 0xff6d5c46 };

    inline const juce::Colour brass       { 0xffb99247 };
    inline const juce::Colour brassLite   { 0xffedd699 };
    inline const juce::Colour brassDark   { 0xff6b5015 };

    inline const juce::Colour amber       { 0xffff9d2e };
    inline const juce::Colour amberLite   { 0xffffdba6 };
    inline const juce::Colour amberDeep   { 0xff8a4708 };

    inline const juce::Colour glass       { 0xff1d1208 };
    inline const juce::Colour glassLite   { 0xff42280f };

    inline const juce::Colour bakelite    { 0xff1b1714 };
    inline const juce::Colour bakeliteMid { 0xff322b25 };
    inline const juce::Colour bakeliteLite{ 0xff574c40 };

    inline const juce::Colour vuFace      { 0xfff0e2bb };
    inline const juce::Colour redZone     { 0xffb03426 };
    inline const juce::Colour lampOn      { 0xffff6a3d };
    inline const juce::Colour lampOff     { 0xff4a2a20 };
}

/** juce::String treats a plain char* as Latin-1, so anything above ASCII (the
    interpuncts and the +/- sign used on the panel) has to be decoded explicitly. */
inline juce::String u8 (const char* utf8Text) { return juce::String::fromUTF8 (utf8Text); }

/** Condensed sans, used for the small engraved legends. */
juce::Font legendFont (float height, bool bold = false);

/** Serif, used for nameplates and dial numerals - the period-correct choice. */
juce::Font serifFont (float height, bool bold = false);

/** Draws text with extra letter spacing, the way engraved panel lettering looks. */
void drawTracked (juce::Graphics&, const juce::String&, const juce::Font&,
                  juce::Rectangle<float> area, float tracking,
                  juce::Justification justification);

/** Engraved lettering: a light lower edge under dark ink, so the glyphs look
    stamped into the panel rather than printed on it. */
void drawEngraved (juce::Graphics&, const juce::String&, const juce::Font&,
                   juce::Rectangle<float> area, juce::Justification,
                   juce::Colour ink = colour::ink, float tracking = 0.0f);

/** Overlays the cached film-grain tile. */
void applyGrain (juce::Graphics&, juce::Rectangle<float> area, float alpha);

void paintWoodBezel (juce::Graphics&, juce::Rectangle<float> area, float corner);
void paintFaceplate (juce::Graphics&, juce::Rectangle<float> area, float corner);
void paintBrassPlate (juce::Graphics&, juce::Rectangle<float> area, float corner);
void paintScrew (juce::Graphics&, juce::Point<float> centre, float radius, float angle);
void paintGrilleCloth (juce::Graphics&, juce::Rectangle<float> area, float corner);
void paintRecessedWindow (juce::Graphics&, juce::Rectangle<float> area, float corner);
void paintGlassSheen (juce::Graphics&, juce::Rectangle<float> area, float corner);

/** Engraved section divider with a small brass caption plate. */
void paintSectionFrame (juce::Graphics&, juce::Rectangle<float> area, const juce::String& title);

} // namespace kc::skin
