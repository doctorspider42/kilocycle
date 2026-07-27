/*
    Kilocycle - retro broadcast console channel strip
    Copyright (C) 2026  Kilocycle Audio
    Licensed under the GNU General Public License v3 or later. See LICENSE.
*/

#include "PluginEditor.h"

namespace kc
{

using juce::Graphics;
using juce::Rectangle;

// =============================================================================
//  Backdrop
// =============================================================================

Backdrop::Backdrop()
{
    setOpaque (true);
    setInterceptsMouseClicks (false, false);
}

void Backdrop::setLayout (const PanelLayout& newLayout)
{
    layout = newLayout;
    cacheImage = {};
    repaint();
}

void Backdrop::paint (Graphics& g)
{
    // Rendered once at the window's physical resolution: sharp when the editor is
    // scaled up, and cheap for every subsequent repaint underneath the controls.
    const auto scale = juce::jlimit (1.0f, 3.0f,
                                     (float) g.getInternalContext().getPhysicalPixelScaleFactor());

    const auto wantedW = juce::jmax (1, juce::roundToInt ((float) getWidth()  * scale));
    const auto wantedH = juce::jmax (1, juce::roundToInt ((float) getHeight() * scale));

    if (cacheImage.isNull() || cacheImage.getWidth() != wantedW || cacheImage.getHeight() != wantedH)
    {
        cacheImage = juce::Image (juce::Image::ARGB, wantedW, wantedH, true);

        Graphics ig (cacheImage);
        ig.addTransform (juce::AffineTransform::scale (scale));
        paintPanel (ig);
    }

    g.drawImage (cacheImage, getLocalBounds().toFloat(), juce::RectanglePlacement::stretchToFit);
}

void Backdrop::paintPanel (Graphics& g)
{
    const auto full = getLocalBounds().toFloat();

    // Opaque base so the rounded bezel corners have something behind them.
    g.fillAll (skin::colour::walnutDeep);

    skin::paintWoodBezel (g, full.reduced (1.0f), 10.0f);
    skin::paintFaceplate (g, layout.faceplate, 7.0f);

    // ---- corner screws ----------------------------------------------------
    {
        const auto inner = layout.faceplate.reduced (9.0f);
        const float angles[] { 0.5f, -0.7f, 1.1f, -0.3f };
        const juce::Point<float> corners[]
        {
            { inner.getX(),     inner.getY() },
            { inner.getRight(), inner.getY() },
            { inner.getX(),     inner.getBottom() },
            { inner.getRight(), inner.getBottom() }
        };

        for (int i = 0; i < 4; ++i)
            skin::paintScrew (g, corners[i], 4.6f, angles[i]);
    }

    // ---- nameplate --------------------------------------------------------
    {
        const auto plate = layout.nameplate;
        skin::paintBrassPlate (g, plate, 4.0f);

        auto text = plate.reduced (8.0f, 4.0f);
        const auto title = text.removeFromTop (text.getHeight() * 0.62f);

        g.setColour (skin::colour::brassDark.withAlpha (0.35f));
        skin::drawTracked (g, "KILOCYCLE", skin::serifFont (title.getHeight() * 0.86f, true),
                           title.translated (1.0f, 1.4f), 6.0f, juce::Justification::centred);

        g.setColour (juce::Colour (0xff41300a));
        skin::drawTracked (g, "KILOCYCLE", skin::serifFont (title.getHeight() * 0.86f, true),
                           title, 6.0f, juce::Justification::centred);

        g.setColour (juce::Colour (0xff5c4512));
        skin::drawTracked (g, "BROADCAST  CHANNEL  AMPLIFIER",
                           skin::legendFont (text.getHeight() * 0.62f, true),
                           text, 1.7f, juce::Justification::centred);
    }

    // ---- header legends ---------------------------------------------------
    {
        auto area = layout.headerCentre;
        const auto line1 = area.removeFromTop (area.getHeight() * 0.55f);

        skin::drawEngraved (g, "TYPE 45", skin::serifFont (17.0f, true), line1,
                            juce::Justification::centred, skin::colour::ink, 4.0f);

        skin::drawEngraved (g, skin::u8 ("VALVE  TONE  \xc2\xb7  LEVELLING  AMPLIFIER  \xc2\xb7  0 VU = -18 dBFS"),
                            skin::legendFont (9.5f, true), area,
                            juce::Justification::centred, skin::colour::inkSoft, 1.5f);
    }

    skin::drawEngraved (g, "BYPASS", skin::legendFont (10.0f, true), layout.bypassLegend,
                        juce::Justification::centredRight, skin::colour::ink, 1.4f);

    // ---- section frames ---------------------------------------------------
    skin::paintSectionFrame (g, layout.input,    "INPUT");
    skin::paintSectionFrame (g, layout.tone,     "TONE");
    skin::paintSectionFrame (g, layout.leveller, "LEVELLER");
    skin::paintSectionFrame (g, layout.valve,    skin::u8 ("VALVE  \xc2\xb7  OUTPUT"));

    // ---- the note in the spare tone cell ----------------------------------
    {
        auto note = layout.toneNote;
        auto top = note.removeFromTop (note.getHeight() * 0.5f);

        g.setColour (skin::colour::inkSoft.withAlpha (0.8f));
        skin::drawTracked (g, "SHELVING", skin::legendFont (9.0f, true),
                           top.removeFromBottom (14.0f), 1.4f, juce::Justification::centred);
        skin::drawTracked (g, skin::u8 ("\xc2\xb1 15 dB"), skin::legendFont (9.0f), note.removeFromTop (14.0f),
                           1.2f, juce::Justification::centred);
    }

    // ---- footer -----------------------------------------------------------
    {
        const auto font = skin::legendFont (9.0f, true);
        g.setColour (skin::colour::inkSoft.withAlpha (0.85f));

        skin::drawTracked (g, skin::u8 ("FREE SOFTWARE  \xc2\xb7  GNU GPL v3 OR LATER"), font,
                           layout.footer, 1.3f, juce::Justification::left);
        skin::drawTracked (g, "DRAG THE CURSOR ON THE DIAL TO TUNE THE MID BAND", font,
                           layout.footer, 1.3f, juce::Justification::centred);
        // Development builds carry the commit they came from, so a bug report can
        // always name the exact binary.
        auto version = juce::String ("v") + JucePlugin_VersionString;

       #ifdef KILOCYCLE_BUILD_ID
        version << skin::u8 ("  \xc2\xb7  ") << KILOCYCLE_BUILD_ID;
       #endif

        skin::drawTracked (g, version, font, layout.footer, 1.3f, juce::Justification::right);
    }
}

// =============================================================================
//  Editor
// =============================================================================

KilocycleEditor::KilocycleEditor (KilocycleProcessor& p)
    : juce::AudioProcessorEditor (p),
      processor (p),
      dial (*p.getState().getParameter (pid::midFreq)),
      vu ([&p] { return p.meterOutputVu.load(); }),
      valve (*p.getState().getParameter (pid::valve),
             [&p] { return p.meterValveHeat.load(); }),
      grMeter ([&p] { return p.meterGainReduce.load(); }),
      trim      (p.getState(), pid::trim,        "Trim"),
      lowCut    (p.getState(), pid::lowCut,      "Low Cut"),
      lowGain   (p.getState(), pid::lowGain,     "Low"),
      midGain   (p.getState(), pid::midGain,     "Mid"),
      highGain  (p.getState(), pid::highGain,    "High"),
      midQ      (p.getState(), pid::midQ,        "Width", 9),
      lowFreq   (p.getState(), pid::lowFreq,     "Low Freq"),
      midFreq   (p.getState(), pid::midFreq,     "Mid Freq"),
      highFreq  (p.getState(), pid::highFreq,    "High Freq"),
      threshold (p.getState(), pid::compThresh,  "Threshold"),
      ratio     (p.getState(), pid::compRatio,   "Ratio", 9),
      attack    (p.getState(), pid::compAttack,  "Attack", 9),
      release   (p.getState(), pid::compRelease, "Release", 9),
      compMix   (p.getState(), pid::compMix,     "Mix", 9),
      drive     (p.getState(), pid::drive,       "Drive"),
      output    (p.getState(), pid::output,      "Output"),
      quality   (p.getState(), pid::quality,     "Quality", 2, true)
{
    setLookAndFeel (&lookAndFeel);

    addAndMakeVisible (content);
    content.addAndMakeVisible (backdrop);

    for (auto* c : std::initializer_list<juce::Component*> {
             &dial, &vu, &valve, &grMeter,
             &trim, &lowCut, &lowGain, &midGain, &highGain, &midQ,
             &lowFreq, &midFreq, &highFreq,
             &threshold, &ratio, &attack, &release, &compMix,
             &drive, &output, &quality,
             &bypassSwitch, &autoSwitch })
        content.addAndMakeVisible (c);

    bypassSwitch.setLampInverted (true);
    bypassSwitch.setLampColour (juce::Colour (0xffff7a45));
    bypassSwitch.setTooltip ("Bypass the whole strip");

    autoSwitch.setLampColour (skin::colour::amber);
    autoSwitch.setTooltip ("Automatic make-up gain");

    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        p.getState(), pid::bypass, bypassSwitch);
    autoAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        p.getState(), pid::compAuto, autoSwitch);

    quality.setTextHeights (13.0f, 14.0f);

    // Show the real curve straight away rather than waiting for the first tick.
    dial.setEqSettings (processor.currentEqSettings(), processor.displaySampleRate());

    setResizable (true, true);
    setResizeLimits (kLogicalWidth * 3 / 5, kLogicalHeight * 3 / 5,
                     kLogicalWidth * 2,     kLogicalHeight * 2);

    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) kLogicalWidth / (double) kLogicalHeight);

    const auto savedWidth  = (int) processor.getState().state.getProperty ("uiWidth",  kLogicalWidth);
    const auto savedHeight = (int) processor.getState().state.getProperty ("uiHeight", kLogicalHeight);
    setSize (juce::jmax (400, savedWidth), juce::jmax (200, savedHeight));

    startTimerHz (20);
}

KilocycleEditor::~KilocycleEditor()
{
    setLookAndFeel (nullptr);
}

void KilocycleEditor::paint (Graphics& g)
{
    g.fillAll (skin::colour::walnutDeep);
}

void KilocycleEditor::timerCallback()
{
    dial.setEqSettings (processor.currentEqSettings(), processor.displaySampleRate());
}

void KilocycleEditor::resized()
{
    const auto scale = juce::jmin ((float) getWidth()  / (float) kLogicalWidth,
                                   (float) getHeight() / (float) kLogicalHeight);

    content.setTransform (juce::AffineTransform::scale (scale));
    content.setBounds (0, 0, kLogicalWidth, kLogicalHeight);

    layOutContent();

    processor.getState().state.setProperty ("uiWidth",  getWidth(),  nullptr);
    processor.getState().state.setProperty ("uiHeight", getHeight(), nullptr);
}

void KilocycleEditor::layOutContent()
{
    backdrop.setBounds (content.getLocalBounds());

    PanelLayout layout;

    const auto full = content.getLocalBounds().toFloat();
    layout.faceplate = full.reduced (16.0f);

    auto work = layout.faceplate.reduced (18.0f);

    // ---- header -----------------------------------------------------------
    {
        auto header = work.removeFromTop (58.0f);
        work.removeFromTop (12.0f);

        layout.nameplate = header.removeFromLeft (306.0f).withSizeKeepingCentre (306.0f, 46.0f);

        auto bypassArea = header.removeFromRight (146.0f);
        const auto rocker = bypassArea.removeFromRight (42.0f).withSizeKeepingCentre (42.0f, 56.0f);
        bypassSwitch.setBounds (rocker.toNearestInt());
        layout.bypassLegend = bypassArea.withTrimmedRight (8.0f);

        layout.headerCentre = header.reduced (12.0f, 6.0f);
    }

    // ---- dial and meter ---------------------------------------------------
    {
        auto row = work.removeFromTop (150.0f);
        work.removeFromTop (14.0f);

        dial.setBounds (row.removeFromLeft (752.0f).toNearestInt());
        row.removeFromLeft (16.0f);
        vu.setBounds (row.toNearestInt());
    }

    layout.footer = work.removeFromBottom (18.0f);
    work.removeFromBottom (4.0f);

    // ---- control sections -------------------------------------------------
    auto row = work;

    layout.input = row.removeFromLeft (150.0f);
    row.removeFromLeft (10.0f);
    layout.tone = row.removeFromLeft (330.0f);
    row.removeFromLeft (10.0f);
    layout.leveller = row.removeFromLeft (300.0f);
    row.removeFromLeft (10.0f);
    layout.valve = row;

    auto sectionInner = [] (juce::Rectangle<float> section)
    {
        return section.reduced (10.0f).withTrimmedTop (14.0f);
    };

    auto place = [] (gui::RadioKnob& knob, juce::Rectangle<float> cell)
    {
        knob.setBounds (cell.reduced (2.0f).toNearestInt());
    };

    // INPUT ------------------------------------------------------------------
    {
        auto area = sectionInner (layout.input);
        const auto rowH = area.getHeight() * 0.5f;
        place (trim,   area.removeFromTop (rowH));
        place (lowCut, area);
    }

    // TONE -------------------------------------------------------------------
    {
        auto area = sectionInner (layout.tone);
        const auto rowH = area.getHeight() * 0.5f;

        auto top = area.removeFromTop (rowH);
        auto bottom = area;
        const auto cellW = top.getWidth() * 0.25f;

        place (lowGain,  top.removeFromLeft (cellW));
        place (midGain,  top.removeFromLeft (cellW));
        place (highGain, top.removeFromLeft (cellW));
        place (midQ,     top);

        place (lowFreq,  bottom.removeFromLeft (cellW));
        place (midFreq,  bottom.removeFromLeft (cellW));
        place (highFreq, bottom.removeFromLeft (cellW));
        layout.toneNote = bottom;
    }

    // LEVELLER ---------------------------------------------------------------
    {
        auto area = sectionInner (layout.leveller);

        grMeter.setBounds (area.removeFromBottom (32.0f).toNearestInt());
        area.removeFromBottom (2.0f);

        const auto rowH = area.getHeight() * 0.5f;
        auto top = area.removeFromTop (rowH);
        auto bottom = area;
        const auto cellW = top.getWidth() / 3.0f;

        place (threshold, top.removeFromLeft (cellW));
        place (ratio,     top.removeFromLeft (cellW));
        place (attack,    top);

        place (release, bottom.removeFromLeft (cellW));
        place (compMix, bottom.removeFromLeft (cellW));
        autoSwitch.setBounds (bottom.withSizeKeepingCentre (34.0f, 50.0f).toNearestInt());
    }

    // VALVE / OUTPUT ---------------------------------------------------------
    {
        auto area = sectionInner (layout.valve);
        const auto rowH = area.getHeight() * 0.5f;

        auto top = area.removeFromTop (rowH);
        place (drive,  top.removeFromLeft (top.getWidth() * 0.5f));
        place (output, top);

        auto bottom = area;
        valve.setBounds (bottom.removeFromLeft (84.0f).reduced (4.0f, 2.0f).toNearestInt());
        place (quality, bottom);
    }

    backdrop.setLayout (layout);
}

} // namespace kc
