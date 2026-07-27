/*
    Kilocycle - retro broadcast console channel strip
    Copyright (C) 2026  Kilocycle Audio
    Licensed under the GNU General Public License v3 or later. See LICENSE.
*/

#include "Parameters.h"
#include "dsp/ValveModels.h"

namespace kc
{

namespace
{
    constexpr int kParamVersion = 1;

    juce::ParameterID pv (const char* id) { return { id, kParamVersion }; }

    juce::String hzToText (float hz, int)
    {
        if (hz >= 1000.0f)
            return juce::String (hz / 1000.0f, hz < 10000.0f ? 2 : 1) + " kHz";

        return juce::String (juce::roundToInt (hz)) + " Hz";
    }

    float textToHz (const juce::String& text)
    {
        auto t = text.trim().toLowerCase();
        auto value = t.getFloatValue();

        if (t.contains ("k"))
            value *= 1000.0f;

        return value;
    }

    juce::String dbToText (float db, int)
    {
        return (db > 0.0f ? "+" : "") + juce::String (db, std::abs (db) < 10.0f ? 1 : 1) + " dB";
    }

    juce::String msToText (float ms, int)
    {
        if (ms >= 1000.0f)
            return juce::String (ms / 1000.0f, 2) + " s";

        return juce::String (ms, ms < 10.0f ? 1 : 0) + " ms";
    }

    juce::String percentToText (float v, int)
    {
        return juce::String (juce::roundToInt (v)) + " %";
    }

    /** Frequency range with the usual musical (log-ish) skew. */
    juce::NormalisableRange<float> freqRange (float lo, float hi)
    {
        juce::NormalisableRange<float> r { lo, hi, 0.01f };
        r.setSkewForCentre (std::sqrt (lo * hi));
        return r;
    }

    using Attributes = juce::AudioParameterFloatAttributes;
} // namespace

juce::StringArray valveTypeNames()
{
    juce::StringArray names;

    for (int i = 0; i < dsp::numValveModels; ++i)
        names.add (dsp::valveModel (i).code);

    return names;
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using APF  = juce::AudioParameterFloat;
    using APB  = juce::AudioParameterBool;
    using APC  = juce::AudioParameterChoice;
    using Range = juce::NormalisableRange<float>;

    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    auto gainAttrs = Attributes().withStringFromValueFunction (dbToText)
                                 .withLabel ("dB");
    auto hzAttrs   = Attributes().withStringFromValueFunction (hzToText)
                                 .withValueFromStringFunction (textToHz)
                                 .withLabel ("Hz");

    // ----- global ----------------------------------------------------------
    layout.add (std::make_unique<APB> (pv (pid::bypass), "Bypass", false));

    // ----- input stage -----------------------------------------------------
    layout.add (std::make_unique<APF> (pv (pid::trim), "Input Trim",
                                       Range { -24.0f, 24.0f, 0.01f }, 0.0f, gainAttrs));

    {
        auto r = freqRange (20.0f, 400.0f);
        auto attrs = Attributes()
            .withStringFromValueFunction ([] (float hz, int) -> juce::String
                                          {
                                              if (hz <= 20.5f)
                                                  return "OFF";
                                              return hzToText (hz, 0);
                                          })
            .withValueFromStringFunction (textToHz)
            .withLabel ("Hz");
        layout.add (std::make_unique<APF> (pv (pid::lowCut), "Low Cut", r, 20.0f, attrs));
    }

    // ----- tone ------------------------------------------------------------
    layout.add (std::make_unique<APF> (pv (pid::lowGain), "Low",
                                       Range { -15.0f, 15.0f, 0.01f }, 0.0f, gainAttrs));
    layout.add (std::make_unique<APF> (pv (pid::lowFreq), "Low Freq",
                                       freqRange (40.0f, 320.0f), 110.0f, hzAttrs));

    layout.add (std::make_unique<APF> (pv (pid::midGain), "Mid",
                                       Range { -15.0f, 15.0f, 0.01f }, 0.0f, gainAttrs));
    layout.add (std::make_unique<APF> (pv (pid::midFreq), "Mid Freq",
                                       freqRange (180.0f, 6500.0f), 1100.0f, hzAttrs));
    layout.add (std::make_unique<APF> (pv (pid::midQ), "Mid Width",
                                       [] { Range r { 0.35f, 4.0f, 0.001f }; r.setSkewForCentre (1.0f); return r; }(),
                                       0.85f,
                                       Attributes().withStringFromValueFunction ([] (float q, int)
                                                                                { return juce::String (q, 2); })));

    layout.add (std::make_unique<APF> (pv (pid::highGain), "High",
                                       Range { -15.0f, 15.0f, 0.01f }, 0.0f, gainAttrs));
    layout.add (std::make_unique<APF> (pv (pid::highFreq), "High Freq",
                                       freqRange (1500.0f, 14000.0f), 5600.0f, hzAttrs));

    // ----- leveller --------------------------------------------------------
    layout.add (std::make_unique<APF> (pv (pid::compThresh), "Threshold",
                                       Range { -48.0f, 0.0f, 0.01f }, -14.0f, gainAttrs));
    layout.add (std::make_unique<APF> (pv (pid::compRatio), "Ratio",
                                       [] { Range r { 1.0f, 12.0f, 0.01f }; r.setSkewForCentre (3.0f); return r; }(),
                                       2.5f,
                                       Attributes().withStringFromValueFunction ([] (float r, int)
                                                                                { return juce::String (r, 2) + " : 1"; })));
    layout.add (std::make_unique<APF> (pv (pid::compAttack), "Attack",
                                       [] { Range r { 0.5f, 120.0f, 0.01f }; r.setSkewForCentre (15.0f); return r; }(),
                                       18.0f,
                                       Attributes().withStringFromValueFunction (msToText).withLabel ("ms")));
    layout.add (std::make_unique<APF> (pv (pid::compRelease), "Release",
                                       [] { Range r { 30.0f, 1500.0f, 0.1f }; r.setSkewForCentre (280.0f); return r; }(),
                                       280.0f,
                                       Attributes().withStringFromValueFunction (msToText).withLabel ("ms")));
    layout.add (std::make_unique<APF> (pv (pid::compMix), "Comp Mix",
                                       Range { 0.0f, 100.0f, 0.1f }, 100.0f,
                                       Attributes().withStringFromValueFunction (percentToText).withLabel ("%")));
    layout.add (std::make_unique<APB> (pv (pid::compAuto), "Auto Makeup", true));

    // ----- valve -----------------------------------------------------------
    layout.add (std::make_unique<APF> (pv (pid::drive), "Valve Drive",
                                       Range { 0.0f, 100.0f, 0.1f }, 25.0f,
                                       Attributes().withStringFromValueFunction (percentToText).withLabel ("%")));

    layout.add (std::make_unique<APC> (pv (pid::valve), "Valve", valveTypeNames(), 0));

    layout.add (std::make_unique<APC> (pv (pid::quality), "Quality",
                                       juce::StringArray { "Eco (2x)", "Fine (4x)" }, 1));

    // ----- output ----------------------------------------------------------
    layout.add (std::make_unique<APF> (pv (pid::output), "Output",
                                       Range { -24.0f, 12.0f, 0.01f }, 0.0f, gainAttrs));

    return layout;
}

void ParameterHandles::connect (juce::AudioProcessorValueTreeState& state)
{
    auto get = [&state] (const char* id)
    {
        auto* p = state.getRawParameterValue (id);
        jassert (p != nullptr);
        return p;
    };

    bypass      = get (pid::bypass);

    trim        = get (pid::trim);
    lowCut      = get (pid::lowCut);

    lowGain     = get (pid::lowGain);
    lowFreq     = get (pid::lowFreq);
    midGain     = get (pid::midGain);
    midFreq     = get (pid::midFreq);
    midQ        = get (pid::midQ);
    highGain    = get (pid::highGain);
    highFreq    = get (pid::highFreq);

    compThresh  = get (pid::compThresh);
    compRatio   = get (pid::compRatio);
    compAttack  = get (pid::compAttack);
    compRelease = get (pid::compRelease);
    compMix     = get (pid::compMix);
    compAuto    = get (pid::compAuto);

    drive       = get (pid::drive);
    valve       = get (pid::valve);
    quality     = get (pid::quality);

    output      = get (pid::output);
}

} // namespace kc
