/*
    Kilocycle - retro broadcast console channel strip
    Copyright (C) 2026  Kilocycle Audio
    Licensed under the GNU General Public License v3 or later. See LICENSE.
*/

#include "TuningDial.h"

namespace kc::gui
{

using juce::ColourGradient;
using juce::Graphics;
using juce::Path;
using juce::Rectangle;

namespace
{
    struct ScaleMark { float hz; const char* label; bool major; };

    const ScaleMark scaleMarks[]
    {
        {    20.0f, "20",  true  },
        {    30.0f, nullptr, false },
        {    50.0f, "50",  false },
        {    70.0f, nullptr, false },
        {   100.0f, "100", true  },
        {   150.0f, nullptr, false },
        {   200.0f, "200", false },
        {   300.0f, "300", false },
        {   500.0f, "500", true  },
        {   700.0f, nullptr, false },
        {  1000.0f, "1k",  true  },
        {  1500.0f, nullptr, false },
        {  2000.0f, "2k",  false },
        {  3000.0f, "3k",  false },
        {  5000.0f, "5k",  true  },
        {  7000.0f, nullptr, false },
        { 10000.0f, "10k", true  },
        { 15000.0f, nullptr, false },
        { 20000.0f, "20k", true  },
    };

    bool nearlyEqual (const dsp::EqSettings& a, const dsp::EqSettings& b)
    {
        auto same = [] (float x, float y) { return std::abs (x - y) < 1.0e-4f; };

        return same (a.lowCutHz, b.lowCutHz) && same (a.lowGainDb, b.lowGainDb)
            && same (a.lowFreq, b.lowFreq)   && same (a.midGainDb, b.midGainDb)
            && same (a.midFreq, b.midFreq)   && same (a.midQ, b.midQ)
            && same (a.highGainDb, b.highGainDb) && same (a.highFreq, b.highFreq);
    }
} // namespace

TuningDial::TuningDial (juce::RangedAudioParameter& midFrequencyParameter)
    : parameter (midFrequencyParameter)
{
    attachment = std::make_unique<juce::ParameterAttachment> (
        parameter,
        [this] (float newValue)
        {
            cursorHz = newValue;
            repaint();
        });

    attachment->sendInitialUpdate();
    setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
}

TuningDial::~TuningDial() = default;

void TuningDial::setEqSettings (const dsp::EqSettings& newSettings, double newSampleRate)
{
    if (nearlyEqual (settings, newSettings) && juce::approximatelyEqual (sampleRate, newSampleRate))
        return;

    settings   = newSettings;
    sampleRate = newSampleRate;
    repaint();
}

void TuningDial::resized()
{
    glass = getLocalBounds().toFloat().reduced (3.0f);
    trace = glass.reduced (10.0f, 8.0f).withTrimmedBottom (glass.getHeight() * 0.26f);
    scaleCache = {};
}

float TuningDial::frequencyForX (float x) const
{
    const auto t = juce::jlimit (0.0f, 1.0f, (x - trace.getX()) / juce::jmax (1.0f, trace.getWidth()));
    return kDisplayLowHz * std::pow (kDisplayHighHz / kDisplayLowHz, t);
}

float TuningDial::xForFrequency (float hz) const
{
    const auto t = std::log (juce::jmax (1.0f, hz) / kDisplayLowHz)
                     / std::log (kDisplayHighHz / kDisplayLowHz);
    return trace.getX() + juce::jlimit (0.0f, 1.0f, t) * trace.getWidth();
}

float TuningDial::yForDb (float db) const
{
    const auto t = juce::jlimit (-1.0f, 1.0f, db / kDisplayDb);
    return trace.getCentreY() - t * trace.getHeight() * 0.44f;
}

void TuningDial::setFrequencyFromMouse (const juce::MouseEvent& e)
{
    const auto range = parameter.getNormalisableRange();
    const auto hz = juce::jlimit (range.start, range.end, frequencyForX ((float) e.position.x));
    attachment->setValueAsPartOfGesture (hz);
}

void TuningDial::mouseDown (const juce::MouseEvent& e)
{
    dragging = true;
    attachment->beginGesture();
    setFrequencyFromMouse (e);
}

void TuningDial::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging)
        setFrequencyFromMouse (e);
}

void TuningDial::mouseUp (const juce::MouseEvent&)
{
    if (dragging)
    {
        dragging = false;
        attachment->endGesture();
    }
}

void TuningDial::mouseDoubleClick (const juce::MouseEvent&)
{
    const auto& range = parameter.getNormalisableRange();
    attachment->setValueAsCompleteGesture (range.convertFrom0to1 (parameter.getDefaultValue()));
}

void TuningDial::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    if (juce::approximatelyEqual (wheel.deltaY, 0.0f))
        return;

    const auto& range = parameter.getNormalisableRange();
    const auto factor = std::pow (2.0f, wheel.deltaY * 0.35f);
    attachment->setValueAsCompleteGesture (juce::jlimit (range.start, range.end, cursorHz * factor));
}

void TuningDial::rebuildScaleCache()
{
    constexpr int supersample = 2;
    scaleCache = juce::Image (juce::Image::ARGB,
                              juce::jmax (1, getWidth()  * supersample),
                              juce::jmax (1, getHeight() * supersample),
                              true);

    Graphics ig (scaleCache);
    ig.addTransform (juce::AffineTransform::scale ((float) supersample));
    paintScale (ig);
}

void TuningDial::paintScale (Graphics& g) const
{
    // ---- warm lamp behind the glass ---------------------------------------
    {
        Path clip;
        clip.addRoundedRectangle (glass, 4.0f);
        Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (clip);

        g.setGradientFill (ColourGradient (juce::Colour (0xff4a2a0d), glass.getCentreX(), glass.getBottom(),
                                           juce::Colour (0xff1c1108), glass.getCentreX(), glass.getY(), false));
        g.fillRect (glass);

        ColourGradient bloom (skin::colour::amber.withAlpha (0.30f),
                              glass.getCentreX(), glass.getBottom() - glass.getHeight() * 0.1f,
                              juce::Colours::transparentBlack,
                              glass.getCentreX(), glass.getY() - glass.getHeight() * 0.35f, true);
        g.setGradientFill (bloom);
        g.fillRect (glass);
    }

    // ---- graticule --------------------------------------------------------
    const auto scaleTop = trace.getBottom() + 2.0f;
    const auto scaleBottom = glass.getBottom() - 13.0f;
    const auto labelFont = skin::legendFont (9.5f, true);

    for (const auto& mark : scaleMarks)
    {
        const auto x = xForFrequency (mark.hz);

        // Vertical graticule line through the trace area.
        g.setColour (skin::colour::amber.withAlpha (mark.major ? 0.16f : 0.08f));
        g.drawLine (x, trace.getY(), x, trace.getBottom(), mark.major ? 0.9f : 0.6f);

        // Engraved tick on the dial scale.
        g.setColour (skin::colour::amberLite.withAlpha (mark.major ? 0.92f : 0.55f));
        g.drawLine (x, scaleTop, x,
                    scaleTop + (scaleBottom - scaleTop) * (mark.major ? 0.62f : 0.36f),
                    mark.major ? 1.5f : 1.0f);

        if (mark.label != nullptr)
        {
            g.setColour (skin::colour::amberLite.withAlpha (0.88f));
            g.setFont (labelFont);
            g.drawText (mark.label,
                        Rectangle<float> (34.0f, 11.0f).withCentre ({ x, scaleBottom - 1.0f }),
                        juce::Justification::centred, false);
        }
    }

    // 0 dB reference.
    g.setColour (skin::colour::amberLite.withAlpha (0.22f));
    for (auto x = trace.getX(); x < trace.getRight(); x += 5.0f)
        g.drawLine (x, trace.getCentreY(), x + 2.4f, trace.getCentreY(), 0.8f);

    // ---- legends ----------------------------------------------------------
    g.setColour (skin::colour::amberLite.withAlpha (0.55f));
    skin::drawTracked (g, "KILOCYCLES  PER  SECOND", skin::legendFont (8.0f, true),
                       Rectangle<float> (glass.getWidth(), 10.0f)
                           .withCentre ({ glass.getCentreX(), glass.getBottom() - 6.0f }),
                       1.8f, juce::Justification::centred);

    g.setColour (skin::colour::amberLite.withAlpha (0.35f));
    g.setFont (skin::legendFont (8.0f));
    g.drawText ("+20", Rectangle<float> (26.0f, 9.0f).withCentre ({ trace.getRight() - 12.0f, yForDb (20.0f) + 4.0f }),
                juce::Justification::centredRight, false);
    g.drawText ("-20", Rectangle<float> (26.0f, 9.0f).withCentre ({ trace.getRight() - 12.0f, yForDb (-20.0f) - 4.0f }),
                juce::Justification::centredRight, false);
}

void TuningDial::paint (Graphics& g)
{
    skin::paintRecessedWindow (g, glass, 4.0f);

    if (scaleCache.isNull())
        rebuildScaleCache();

    g.drawImage (scaleCache, getLocalBounds().toFloat(), juce::RectanglePlacement::stretchToFit);

    Path clip;
    clip.addRoundedRectangle (glass, 4.0f);

    {
        Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (clip);

        // ---- response trace ----------------------------------------------
        Path curve;
        const auto steps = juce::jmax (24, (int) trace.getWidth());

        for (int i = 0; i <= steps; ++i)
        {
            const auto x  = trace.getX() + trace.getWidth() * (float) i / (float) steps;
            const auto hz = frequencyForX (x);
            const auto db = (float) dsp::ConsoleEQ::magnitudeDbAt (sampleRate, settings, hz);
            const auto y  = yForDb (db);

            if (i == 0)
                curve.startNewSubPath (x, y);
            else
                curve.lineTo (x, y);
        }

        // Soft fill between the trace and the 0 dB line.
        {
            Path filled (curve);
            filled.lineTo (trace.getRight(), trace.getCentreY());
            filled.lineTo (trace.getX(), trace.getCentreY());
            filled.closeSubPath();

            g.setColour (skin::colour::amber.withAlpha (0.16f));
            g.fillPath (filled);
        }

        g.setColour (skin::colour::amber.withAlpha (0.30f));
        g.strokePath (curve, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved));

        g.setColour (skin::colour::amberLite);
        g.strokePath (curve, juce::PathStrokeType (1.7f, juce::PathStrokeType::curved));

        // ---- tuning cursor ------------------------------------------------
        const auto cx = xForFrequency (cursorHz);

        g.setGradientFill (ColourGradient (juce::Colour (0xffff5533).withAlpha (0.35f), cx, glass.getCentreY(),
                                           juce::Colours::transparentBlack, cx + 9.0f, glass.getCentreY(), false));
        g.fillRect (Rectangle<float> (cx - 9.0f, glass.getY(), 18.0f, glass.getHeight()));

        g.setColour (juce::Colour (0xffff6a44));
        g.drawLine (cx, glass.getY() + 3.0f, cx, glass.getBottom() - 3.0f, 1.6f);

        g.setColour (juce::Colour (0xffffd0b0));
        g.drawLine (cx, glass.getY() + 3.0f, cx, glass.getBottom() - 3.0f, 0.7f);

        // Cursor flag.
        Path flag;
        flag.addTriangle (cx - 5.0f, glass.getY() + 2.0f,
                          cx + 5.0f, glass.getY() + 2.0f,
                          cx,        glass.getY() + 10.0f);
        g.setColour (juce::Colour (0xffff6a44));
        g.fillPath (flag);

        // Reading, right next to the cursor.
        const auto text = cursorHz >= 1000.0f
                              ? juce::String (cursorHz / 1000.0f, 2) + " kc/s"
                              : juce::String (juce::roundToInt (cursorHz)) + " c/s";

        const auto font = skin::legendFont (10.5f, true);
        const auto textWidth = juce::GlyphArrangement::getStringWidth (font, text) + 10.0f;
        const auto onRight = cx + textWidth + 6.0f < glass.getRight();
        const auto box = Rectangle<float> (textWidth, 14.0f)
                             .withY (glass.getY() + 4.0f)
                             .withX (onRight ? cx + 5.0f : cx - textWidth - 5.0f);

        g.setColour (juce::Colour (0xcc140b04));
        g.fillRoundedRectangle (box, 2.5f);
        g.setColour (skin::colour::amberLite);
        g.setFont (font);
        g.drawText (text, box, juce::Justification::centred, false);
    }

    skin::paintGlassSheen (g, glass, 4.0f);
}

} // namespace kc::gui
