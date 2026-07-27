/*
    Kilocycle - retro broadcast console channel strip
    Copyright (C) 2026  Kilocycle Audio

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.  See LICENSE for details.
*/

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace kc
{

/** String identifiers for every automatable parameter.

    Kept in one place so the processor, the editor and the preset code can never
    drift apart. Versioned parameter IDs let us rename display names freely
    without breaking sessions.
*/
namespace pid
{
    inline constexpr const char* bypass      = "bypass";

    inline constexpr const char* trim        = "trim";
    inline constexpr const char* lowCut      = "lowcut";

    inline constexpr const char* lowGain     = "low_gain";
    inline constexpr const char* lowFreq     = "low_freq";
    inline constexpr const char* midGain     = "mid_gain";
    inline constexpr const char* midFreq     = "mid_freq";
    inline constexpr const char* midQ        = "mid_q";
    inline constexpr const char* highGain    = "high_gain";
    inline constexpr const char* highFreq    = "high_freq";

    inline constexpr const char* compThresh  = "comp_thresh";
    inline constexpr const char* compRatio   = "comp_ratio";
    inline constexpr const char* compAttack  = "comp_attack";
    inline constexpr const char* compRelease = "comp_release";
    inline constexpr const char* compMix     = "comp_mix";
    inline constexpr const char* compAuto    = "comp_auto";

    inline constexpr const char* drive       = "drive";
    inline constexpr const char* valve       = "valve";
    inline constexpr const char* quality     = "quality";

    inline constexpr const char* output      = "output";
}

/** Builds the full parameter layout for the APVTS. */
juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

/** The valve type numbers, in the order dsp::valveModels() lists them. */
juce::StringArray valveTypeNames();

/** Raw (atomic) pointers to every parameter, resolved once at construction.

    Reading parameters through these on the audio thread costs a single relaxed
    atomic load - no string lookups, no locks.
*/
struct ParameterHandles
{
    void connect (juce::AudioProcessorValueTreeState& state);

    std::atomic<float>* bypass      = nullptr;

    std::atomic<float>* trim        = nullptr;
    std::atomic<float>* lowCut      = nullptr;

    std::atomic<float>* lowGain     = nullptr;
    std::atomic<float>* lowFreq     = nullptr;
    std::atomic<float>* midGain     = nullptr;
    std::atomic<float>* midFreq     = nullptr;
    std::atomic<float>* midQ        = nullptr;
    std::atomic<float>* highGain    = nullptr;
    std::atomic<float>* highFreq    = nullptr;

    std::atomic<float>* compThresh  = nullptr;
    std::atomic<float>* compRatio   = nullptr;
    std::atomic<float>* compAttack  = nullptr;
    std::atomic<float>* compRelease = nullptr;
    std::atomic<float>* compMix     = nullptr;
    std::atomic<float>* compAuto    = nullptr;

    std::atomic<float>* drive       = nullptr;
    std::atomic<float>* valve       = nullptr;
    std::atomic<float>* quality     = nullptr;

    std::atomic<float>* output      = nullptr;
};

} // namespace kc
