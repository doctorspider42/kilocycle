/*
    Kilocycle - retro broadcast console channel strip
    Copyright (C) 2026  Kilocycle Audio
    Licensed under the GNU General Public License v3 or later. See LICENSE.

    Developer tool. Two jobs, neither of which needs a host or a window:

      kilocycle-devtool check               - offline DSP smoke test
      kilocycle-devtool shot <png> [valve]  - render the panel to a PNG
      kilocycle-devtool icon <png> [size]   - render the application icon to a PNG

    Build with -DKILOCYCLE_BUILD_TOOLS=ON.
*/

#include <juce_gui_basics/juce_gui_basics.h>

#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include "../Source/gui/Icon.h"

namespace
{

void setParam (kc::KilocycleProcessor& proc, const char* id, float value)
{
    if (auto* p = proc.getState().getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (value));
}

struct Stats
{
    float rms = 0.0f;
    float peak = 0.0f;
    bool  finite = true;
};

Stats analyse (const juce::AudioBuffer<float>& buffer)
{
    Stats s;
    double sum = 0.0;
    int count = 0;

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        const auto* d = buffer.getReadPointer (ch);

        for (int n = 0; n < buffer.getNumSamples(); ++n)
        {
            const auto x = d[n];

            if (! std::isfinite (x))
                s.finite = false;

            sum += (double) x * x;
            s.peak = juce::jmax (s.peak, std::abs (x));
            ++count;
        }
    }

    s.rms = count > 0 ? (float) std::sqrt (sum / count) : 0.0f;
    return s;
}

void fillSine (juce::AudioBuffer<float>& buffer, double sampleRate, double freq, float amplitude)
{
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* d = buffer.getWritePointer (ch);

        for (int n = 0; n < buffer.getNumSamples(); ++n)
            d[n] = amplitude * (float) std::sin (2.0 * juce::MathConstants<double>::pi * freq
                                                     * (double) n / sampleRate);
    }
}

/** Amplitude of one frequency component, by direct correlation. Exact as long as
    the window holds a whole number of periods. */
float magnitudeAt (const juce::AudioBuffer<float>& buffer, double sampleRate, double freq,
                   int startSample, int numSamples)
{
    double re = 0.0, im = 0.0;
    const auto* d = buffer.getReadPointer (0);

    for (int n = 0; n < numSamples; ++n)
    {
        const auto phase = 2.0 * juce::MathConstants<double>::pi * freq * (double) n / sampleRate;
        re += (double) d[startSample + n] * std::cos (phase);
        im += (double) d[startSample + n] * std::sin (phase);
    }

    return (float) (2.0 * std::sqrt (re * re + im * im) / (double) numSamples);
}

/** Runs a buffer through the processor in host-sized blocks. */
void runThrough (kc::KilocycleProcessor& proc, juce::AudioBuffer<float>& buffer, int blockSize)
{
    juce::MidiBuffer midi;

    for (int start = 0; start < buffer.getNumSamples(); start += blockSize)
    {
        const auto len = juce::jmin (blockSize, buffer.getNumSamples() - start);
        juce::AudioBuffer<float> slice (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), start, len);
        proc.processBlock (slice, midi);
    }
}

int runCheck()
{
    constexpr double sampleRate = 48000.0;
    constexpr int    blockSize  = 512;
    constexpr int    length     = 48000 * 2;

    kc::KilocycleProcessor proc;
    proc.setPlayConfigDetails (2, 2, sampleRate, blockSize);
    proc.prepareToPlay (sampleRate, blockSize);

    int failures = 0;
    auto expect = [&failures] (bool condition, const juce::String& what)
    {
        std::cout << (condition ? "  ok    " : "  FAIL  ") << what << std::endl;

        if (! condition)
            ++failures;
    };

    std::cout << "Kilocycle offline check @ " << sampleRate << " Hz" << std::endl;
    std::cout << "reported latency: " << proc.getLatencySamples() << " samples" << std::endl;

    // ---- 1. neutral settings should be close to transparent ---------------
    {
        setParam (proc, kc::pid::drive, 0.0f);
        setParam (proc, kc::pid::compThresh, 0.0f);
        setParam (proc, kc::pid::compRatio, 1.0f);
        setParam (proc, kc::pid::compAuto, 0.0f);
        proc.reset();

        juce::AudioBuffer<float> buffer (2, length);
        fillSine (buffer, sampleRate, 1000.0, 0.25f);
        const auto before = analyse (buffer);
        runThrough (proc, buffer, blockSize);
        const auto after = analyse (buffer);

        expect (after.finite, "output is finite at neutral settings");
        expect (std::abs (juce::Decibels::gainToDecibels (after.rms / before.rms)) < 0.7f,
                "neutral gain within 0.7 dB ("
                    + juce::String (juce::Decibels::gainToDecibels (after.rms / before.rms), 2) + " dB)");
    }

    // ---- 2. compressor actually compresses -------------------------------
    {
        setParam (proc, kc::pid::compThresh, -30.0f);
        setParam (proc, kc::pid::compRatio, 8.0f);
        setParam (proc, kc::pid::compAuto, 0.0f);
        proc.reset();

        juce::AudioBuffer<float> buffer (2, length);
        fillSine (buffer, sampleRate, 200.0, 0.5f);
        runThrough (proc, buffer, blockSize);

        const auto gr = proc.meterGainReduce.load();
        std::cout << "  gain reduction: " << gr << " dB" << std::endl;
        expect (gr > 6.0f, "8:1 at -30 dB gives more than 6 dB of reduction");
        expect (analyse (buffer).finite, "compressed output is finite");
    }

    // ---- 3. valve stage: harmonics without runaway level ------------------
    {
        setParam (proc, kc::pid::compThresh, 0.0f);
        setParam (proc, kc::pid::compRatio, 1.0f);
        setParam (proc, kc::pid::drive, 100.0f);
        proc.reset();

        juce::AudioBuffer<float> buffer (2, length);
        fillSine (buffer, sampleRate, 440.0, 0.5f);
        const auto before = analyse (buffer);
        runThrough (proc, buffer, blockSize);
        const auto after = analyse (buffer);

        const auto deltaDb = juce::Decibels::gainToDecibels (after.rms / before.rms);
        std::cout << "  full drive level change: " << deltaDb << " dB" << std::endl;
        expect (after.finite, "driven output is finite");
        expect (std::abs (deltaDb) < 6.0f, "full drive stays within 6 dB of unity");
        expect (after.peak < 1.6f, "full drive does not blow up the peak level");
    }

    // ---- 4. bypass is sample-accurate apart from the reported latency -----
    {
        setParam (proc, kc::pid::bypass, 1.0f);
        proc.reset();

        const auto latency = proc.getLatencySamples();

        juce::AudioBuffer<float> buffer (2, length);
        fillSine (buffer, sampleRate, 700.0, 0.4f);
        juce::AudioBuffer<float> reference;
        reference.makeCopyOf (buffer);
        runThrough (proc, buffer, blockSize);

        auto worst = 0.0f;
        for (int n = 4096; n < length - latency - 8; ++n)
            worst = juce::jmax (worst, std::abs (buffer.getSample (0, n + latency) - reference.getSample (0, n)));

        std::cout << "  bypass error: " << juce::Decibels::gainToDecibels (juce::jmax (1.0e-9f, worst))
                  << " dBFS" << std::endl;
        expect (worst < 0.02f, "bypass passes audio through, delayed by the reported latency");

        setParam (proc, kc::pid::bypass, 0.0f);
    }

    // ---- 5. silence in, silence out ---------------------------------------
    {
        setParam (proc, kc::pid::drive, 50.0f);
        setParam (proc, kc::pid::compAuto, 1.0f);
        proc.reset();

        juce::AudioBuffer<float> buffer (2, length);
        buffer.clear();
        runThrough (proc, buffer, blockSize);
        const auto after = analyse (buffer);

        expect (after.finite && after.peak < 1.0e-5f, "silence stays silent");
    }

    // ---- 6. switching oversampling quality mid-stream is safe -------------
    {
        setParam (proc, kc::pid::drive, 60.0f);
        proc.reset();

        juce::AudioBuffer<float> buffer (2, length);
        fillSine (buffer, sampleRate, 330.0, 0.4f);
        juce::MidiBuffer midi;
        auto finite = true;

        for (int start = 0; start < length; start += blockSize)
        {
            const auto len = juce::jmin (blockSize, length - start);
            setParam (proc, kc::pid::quality, (start / blockSize) % 2 == 0 ? 0.0f : 1.0f);

            juce::AudioBuffer<float> slice (buffer.getArrayOfWritePointers(), 2, start, len);
            proc.processBlock (slice, midi);
            finite = finite && analyse (slice).finite;
        }

        expect (finite, "switching quality mid-stream stays finite");
        setParam (proc, kc::pid::quality, 1.0f);
    }

    // ---- 7. every valve behaves, and they agree at Drive 0 ----------------
    {
        setParam (proc, kc::pid::compThresh, 0.0f);
        setParam (proc, kc::pid::compRatio, 1.0f);
        setParam (proc, kc::pid::compAuto, 0.0f);

        // 500 Hz at 48 kHz is exactly 96 samples per period, so the harmonic
        // correlation below needs no windowing.
        constexpr double tone = 500.0;

        juce::AudioBuffer<float> scratch (2, length);

        auto measure = [&] (int valve, float driveAmount)
        {
            setParam (proc, kc::pid::valve, (float) valve);
            setParam (proc, kc::pid::drive, driveAmount);
            proc.reset();

            fillSine (scratch, sampleRate, tone, 0.5f);
            runThrough (proc, scratch, blockSize);
            return analyse (scratch);
        };

        std::cout << "  valve survey at full drive:" << std::endl;
        std::cout << "    valve\tlevel\t2nd\t3rd\t4th\tpeak" << std::endl;

        auto allFinite = true;
        auto worstLevel = 0.0f;
        const auto reference = 0.5f / std::sqrt (2.0f);

        for (int v = 0; v < kc::dsp::numValveModels; ++v)
        {
            const auto s = measure (v, 100.0f);
            const auto deltaDb = juce::Decibels::gainToDecibels (s.rms / reference);

            // Analyse the tail, once every envelope has settled.
            const auto window = 24000;   // 250 whole periods
            const auto start = length - window;
            const auto h1 = magnitudeAt (scratch, sampleRate, tone,       start, window);

            auto harmonicDb = [&] (int n)
            {
                const auto h = magnitudeAt (scratch, sampleRate, tone * n, start, window);
                return juce::Decibels::gainToDecibels (juce::jmax (1.0e-7f, h / juce::jmax (1.0e-7f, h1)));
            };

            std::cout << "    " << kc::dsp::valveModel (v).code << "\t"
                      << juce::String (deltaDb, 2) << " dB\t"
                      << juce::String (harmonicDb (2), 1) << "\t"
                      << juce::String (harmonicDb (3), 1) << "\t"
                      << juce::String (harmonicDb (4), 1) << "\t"
                      << juce::String (s.peak, 3) << std::endl;

            allFinite = allFinite && s.finite;
            worstLevel = juce::jmax (worstLevel, std::abs (deltaDb));
        }

        expect (allFinite, "all valves produce finite output at full drive");
        expect (worstLevel < 6.0f, "no valve is more than 6 dB off unity at full drive");

        // The stage is only allowed to have a character when it is being driven.
        auto spread = 0.0f;
        auto first = 0.0f;

        for (int v = 0; v < kc::dsp::numValveModels; ++v)
        {
            const auto rms = measure (v, 0.0f).rms;

            if (v == 0)
                first = rms;
            else
                spread = juce::jmax (spread, std::abs (juce::Decibels::gainToDecibels (rms / first)));
        }

        std::cout << "  spread between valves at Drive 0: "
                  << juce::String (spread, 4) << " dB" << std::endl;
        expect (spread < 0.02f, "all valves are identical at Drive 0");

        setParam (proc, kc::pid::valve, 0.0f);
    }

    // ---- CPU cost ---------------------------------------------------------
    {
        constexpr double seconds = 60.0;
        const auto totalSamples = (int) (seconds * sampleRate);

        setParam (proc, kc::pid::drive, 50.0f);
        setParam (proc, kc::pid::compThresh, -20.0f);
        setParam (proc, kc::pid::compRatio, 4.0f);
        proc.reset();

        juce::AudioBuffer<float> buffer (2, blockSize);
        fillSine (buffer, sampleRate, 500.0, 0.3f);

        juce::MidiBuffer midi;
        const auto start = juce::Time::getHighResolutionTicks();

        for (int done = 0; done < totalSamples; done += blockSize)
            proc.processBlock (buffer, midi);

        const auto elapsed = juce::Time::highResolutionTicksToSeconds (
                                 juce::Time::getHighResolutionTicks() - start);

        std::cout << "\n" << seconds << " s of stereo audio processed in "
                  << juce::String (elapsed, 3) << " s  ("
                  << juce::String (elapsed / seconds * 100.0, 2)
                  << " % of one core at " << sampleRate << " Hz, Fine 4x)" << std::endl;
    }

    std::cout << (failures == 0 ? "\nall checks passed" : "\n" + juce::String (failures) + " check(s) failed")
              << std::endl;

    return failures == 0 ? 0 : 1;
}

int writePng (const juce::Image& image, const juce::File& destination)
{
    if (image.isNull())
    {
        std::cerr << "nothing to write" << std::endl;
        return 1;
    }

    destination.deleteFile();

    if (auto stream = destination.createOutputStream())
    {
        juce::PNGImageFormat png;

        if (png.writeImageToStream (image, *stream))
        {
            std::cout << "wrote " << destination.getFullPathName() << " ("
                      << image.getWidth() << "x" << image.getHeight() << ")" << std::endl;
            return 0;
        }
    }

    std::cerr << "could not write " << destination.getFullPathName() << std::endl;
    return 1;
}

int renderShot (const juce::File& destination, int valveIndex)
{
    kc::KilocycleProcessor proc;
    proc.setPlayConfigDetails (2, 2, 48000.0, 512);
    proc.prepareToPlay (48000.0, 512);

    // A musical-looking setting, so the screenshot shows the panel doing something.
    setParam (proc, kc::pid::trim,        4.0f);
    setParam (proc, kc::pid::lowCut,      65.0f);
    setParam (proc, kc::pid::lowGain,     3.5f);
    setParam (proc, kc::pid::lowFreq,     95.0f);
    setParam (proc, kc::pid::midGain,    -4.0f);
    setParam (proc, kc::pid::midFreq,     620.0f);
    setParam (proc, kc::pid::midQ,        1.4f);
    setParam (proc, kc::pid::highGain,    4.5f);
    setParam (proc, kc::pid::highFreq,    7200.0f);
    setParam (proc, kc::pid::compThresh, -18.0f);
    setParam (proc, kc::pid::compRatio,   3.5f);
    setParam (proc, kc::pid::compAttack,  12.0f);
    setParam (proc, kc::pid::compRelease, 220.0f);
    setParam (proc, kc::pid::compMix,     85.0f);
    setParam (proc, kc::pid::drive,       38.0f);
    setParam (proc, kc::pid::output,      -1.5f);

    // Optional third argument picks the valve, so the renderer can show any bottle.
    if (valveIndex >= 0)
        setParam (proc, kc::pid::valve, (float) valveIndex);

    // Push some programme material through so the meters have something to show.
    {
        juce::AudioBuffer<float> buffer (2, 48000);
        fillSine (buffer, 48000.0, 220.0, 0.035f);
        runThrough (proc, buffer, 512);
    }

    std::unique_ptr<juce::AudioProcessorEditor> editor (proc.createEditor());

    if (editor == nullptr)
    {
        std::cerr << "no editor" << std::endl;
        return 1;
    }

    editor->setSize (1080, 612);

    return writePng (editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f), destination);
}

/** Renders the application icon. This is how docs/icon.png and docs/icon-small.png
    are produced - CMake hands both to JUCE, so the icon is generated from the same
    code as the panel rather than maintained as artwork on the side. */
int renderIcon (const juce::File& destination, int size)
{
    return writePng (kc::gui::renderIcon (size), destination);
}

} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    const juce::String command = argc > 1 ? juce::String (argv[1]) : "check";

    if (command == "check")
        return runCheck();

    if (command == "shot")
        return renderShot (juce::File::getCurrentWorkingDirectory()
                               .getChildFile (argc > 2 ? juce::String (argv[2]) : "panel.png"),
                           argc > 3 ? juce::String (argv[3]).getIntValue() : -1);

    if (command == "icon")
        return renderIcon (juce::File::getCurrentWorkingDirectory()
                               .getChildFile (argc > 2 ? juce::String (argv[2]) : "icon.png"),
                           argc > 3 ? juce::String (argv[3]).getIntValue() : 1024);

    std::cerr << "usage: kilocycle-devtool [check | shot <file.png> [valve index] | icon <file.png> [size]]"
              << std::endl;
    return 2;
}
