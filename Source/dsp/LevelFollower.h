/*
    Kilocycle - retro broadcast console channel strip
    Copyright (C) 2026  Kilocycle Audio
    Licensed under the GNU General Public License v3 or later. See LICENSE.
*/

#pragma once

#include <cmath>
#include <algorithm>

namespace kc::dsp
{

/** Average-responding meter with classic VU ballistics.

    A real VU movement is a mechanical integrator: roughly 300 ms to reach 99 %
    of a step, and the same coming back down. That slow, symmetric response is
    the whole reason VU needles look alive rather than twitchy, so it is worth
    modelling properly instead of using a peak meter with a decay.

    0 VU is referenced to -18 dBFS, the usual digital alignment level.
*/
class VuFollower
{
public:
    static constexpr float kZeroVuDbfs = -18.0f;

    void prepare (double sampleRate)
    {
        // 300 ms to ~99 % => time constant of roughly 300/4.6 ms.
        coeff = static_cast<float> (1.0 - std::exp (-1.0 / (0.065 * sampleRate)));
        reset();
    }

    void reset() { average = 0.0f; }

    inline void push (float magnitude) noexcept
    {
        average += coeff * (magnitude - average);
    }

    /** Level in dB relative to 0 VU (i.e. 0.0 means the needle sits on 0 VU). */
    float getVuDb() const noexcept
    {
        constexpr float meanToRms = 1.11f;
        const auto dbfs = 20.0f * std::log10 (std::max (1.0e-6f, average * meanToRms));
        return dbfs - kZeroVuDbfs;
    }

private:
    float coeff   = 0.001f;
    float average = 0.0f;
};

} // namespace kc::dsp
