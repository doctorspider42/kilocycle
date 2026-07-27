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

/** Bi-quad coefficient set (RBJ audio EQ cookbook), normalised so that a0 == 1.

    Coefficients are plain values - computing them never allocates, so they can
    safely be refreshed from the audio thread. State lives separately in
    BiquadState, which means one coefficient set can drive N channels.
*/
struct BiquadCoeffs
{
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;

    void setBypass() noexcept
    {
        b0 = 1.0; b1 = b2 = a1 = a2 = 0.0;
    }

    /** 12 dB/oct high-pass. */
    void setHighpass (double sampleRate, double freq, double q) noexcept
    {
        const auto w0    = omega (sampleRate, freq);
        const auto cosw  = std::cos (w0);
        const auto alpha = std::sin (w0) / (2.0 * std::max (0.05, q));

        const auto a0 =  1.0 + alpha;
        b0 =  (1.0 + cosw) * 0.5 / a0;
        b1 = -(1.0 + cosw)       / a0;
        b2 =  (1.0 + cosw) * 0.5 / a0;
        a1 =  (-2.0 * cosw)      / a0;
        a2 =  (1.0 - alpha)      / a0;
    }

    /** 12 dB/oct low-pass. */
    void setLowpass (double sampleRate, double freq, double q) noexcept
    {
        const auto w0    = omega (sampleRate, freq);
        const auto cosw  = std::cos (w0);
        const auto alpha = std::sin (w0) / (2.0 * std::max (0.05, q));

        const auto a0 = 1.0 + alpha;
        b0 = (1.0 - cosw) * 0.5 / a0;
        b1 = (1.0 - cosw)       / a0;
        b2 = (1.0 - cosw) * 0.5 / a0;
        a1 = (-2.0 * cosw)      / a0;
        a2 = (1.0 - alpha)      / a0;
    }

    /** Peaking (bell) filter. */
    void setPeak (double sampleRate, double freq, double gainDb, double q) noexcept
    {
        if (std::abs (gainDb) < 1.0e-4)
            return setBypass();

        const auto A     = std::pow (10.0, gainDb / 40.0);
        const auto w0    = omega (sampleRate, freq);
        const auto cosw  = std::cos (w0);
        const auto alpha = std::sin (w0) / (2.0 * std::max (0.05, q));

        const auto a0 = 1.0 + alpha / A;
        b0 = (1.0 + alpha * A) / a0;
        b1 = (-2.0 * cosw)     / a0;
        b2 = (1.0 - alpha * A) / a0;
        a1 = (-2.0 * cosw)     / a0;
        a2 = (1.0 - alpha / A) / a0;
    }

    /** Low shelf. @p slope of 1.0 is the classic cookbook shelf; lower values
        give the gentler, wider curve typical of broadcast tone controls. */
    void setLowShelf (double sampleRate, double freq, double gainDb, double slope = 0.8) noexcept
    {
        if (std::abs (gainDb) < 1.0e-4)
            return setBypass();

        const auto A    = std::pow (10.0, gainDb / 40.0);
        const auto w0   = omega (sampleRate, freq);
        const auto cosw = std::cos (w0);
        const auto sinw = std::sin (w0);
        const auto beta = sinw * std::sqrt (std::max (0.0, (A * A + 1.0) / std::max (0.05, slope) - (A - 1.0) * (A - 1.0)));
        const auto Ap1  = A + 1.0;
        const auto Am1  = A - 1.0;

        const auto a0 =  Ap1 + Am1 * cosw + beta;
        b0 =  A * (Ap1 - Am1 * cosw + beta) / a0;
        b1 =  2.0 * A * (Am1 - Ap1 * cosw)  / a0;
        b2 =  A * (Ap1 - Am1 * cosw - beta) / a0;
        a1 = -2.0 * (Am1 + Ap1 * cosw)      / a0;
        a2 =  (Ap1 + Am1 * cosw - beta)     / a0;
    }

    /** High shelf. */
    void setHighShelf (double sampleRate, double freq, double gainDb, double slope = 0.8) noexcept
    {
        if (std::abs (gainDb) < 1.0e-4)
            return setBypass();

        const auto A    = std::pow (10.0, gainDb / 40.0);
        const auto w0   = omega (sampleRate, freq);
        const auto cosw = std::cos (w0);
        const auto sinw = std::sin (w0);
        const auto beta = sinw * std::sqrt (std::max (0.0, (A * A + 1.0) / std::max (0.05, slope) - (A - 1.0) * (A - 1.0)));
        const auto Ap1  = A + 1.0;
        const auto Am1  = A - 1.0;

        const auto a0 =  Ap1 - Am1 * cosw + beta;
        b0 =  A * (Ap1 + Am1 * cosw + beta) / a0;
        b1 = -2.0 * A * (Am1 + Ap1 * cosw)  / a0;
        b2 =  A * (Ap1 + Am1 * cosw - beta) / a0;
        a1 =  2.0 * (Am1 - Ap1 * cosw)      / a0;
        a2 =  (Ap1 - Am1 * cosw - beta)     / a0;
    }

    /** Magnitude response in dB at @p freq - used to draw the EQ curve. */
    double magnitudeDb (double sampleRate, double freq) const noexcept
    {
        const auto w  = omega (sampleRate, freq);
        const auto cw = std::cos (w);
        const auto sw = std::sin (w);
        const auto c2 = std::cos (2.0 * w);
        const auto s2 = std::sin (2.0 * w);

        const auto numRe = b0 + b1 * cw + b2 * c2;
        const auto numIm =    - b1 * sw - b2 * s2;
        const auto denRe = 1.0 + a1 * cw + a2 * c2;
        const auto denIm =     - a1 * sw - a2 * s2;

        const auto num = std::sqrt (numRe * numRe + numIm * numIm);
        const auto den = std::sqrt (denRe * denRe + denIm * denIm);

        return 20.0 * std::log10 (std::max (1.0e-9, num / std::max (1.0e-9, den)));
    }

private:
    static double omega (double sampleRate, double freq) noexcept
    {
        // Keep the pre-warped frequency comfortably below Nyquist.
        const auto f = std::clamp (freq, 5.0, sampleRate * 0.49);
        return 2.0 * 3.14159265358979323846 * f / sampleRate;
    }
};

/** Per-channel bi-quad state, transposed direct form II (good numerical
    behaviour with float and only two state variables). */
struct BiquadState
{
    float z1 = 0.0f, z2 = 0.0f;

    void reset() noexcept { z1 = z2 = 0.0f; }

    inline float process (const BiquadCoeffs& c, float x) noexcept
    {
        const auto y = static_cast<float> (c.b0) * x + z1;
        z1 = static_cast<float> (c.b1) * x - static_cast<float> (c.a1) * y + z2;
        z2 = static_cast<float> (c.b2) * x - static_cast<float> (c.a2) * y;
        return y;
    }
};

/** One-pole DC blocker (~8 Hz), used after every non-linear stage. */
struct DcBlocker
{
    void prepare (double sampleRate) noexcept
    {
        const auto fc = 8.0;
        r = static_cast<float> (1.0 - (2.0 * 3.14159265358979323846 * fc / sampleRate));
        r = std::clamp (r, 0.9f, 0.99999f);
        reset();
    }

    void reset() noexcept { x1 = y1 = 0.0f; }

    inline float process (float x) noexcept
    {
        const auto y = x - x1 + r * y1;
        x1 = x;
        y1 = y;
        return y;
    }

private:
    float r = 0.999f, x1 = 0.0f, y1 = 0.0f;
};

} // namespace kc::dsp
