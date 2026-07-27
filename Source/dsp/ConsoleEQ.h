/*
    Kilocycle - retro broadcast console channel strip
    Copyright (C) 2026  Kilocycle Audio
    Licensed under the GNU General Public License v3 or later. See LICENSE.
*/

#pragma once

#include "Biquad.h"
#include <array>

namespace kc::dsp
{

/** The four tone-control settings of the console strip. */
struct EqSettings
{
    float lowCutHz  = 20.0f;
    float lowGainDb = 0.0f;
    float lowFreq   = 110.0f;
    float midGainDb = 0.0f;
    float midFreq   = 1100.0f;
    float midQ      = 0.85f;
    float highGainDb = 0.0f;
    float highFreq   = 5600.0f;
};

/** Broadcast-style tone section: switchable low cut, low shelf, mid bell,
    high shelf.

    Coefficients are shared between channels and refreshed at control rate
    (every ~32 samples by the processor), which keeps parameter moves smooth
    without ever allocating or locking.
*/
class ConsoleEQ
{
public:
    static constexpr int kMaxChannels = 2;

    void prepare (double newSampleRate, int numChannels)
    {
        sampleRate = newSampleRate;
        channels   = std::min (numChannels, kMaxChannels);
        reset();
    }

    void reset()
    {
        for (auto& bank : state)
            for (auto& s : bank)
                s.reset();
    }

    void updateCoefficients (const EqSettings& s)
    {
        lowCutActive = s.lowCutHz > 20.5f;

        if (lowCutActive)
            coeffs[hpf].setHighpass (sampleRate, s.lowCutHz, 0.7071);
        else
            coeffs[hpf].setBypass();

        coeffs[lowShelf] .setLowShelf  (sampleRate, s.lowFreq,  s.lowGainDb,  0.75);
        coeffs[midPeak]  .setPeak      (sampleRate, s.midFreq,  s.midGainDb,  s.midQ);
        coeffs[highShelf].setHighShelf (sampleRate, s.highFreq, s.highGainDb, 0.75);
    }

    inline float processSample (int channel, float x) noexcept
    {
        auto& bank = state[(size_t) channel];

        if (lowCutActive)
            x = bank[hpf].process (coeffs[hpf], x);

        x = bank[lowShelf] .process (coeffs[lowShelf],  x);
        x = bank[midPeak]  .process (coeffs[midPeak],   x);
        x = bank[highShelf].process (coeffs[highShelf], x);

        return x;
    }

    /** Offline helper for drawing the response curve in the editor. */
    static double magnitudeDbAt (double sampleRate, const EqSettings& s, double freq)
    {
        BiquadCoeffs c;
        double sum = 0.0;

        if (s.lowCutHz > 20.5f)
        {
            c.setHighpass (sampleRate, s.lowCutHz, 0.7071);
            sum += c.magnitudeDb (sampleRate, freq);
        }

        c.setLowShelf (sampleRate, s.lowFreq, s.lowGainDb, 0.75);
        sum += c.magnitudeDb (sampleRate, freq);

        c.setPeak (sampleRate, s.midFreq, s.midGainDb, s.midQ);
        sum += c.magnitudeDb (sampleRate, freq);

        c.setHighShelf (sampleRate, s.highFreq, s.highGainDb, 0.75);
        sum += c.magnitudeDb (sampleRate, freq);

        return sum;
    }

private:
    enum Stage { hpf = 0, lowShelf, midPeak, highShelf, numStages };

    double sampleRate = 44100.0;
    int    channels   = 2;
    bool   lowCutActive = false;

    std::array<BiquadCoeffs, numStages> coeffs {};
    std::array<std::array<BiquadState, numStages>, kMaxChannels> state {};
};

} // namespace kc::dsp
