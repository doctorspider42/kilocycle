/*
    Kilocycle - retro broadcast console channel strip
    Copyright (C) 2026  Kilocycle Audio
    Licensed under the GNU General Public License v3 or later. See LICENSE.
*/

#pragma once

#include "Biquad.h"
#include "ValveModels.h"
#include <array>

namespace kc::dsp
{

/** Gentle valve / output-transformer colouration, with a swappable valve.

    Deliberately restrained: even at full drive this is a warming stage, not a
    distortion box. Four things happen together, which is what makes it read as
    "valve" rather than "clipper":

      1. Low-frequency pre-emphasis pushes the bottom end harder into the
         non-linearity and a complementary shelf pulls it back afterwards. The
         low mids gain harmonic density without the mix turning muddy.
      2. The transfer curve is asymmetric - the negative half compresses a little
         more than the positive one, exactly like a single-ended triode stage.
         That asymmetry is what produces second-harmonic warmth instead of the
         hollow odd-harmonic sound of a symmetric clipper. A small grid bias adds
         more of the same at low levels.
      3. Output valves sag: the supply droops on sustained peaks, so the stage
         compresses itself a little. That is most of what separates an EL84 from
         a small-signal triode.
      4. A high shelf and a gentle voicing bell afterwards take the edge off the
         highest harmonics, the way the output transformer would.

    The shaper blends two curves - x / sqrt(1 + x^2) and x / (1 + |x|) - which
    share a unity slope at the origin, so the blend controls how long the knee is
    without ever introducing a discontinuity. Both are cheaper than tanh, and with
    2x/4x oversampling in the host processor aliasing stays below the noise floor.

    Every model parameter is scaled by drive, so at Drive 0 all valves collapse to
    the same near-linear stage and the strip measures 0.00 dB. Swapping valves
    changes how the stage behaves when worked, not the frequency response at rest.

    Everything is run through a DC blocker afterwards, because an asymmetric curve
    necessarily produces a DC offset.
*/
class TubeStage
{
public:
    static constexpr int kMaxChannels = 2;

    void prepare (double oversampledRate, int numChannels)
    {
        sampleRate = oversampledRate;
        channels   = std::min (numChannels, kMaxChannels);

        for (auto& d : dcBlock)
            d.prepare (sampleRate);

        // ~4 ms smoothing on the derived gains, so neither automation nor a valve
        // swap can produce a step.
        smoothCoeff = static_cast<float> (1.0 - std::exp (-1.0 / (0.004 * sampleRate)));

        // Supply sag: quick to droop, slow to recover.
        sagAttack  = static_cast<float> (1.0 - std::exp (-1.0 / (0.015 * sampleRate)));
        sagRelease = static_cast<float> (1.0 - std::exp (-1.0 / (0.180 * sampleRate)));

        updateCoefficients (true);
        reset();
    }

    void reset()
    {
        for (auto& bank : preState)      for (auto& s : bank) s.reset();
        for (auto& bank : postState)     for (auto& s : bank) s.reset();
        for (auto& bank : tiltState)     for (auto& s : bank) s.reset();
        for (auto& bank : presenceState) for (auto& s : bank) s.reset();
        for (auto& d : dcBlock) d.reset();

        smoothedG    = targetG;
        smoothedNegK = targetNegK;
        smoothedBias = targetBias;
        smoothedTrim = targetTrim;
        smoothedSag  = targetSag;
        sagEnvelope  = 0.0f;
    }

    /** @param amount  0 .. 1 */
    void setDrive (float amount)
    {
        amount = std::clamp (amount, 0.0f, 1.0f);

        if (std::abs (amount - drive) < 1.0e-5f)
            return;

        drive = amount;
        updateCoefficients (false);
    }

    /** Fit a different valve. Index into dsp::valveModels(). */
    void setModel (int index)
    {
        const auto clamped = std::clamp (index, 0, numValveModels - 1);

        if (clamped == modelIndex)
            return;

        modelIndex = clamped;
        updateCoefficients (false);
    }

    int getModelIndex() const noexcept { return modelIndex; }

    /** Processes one sample frame in place. Frame-based rather than per-sample
        because the sag envelope is shared between channels, exactly as a single
        power supply would be. */
    inline void processFrame (float* const* data, int index, int numChannels) noexcept
    {
        // ---- control smoothing ------------------------------------------------
        smoothedG    += smoothCoeff * (targetG    - smoothedG);
        smoothedNegK += smoothCoeff * (targetNegK - smoothedNegK);
        smoothedBias += smoothCoeff * (targetBias - smoothedBias);
        smoothedTrim += smoothCoeff * (targetTrim - smoothedTrim);
        smoothedSag  += smoothCoeff * (targetSag  - smoothedSag);

        const auto g     = smoothedG;
        const auto invG  = 1.0f / g;
        const auto negK  = smoothedNegK;
        const auto soft  = smoothedSoftness;
        const auto bias  = smoothedBias;

        // Static offset the bias introduces, removed so the DC blocker has almost
        // nothing left to do.
        const auto biasOffset = shape (bias, negK, soft);

        // ---- supply sag -------------------------------------------------------
        auto peak = 0.0f;

        for (int ch = 0; ch < numChannels; ++ch)
            peak = std::max (peak, std::abs (data[ch][index]));

        sagEnvelope += (peak > sagEnvelope ? sagAttack : sagRelease) * (peak - sagEnvelope);

        const auto sagGain = 1.0f - smoothedSag * std::min (1.0f, sagEnvelope * 1.4f);

        // ---- the valve --------------------------------------------------------
        for (int ch = 0; ch < numChannels; ++ch)
        {
            const auto c = (size_t) ch;
            auto x = data[ch][index];

            x = preState[c][0].process (preCoeffs, x);

            x = (shape (x * g + bias, negK, soft) - biasOffset) * invG;

            x = postState[c][0].process (postCoeffs, x);
            x = tiltState[c][0].process (tiltCoeffs, x);
            x = presenceState[c][0].process (presenceCoeffs, x);
            x = dcBlock[c].process (x);

            data[ch][index] = x * smoothedTrim * sagGain;
        }
    }

private:
    /** Firm knee. */
    static inline float shapeFirm (float x) noexcept
    {
        return x / std::sqrt (1.0f + x * x);
    }

    /** Long, gradual knee - more low-order harmonic content for the same level. */
    static inline float shapeGradual (float x) noexcept
    {
        return x / (1.0f + std::abs (x));
    }

    /** Both curves have unity slope at the origin, so any blend of them does too. */
    static inline float shapeBlend (float x, float softness) noexcept
    {
        const auto firm = shapeFirm (x);
        return softness <= 1.0e-4f ? firm : firm + softness * (shapeGradual (x) - firm);
    }

    /** Asymmetric soft saturation. The two halves share the same slope at the
        origin, so the curve stays C1-continuous and only the curvature differs -
        that difference is the second-harmonic content. */
    static inline float shape (float x, float negK, float softness) noexcept
    {
        if (x >= 0.0f)
            return shapeBlend (x, softness);

        const auto xn = x * negK;
        return shapeBlend (xn, softness) / negK;
    }

    void updateCoefficients (bool force)
    {
        const auto& model = valveModel (modelIndex);

        // Input gain into the shaper. Small values keep the curve in its almost
        // linear region, which is why low settings are genuinely transparent.
        targetG    = 0.15f + 2.5f * model.driveScale * std::pow (drive, 1.3f);
        targetNegK = 1.0f + model.asymmetry * drive;
        targetBias = model.bias * drive;
        // Make-up that tracks both how hard the valve is driven and how much it
        // sags, so swapping valves compares character rather than loudness.
        targetTrim = 1.0f + (0.42f * model.driveScale + 0.5f * model.sag) * drive;
        targetSag  = model.sag * drive;

        // Softness is only used inside the shaper, where a per-sample smoother
        // would buy nothing: the blend is continuous in the parameter itself.
        smoothedSoftness = model.kneeSoftness * drive;

        const auto emphasisDb = model.bassPushDb * drive;
        preCoeffs .setLowShelf  (sampleRate, model.bassPushHz,  emphasisDb, 0.7);
        postCoeffs.setLowShelf  (sampleRate, model.bassPushHz, -emphasisDb, 0.7);
        tiltCoeffs.setHighShelf (sampleRate, model.tiltHz, model.tiltDb * drive, 0.7);
        presenceCoeffs.setPeak  (sampleRate, model.presenceHz, model.presenceDb * drive, model.presenceQ);

        if (force)
        {
            smoothedG    = targetG;
            smoothedNegK = targetNegK;
            smoothedBias = targetBias;
            smoothedTrim = targetTrim;
            smoothedSag  = targetSag;
        }
    }

    double sampleRate = 88200.0;
    int    channels   = 2;

    int   modelIndex = 0;
    float drive      = 0.25f;

    float targetG = 0.5f,    smoothedG = 0.5f;
    float targetNegK = 1.1f, smoothedNegK = 1.1f;
    float targetBias = 0.0f, smoothedBias = 0.0f;
    float targetTrim = 1.0f, smoothedTrim = 1.0f;
    float targetSag = 0.0f,  smoothedSag = 0.0f;
    float smoothedSoftness = 0.0f;

    float smoothCoeff = 0.01f;
    float sagAttack = 0.01f, sagRelease = 0.001f;
    float sagEnvelope = 0.0f;

    BiquadCoeffs preCoeffs, postCoeffs, tiltCoeffs, presenceCoeffs;

    std::array<std::array<BiquadState, 1>, kMaxChannels> preState {};
    std::array<std::array<BiquadState, 1>, kMaxChannels> postState {};
    std::array<std::array<BiquadState, 1>, kMaxChannels> tiltState {};
    std::array<std::array<BiquadState, 1>, kMaxChannels> presenceState {};
    std::array<DcBlocker, kMaxChannels> dcBlock {};
};

} // namespace kc::dsp
