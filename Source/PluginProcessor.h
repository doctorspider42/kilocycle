/*
    Kilocycle - retro broadcast console channel strip
    Copyright (C) 2026  Kilocycle Audio
    Licensed under the GNU General Public License v3 or later. See LICENSE.
*/

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "Parameters.h"
#include "dsp/ConsoleEQ.h"
#include "dsp/ConsoleCompressor.h"
#include "dsp/TubeStage.h"
#include "dsp/LevelFollower.h"

namespace kc
{

/** The channel strip.

    Signal flow:

        input trim -> low cut -> tone (low / mid / high) -> leveller
                   -> valve stage (oversampled) -> output

    Control-rate work (filter coefficients, compressor time constants) happens
    once every 32 samples; sample-rate work is branch-light and allocation-free.
*/
class KilocycleProcessor final : public juce::AudioProcessor,
                                 private juce::AsyncUpdater
{
public:
    KilocycleProcessor();
    ~KilocycleProcessor() override;

    // ---- AudioProcessor ---------------------------------------------------
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;   // keep the double-precision overload visible
    void reset() override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }

    bool acceptsMidi()  const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    juce::AudioProcessorParameter* getBypassParameter() const override;

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ---- editor support ---------------------------------------------------
    juce::AudioProcessorValueTreeState& getState() noexcept { return apvts; }

    /** Current tone settings, for drawing the response curve. */
    dsp::EqSettings currentEqSettings() const noexcept;
    double displaySampleRate() const noexcept { return currentSampleRate; }

    /** Metering values, written on the audio thread and read by the editor. */
    std::atomic<float> meterOutputVu   { -30.0f };  // dB relative to 0 VU
    std::atomic<float> meterInputVu    { -30.0f };
    std::atomic<float> meterGainReduce {   0.0f };  // dB, positive
    std::atomic<float> meterValveHeat  {   0.0f };  // 0 .. 1, drives the glow

private:
    void handleAsyncUpdate() override;   // reports latency changes to the host
    void updateOversamplingSelection (int wanted);

    static constexpr int kControlBlock = 32;

    juce::AudioProcessorValueTreeState apvts;
    ParameterHandles p;

    dsp::ConsoleEQ         eq;
    dsp::ConsoleCompressor comp;
    dsp::VuFollower        vuIn, vuOut;

    // Index 0: economical 2x IIR. Index 1: linear-phase 4x FIR.
    std::array<std::unique_ptr<juce::dsp::Oversampling<float>>, 2> oversamplers;
    std::array<dsp::TubeStage, 2> tube;
    int activeQuality = 1;
    std::atomic<int> pendingLatency { 0 };

    juce::AudioBuffer<float> dryBuffer;   // pre-compressor copy, for Comp Mix

    juce::SmoothedValue<float> smTrim, smOutput, smCompMix;
    juce::SmoothedValue<float> smLowCut, smLowGain, smLowFreq,
                               smMidGain, smMidFreq, smMidQ,
                               smHighGain, smHighFreq;
    juce::SmoothedValue<float> smThresh, smRatio, smAttack, smRelease, smDrive;

    double currentSampleRate = 44100.0;
    int    currentBlockSize  = 512;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KilocycleProcessor)
};

} // namespace kc
