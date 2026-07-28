/*
    Kilocycle - retro broadcast console channel strip
    Copyright (C) 2026  Kilocycle Audio
    Licensed under the GNU General Public License v3 or later. See LICENSE.
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace kc::gui
{

/** The application icon: the panel's VU meter in a walnut case.

    Vector-drawn like everything else, from the same palette and the same
    primitives, so the icon and the panel cannot drift apart. Detail is dropped as
    the icon gets smaller - lettering, screws and the finer graduations only
    appear once there are enough pixels to carry them - which is what keeps the
    16 px version a readable silhouette instead of a smudge.

    Draws into any square area; a non-square one is centred on its shorter side.
*/
void paintIcon (juce::Graphics&, juce::Rectangle<float> area);

/** Renders the icon to a transparent ARGB image. Needs no window. */
juce::Image renderIcon (int sizeInPixels);

} // namespace kc::gui
