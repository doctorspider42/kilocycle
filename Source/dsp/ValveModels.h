/*
    Kilocycle - retro broadcast console channel strip
    Copyright (C) 2026  Kilocycle Audio
    Licensed under the GNU General Public License v3 or later. See LICENSE.
*/

#pragma once

namespace kc::dsp
{

/** How the bottle is built - the GUI draws a different envelope and base for each. */
enum class Bottle
{
    miniature,   ///< all-glass, pins straight out of the glass (B7G / B9A)
    octal        ///< bakelite base with a locating key
};

/** One valve's worth of character.

    Every field is scaled by the Drive control inside TubeStage, which keeps an
    important property intact: at Drive 0 all six valves are identical and the
    stage measures 0.00 dB. You are choosing how the valve behaves when it is
    being worked, not adding a permanent EQ curve.

    The numbers are a caricature rather than a measurement - what each valve is
    known for, exaggerated just enough to be audible at sensible settings:

      - driveScale    how readily it leaves its linear region
      - asymmetry     how much harder the negative half compresses (2nd harmonic)
      - bias          static grid offset, more even harmonics at low levels
      - kneeSoftness  0 = firm knee, 1 = long gradual knee (more low-order content)
      - bassPush      pre-emphasis into the non-linearity, taken back out after
      - tilt          post high shelf, the output transformer's manners
      - presence      a gentle fixed voicing bell
      - sag           supply sag: level-dependent compression, an output-valve trait
*/
struct ValveModel
{
    const char* code;
    const char* description;

    Bottle bottle;
    int    pins;
    int    electrodes;      ///< 2 for a triode, 3 for a pentode / beam tetrode

    float driveScale;
    float asymmetry;
    float bias;
    float kneeSoftness;

    float bassPushDb;
    float bassPushHz;
    float tiltDb;
    float tiltHz;

    float presenceDb;
    float presenceHz;
    float presenceQ;

    float sag;
};

inline constexpr int numValveModels = 6;

inline const ValveModel* valveModels() noexcept
{
    static const ValveModel models[numValveModels]
    {
        //  code      description                                bottle              pins el  drive  asym   bias  knee   bassDb bassHz  tiltDb tiltHz  presDb presHz presQ  sag
        {  "ECC83",  "twin triode - the studio standard",        Bottle::miniature,   9,  2,  1.00f, 1.05f, 0.030f, 0.15f,  3.0f, 220.0f, -1.2f, 7500.0f,  0.0f, 3000.0f, 0.8f, 0.00f },
        {  "ECC88",  "twin triode - fast, clean, extended",      Bottle::miniature,   9,  2,  0.70f, 0.70f, 0.015f, 0.05f,  1.2f, 200.0f, -0.5f, 11000.0f, 0.0f, 3000.0f, 0.8f, 0.00f },
        {  "EF86",   "pentode - the broadcast microphone valve", Bottle::miniature,   7,  3,  1.25f, 0.22f, 0.035f, 0.20f,  1.6f, 180.0f, -0.4f, 9000.0f,  1.3f, 2800.0f, 0.75f, 0.10f },
        {  "6SN7",   "octal twin triode - large and open",       Bottle::octal,       8,  2,  0.82f, 1.35f, 0.055f, 0.65f,  4.5f, 160.0f, -1.8f, 6500.0f, -0.7f, 3200.0f, 0.7f, 0.05f },
        {  "6V6",    "beam tetrode - sweet and compressed",      Bottle::octal,       8,  3,  1.15f, 1.15f, 0.080f, 0.50f,  3.8f, 200.0f, -1.6f, 7000.0f,  0.6f, 1600.0f, 0.7f, 0.20f },
        {  "EL84",   "output pentode - bright and springy",      Bottle::miniature,   9,  3,  1.40f, 0.32f, 0.050f, 0.30f,  2.0f, 250.0f, -0.3f, 10000.0f, 1.6f, 2200.0f, 0.8f, 0.28f },
    };

    return models;
}

inline const ValveModel& valveModel (int index) noexcept
{
    const auto clamped = index < 0 ? 0 : (index >= numValveModels ? numValveModels - 1 : index);
    return valveModels()[clamped];
}

} // namespace kc::dsp
