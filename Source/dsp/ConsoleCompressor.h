/*
    Kilocycle - retro broadcast console channel strip
    Copyright (C) 2026  Kilocycle Audio
    Licensed under the GNU General Public License v3 or later. See LICENSE.
*/

#pragma once

#include <array>
#include <cmath>
#include <algorithm>

namespace kc::dsp
{

/** Stereo-linked, feed-forward soft-knee compressor with program-dependent
    release - the behaviour of a broadcast levelling amplifier rather than a
    surgical mastering compressor.

    Two release envelopes run in parallel: a fast one following the set release
    time, and a slow one five times longer. The applied reduction is the larger
    of the two, so brief transients recover quickly while sustained loud passages
    keep the amplifier "leaned in" - which is what stops the pumping you get from
    a single-time-constant design.

    Detection runs through a gentle 85 Hz side-chain high-pass so that kick drums
    and room rumble do not modulate the whole programme.
*/
class ConsoleCompressor
{
public:
    static constexpr int   kMaxChannels = 2;
    static constexpr float kKneeDb      = 6.0f;

    void prepare (double newSampleRate, int numChannels)
    {
        sampleRate = newSampleRate;
        channels   = std::min (numChannels, kMaxChannels);

        // 85 Hz one-pole side-chain high-pass.
        scCoeff = static_cast<float> (1.0 - std::exp (-2.0 * 3.14159265358979 * 85.0 / sampleRate));

        // ~0.5 ms smoothing on the rectified detector signal.
        detCoeff = static_cast<float> (1.0 - std::exp (-1.0 / (0.0005 * sampleRate)));

        setTimes (18.0f, 280.0f);
        reset();
    }

    void reset()
    {
        scState.fill (0.0f);
        detector = 0.0f;
        grFast = grSlow = 0.0f;
        displayGr = 0.0f;
    }

    void setThresholdAndRatio (float newThresholdDb, float newRatio) noexcept
    {
        thresholdDb = newThresholdDb;
        ratio       = std::max (1.0f, newRatio);
        slope       = 1.0f / ratio - 1.0f;

        // Static reduction the curve would apply to a 0 dBFS signal, used as
        // the basis for auto make-up.
        const auto staticGr = -(curveDb (0.0f));
        autoMakeupDb = std::clamp (staticGr * 0.6f, 0.0f, 18.0f);
    }

    void setTimes (float attackMs, float releaseMs) noexcept
    {
        attackFast  = timeToCoeff (attackMs);
        releaseFast = timeToCoeff (releaseMs);
        attackSlow  = timeToCoeff (attackMs  * 5.0f);
        releaseSlow = timeToCoeff (releaseMs * 5.0f);
    }

    /** Runs the detector for one sample frame and returns the linear gain to
        apply to every channel. */
    inline float processFrame (const float* const* channelData, int sampleIndex) noexcept
    {
        // ---- detection ----------------------------------------------------
        float peak = 0.0f;

        for (int ch = 0; ch < channels; ++ch)
        {
            const auto x = channelData[ch][sampleIndex];

            // One-pole low-pass, subtracted -> high-passed side chain.
            scState[(size_t) ch] += scCoeff * (x - scState[(size_t) ch]);
            peak = std::max (peak, std::abs (x - scState[(size_t) ch]));
        }

        detector += detCoeff * (peak - detector);

        const auto levelDb  = 20.0f * std::log10 (std::max (1.0e-6f, detector));
        const auto targetGr = std::max (0.0f, levelDb - curveDb (levelDb));

        // ---- dual-time-constant ballistics --------------------------------
        grFast += (targetGr > grFast ? attackFast : releaseFast) * (targetGr - grFast);
        grSlow += (targetGr > grSlow ? attackSlow : releaseSlow) * (targetGr - grSlow);

        const auto gr = std::max (grFast, grSlow);
        displayGr = gr;

        return dbToGain (-gr);
    }

    float getGainReductionDb() const noexcept { return displayGr; }
    float getAutoMakeupDb()    const noexcept { return autoMakeupDb; }

private:
    /** Soft-knee static curve: input dB -> output dB. */
    inline float curveDb (float xDb) const noexcept
    {
        const auto over = xDb - thresholdDb;

        if (over <= -kKneeDb * 0.5f)
            return xDb;

        if (over >= kKneeDb * 0.5f)
            return xDb + slope * over;

        const auto t = over + kKneeDb * 0.5f;
        return xDb + slope * (t * t) / (2.0f * kKneeDb);
    }

    static inline float dbToGain (float db) noexcept
    {
        return std::pow (10.0f, db * 0.05f);
    }

    float timeToCoeff (float ms) const noexcept
    {
        const auto seconds = std::max (1.0e-4f, ms * 0.001f);
        return static_cast<float> (1.0 - std::exp (-1.0 / (seconds * sampleRate)));
    }

    double sampleRate = 44100.0;
    int    channels   = 2;

    float thresholdDb = -14.0f;
    float ratio       = 2.5f;
    float slope       = 1.0f / 2.5f - 1.0f;
    float autoMakeupDb = 0.0f;

    float attackFast = 0.01f, releaseFast = 0.001f;
    float attackSlow = 0.002f, releaseSlow = 0.0002f;

    float scCoeff  = 0.01f;
    float detCoeff = 0.1f;

    std::array<float, kMaxChannels> scState {};
    float detector  = 0.0f;
    float grFast    = 0.0f;
    float grSlow    = 0.0f;
    float displayGr = 0.0f;
};

} // namespace kc::dsp
