/*
    Kilocycle - retro broadcast console channel strip
    Copyright (C) 2026  Kilocycle Audio
    Licensed under the GNU General Public License v3 or later. See LICENSE.
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace kc
{

using OS = juce::dsp::Oversampling<float>;

KilocycleProcessor::KilocycleProcessor()
    : juce::AudioProcessor (BusesProperties()
                                .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                                .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "KILOCYCLE", createParameterLayout())
{
    p.connect (apvts);
}

KilocycleProcessor::~KilocycleProcessor()
{
    cancelPendingUpdate();
}

juce::AudioProcessorParameter* KilocycleProcessor::getBypassParameter() const
{
    return apvts.getParameter (pid::bypass);
}

bool KilocycleProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in  = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();

    if (in != out)
        return false;

    return out == juce::AudioChannelSet::mono()
        || out == juce::AudioChannelSet::stereo();
}

void KilocycleProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    currentBlockSize  = samplesPerBlock;

    const auto numChannels = juce::jmax (1, juce::jmin (getTotalNumOutputChannels(),
                                                       dsp::ConsoleEQ::kMaxChannels));

    eq.prepare (sampleRate, numChannels);
    comp.prepare (sampleRate, numChannels);
    vuIn.prepare (sampleRate);
    vuOut.prepare (sampleRate);

    // Two oversamplers, both kept ready so switching quality never allocates.
    oversamplers[0] = std::make_unique<OS> ((size_t) numChannels, 1,
                                            OS::filterHalfBandPolyphaseIIR, false, true);
    oversamplers[1] = std::make_unique<OS> ((size_t) numChannels, 2,
                                            OS::filterHalfBandFIREquiripple, true, true);

    for (size_t i = 0; i < oversamplers.size(); ++i)
    {
        oversamplers[i]->initProcessing ((size_t) juce::jmax (1, samplesPerBlock));
        oversamplers[i]->reset();

        const auto factor = i == 0 ? 2.0 : 4.0;
        tube[i].prepare (sampleRate * factor, numChannels);
    }

    activeQuality = juce::jlimit (0, 1, (int) p.quality->load());
    setLatencySamples (juce::roundToInt (oversamplers[(size_t) activeQuality]->getLatencyInSamples()));

    dryBuffer.setSize (numChannels, kControlBlock, false, true, true);

    // ---- parameter smoothing ---------------------------------------------
    const auto fast = 0.02;   // gains
    const auto slow = 0.06;   // filter frequencies / dynamics timing

    for (auto* s : { &smTrim, &smOutput })
        s->reset (sampleRate, fast);

    for (auto* s : { &smCompMix, &smLowCut, &smLowGain, &smLowFreq, &smMidGain,
                     &smMidFreq, &smMidQ, &smHighGain, &smHighFreq,
                     &smThresh, &smRatio, &smAttack, &smRelease, &smDrive })
        s->reset (sampleRate, slow);

    smTrim    .setCurrentAndTargetValue (juce::Decibels::decibelsToGain (p.trim->load()));
    smOutput  .setCurrentAndTargetValue (juce::Decibels::decibelsToGain (p.output->load()));
    smCompMix .setCurrentAndTargetValue (p.compMix->load() * 0.01f);
    smLowCut  .setCurrentAndTargetValue (p.lowCut->load());
    smLowGain .setCurrentAndTargetValue (p.lowGain->load());
    smLowFreq .setCurrentAndTargetValue (p.lowFreq->load());
    smMidGain .setCurrentAndTargetValue (p.midGain->load());
    smMidFreq .setCurrentAndTargetValue (p.midFreq->load());
    smMidQ    .setCurrentAndTargetValue (p.midQ->load());
    smHighGain.setCurrentAndTargetValue (p.highGain->load());
    smHighFreq.setCurrentAndTargetValue (p.highFreq->load());
    smThresh  .setCurrentAndTargetValue (p.compThresh->load());
    smRatio   .setCurrentAndTargetValue (p.compRatio->load());
    smAttack  .setCurrentAndTargetValue (p.compAttack->load());
    smRelease .setCurrentAndTargetValue (p.compRelease->load());
    smDrive   .setCurrentAndTargetValue (p.drive->load() * 0.01f);

    for (auto& t : tube)
    {
        t.setModel ((int) p.valve->load());
        t.setDrive (smDrive.getCurrentValue());
        t.reset();
    }
}

void KilocycleProcessor::releaseResources()
{
    for (auto& os : oversamplers)
        if (os != nullptr)
            os->reset();
}

void KilocycleProcessor::reset()
{
    eq.reset();
    comp.reset();
    vuIn.reset();
    vuOut.reset();

    for (auto& os : oversamplers)
        if (os != nullptr)
            os->reset();

    for (auto& t : tube)
        t.reset();
}

void KilocycleProcessor::updateOversamplingSelection (int wanted)
{
    wanted = juce::jlimit (0, 1, wanted);

    if (wanted == activeQuality)
        return;

    activeQuality = wanted;

    if (auto* os = oversamplers[(size_t) activeQuality].get())
    {
        os->reset();
        tube[(size_t) activeQuality].reset();
        pendingLatency.store (juce::roundToInt (os->getLatencyInSamples()));
        triggerAsyncUpdate();
    }
}

void KilocycleProcessor::handleAsyncUpdate()
{
    setLatencySamples (pendingLatency.load());
}

void KilocycleProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto totalIn    = getTotalNumInputChannels();
    const auto totalOut   = getTotalNumOutputChannels();
    const auto numSamples = buffer.getNumSamples();

    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, numSamples);

    const auto numChannels = juce::jmin (totalOut, dsp::ConsoleEQ::kMaxChannels);

    if (numChannels <= 0 || numSamples <= 0)
        return;

    const auto bypassed = p.bypass->load() > 0.5f;

    if (const auto wanted = (int) p.quality->load(); wanted != activeQuality)
        updateOversamplingSelection (wanted);

    // ---- pick up parameter changes once per block -------------------------
    smTrim    .setTargetValue (juce::Decibels::decibelsToGain (p.trim->load()));
    smOutput  .setTargetValue (juce::Decibels::decibelsToGain (p.output->load()));
    smCompMix .setTargetValue (p.compMix->load() * 0.01f);
    smLowCut  .setTargetValue (p.lowCut->load());
    smLowGain .setTargetValue (p.lowGain->load());
    smLowFreq .setTargetValue (p.lowFreq->load());
    smMidGain .setTargetValue (p.midGain->load());
    smMidFreq .setTargetValue (p.midFreq->load());
    smMidQ    .setTargetValue (p.midQ->load());
    smHighGain.setTargetValue (p.highGain->load());
    smHighFreq.setTargetValue (p.highFreq->load());
    smThresh  .setTargetValue (p.compThresh->load());
    smRatio   .setTargetValue (p.compRatio->load());
    smAttack  .setTargetValue (p.compAttack->load());
    smRelease .setTargetValue (p.compRelease->load());
    smDrive   .setTargetValue (p.drive->load() * 0.01f);

    auto* const* io = buffer.getArrayOfWritePointers();
    const auto* const* readOnly = buffer.getArrayOfReadPointers();

    // ---- input metering ---------------------------------------------------
    for (int n = 0; n < numSamples; ++n)
    {
        auto m = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
            m = juce::jmax (m, std::abs (io[ch][n]));

        vuIn.push (m);
    }

    // ---- trim, tone and levelling, in control-rate chunks -----------------
    if (! bypassed)
    {
        for (int start = 0; start < numSamples; start += kControlBlock)
        {
            const auto len = juce::jmin (kControlBlock, numSamples - start);

            dsp::EqSettings s;
            s.lowCutHz   = smLowCut  .skip (len);
            s.lowGainDb  = smLowGain .skip (len);
            s.lowFreq    = smLowFreq .skip (len);
            s.midGainDb  = smMidGain .skip (len);
            s.midFreq    = smMidFreq .skip (len);
            s.midQ       = smMidQ    .skip (len);
            s.highGainDb = smHighGain.skip (len);
            s.highFreq   = smHighFreq.skip (len);
            eq.updateCoefficients (s);

            comp.setThresholdAndRatio (smThresh.skip (len), smRatio.skip (len));
            comp.setTimes (smAttack.skip (len), smRelease.skip (len));

            const auto mix    = smCompMix.skip (len);
            const auto makeup = p.compAuto->load() > 0.5f
                                    ? juce::Decibels::decibelsToGain (comp.getAutoMakeupDb())
                                    : 1.0f;

            for (int n = 0; n < len; ++n)
            {
                const auto g = smTrim.getNextValue();

                for (int ch = 0; ch < numChannels; ++ch)
                {
                    auto& x = io[ch][start + n];
                    x = eq.processSample (ch, x * g);
                }
            }

            for (int ch = 0; ch < numChannels; ++ch)
                dryBuffer.copyFrom (ch, 0, buffer, ch, start, len);

            for (int n = 0; n < len; ++n)
            {
                const auto gain = comp.processFrame (readOnly, start + n) * makeup;

                for (int ch = 0; ch < numChannels; ++ch)
                {
                    const auto dry = dryBuffer.getReadPointer (ch)[n];
                    const auto wet = io[ch][start + n] * gain;
                    io[ch][start + n] = dry + mix * (wet - dry);
                }
            }
        }
    }
    else
    {
        // Keep the smoothers in step so un-bypassing does not jump.
        for (auto* s : { &smTrim, &smCompMix, &smLowCut, &smLowGain, &smLowFreq,
                         &smMidGain, &smMidFreq, &smMidQ, &smHighGain, &smHighFreq,
                         &smThresh, &smRatio, &smAttack, &smRelease })
            s->skip (numSamples);
    }

    const auto driveAmount = smDrive.skip (numSamples);

    // ---- valve stage ------------------------------------------------------
    // The dry path is pushed through the same up/down conversion even when the
    // strip is bypassed, so the reported latency stays valid either way.
    if (auto* os = oversamplers[(size_t) activeQuality].get())
    {
        juce::dsp::AudioBlock<float> block (io, (size_t) numChannels, (size_t) numSamples);
        auto upsampled = os->processSamplesUp (block);

        if (! bypassed)
        {
            auto& stage = tube[(size_t) activeQuality];
            stage.setModel ((int) p.valve->load());
            stage.setDrive (driveAmount);

            const auto n = (int) upsampled.getNumSamples();
            float* ptr[dsp::TubeStage::kMaxChannels] {};

            for (int ch = 0; ch < numChannels; ++ch)
                ptr[ch] = upsampled.getChannelPointer ((size_t) ch);

            for (int i = 0; i < n; ++i)
                stage.processFrame (ptr, i, numChannels);
        }

        os->processSamplesDown (block);
    }

    // ---- output gain and metering ----------------------------------------
    if (! bypassed)
    {
        for (int n = 0; n < numSamples; ++n)
        {
            const auto g = smOutput.getNextValue();

            for (int ch = 0; ch < numChannels; ++ch)
                io[ch][n] *= g;
        }
    }
    else
    {
        smOutput.skip (numSamples);
    }

    for (int n = 0; n < numSamples; ++n)
    {
        auto m = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
            m = juce::jmax (m, std::abs (io[ch][n]));

        vuOut.push (m);
    }

    meterInputVu   .store (vuIn.getVuDb());
    meterOutputVu  .store (vuOut.getVuDb());
    meterGainReduce.store (bypassed ? 0.0f : comp.getGainReductionDb());

    // The valve glows with drive, modulated a little by how hard it is being hit.
    const auto heatLevel = juce::jlimit (0.0f, 1.0f, (vuOut.getVuDb() + 24.0f) / 30.0f);
    meterValveHeat.store (bypassed ? 0.0f : driveAmount * (0.35f + 0.65f * heatLevel));
}

dsp::EqSettings KilocycleProcessor::currentEqSettings() const noexcept
{
    dsp::EqSettings s;
    s.lowCutHz   = p.lowCut->load();
    s.lowGainDb  = p.lowGain->load();
    s.lowFreq    = p.lowFreq->load();
    s.midGainDb  = p.midGain->load();
    s.midFreq    = p.midFreq->load();
    s.midQ       = p.midQ->load();
    s.highGainDb = p.highGain->load();
    s.highFreq   = p.highFreq->load();
    return s;
}

juce::AudioProcessorEditor* KilocycleProcessor::createEditor()
{
    return new KilocycleEditor (*this);
}

void KilocycleProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void KilocycleProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

} // namespace kc

// -----------------------------------------------------------------------------
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new kc::KilocycleProcessor();
}
