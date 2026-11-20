#include "PluginEditor.h"

using namespace TQColours;

static juce::Font mkFont (float size, bool bold = false)
{
        juce::Font f { juce::FontOptions { size } };
    return bold ? f.boldened() : f;
}

//==============================================================================
// Look & Feel
//==============================================================================
TerminatorLookAndFeel::TerminatorLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, bg);

    setColour (juce::TextButton::buttonColourId,   panelHi);
    setColour (juce::TextButton::buttonOnColourId, gold);
    setColour (juce::TextButton::textColourOffId,  text);
    setColour (juce::TextButton::textColourOnId,   juce::Colours::black);

    setColour (juce::ComboBox::backgroundColourId, panelHi);
    setColour (juce::ComboBox::textColourId,       text);
    setColour (juce::ComboBox::outlineColourId,    goldDim);
    setColour (juce::ComboBox::arrowColourId,      gold);

    setColour (juce::PopupMenu::backgroundColourId,            panel);
    setColour (juce::PopupMenu::textColourId,                  text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, gold);
    setColour (juce::PopupMenu::highlightedTextColourId,       juce::Colours::black);

    setColour (juce::Slider::textBoxTextColourId,    text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId,            textDim);
}

void TerminatorLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                              float startAngle, float endAngle, juce::Slider&)
{
    auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (6.0f);
    const float r  = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f;
    const auto  c  = b.getCentre();
    const float lw = 5.0f;
    const float angle = startAngle + pos * (endAngle - startAngle);

    juce::Path track;
    track.addCentredArc (c.x, c.y, r - lw * 0.5f, r - lw * 0.5f, 0.0f, startAngle, endAngle, true);
    g.setColour (juce::Colour (0xff2a2a31));
    g.strokePath (track, juce::PathStrokeType (lw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path value;
    value.addCentredArc (c.x, c.y, r - lw * 0.5f, r - lw * 0.5f, 0.0f, startAngle, angle, true);
    g.setColour (gold.withAlpha (0.25f));
    g.strokePath (value, juce::PathStrokeType (lw + 4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (gold);
    g.strokePath (value, juce::PathStrokeType (lw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float br = r - lw - 4.0f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2a2a31), c.x, c.y - br,
                                             juce::Colour (0xff121216), c.x, c.y + br, false));
    g.fillEllipse (c.x - br, c.y - br, br * 2.0f, br * 2.0f);

    juce::Path pointer;
    pointer.addRoundedRectangle (-1.5f, -br + 3.0f, 3.0f, br * 0.45f, 1.5f);
    g.setColour (gold);
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (c.x, c.y));
}

void TerminatorLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&,
                                                  bool highlighted, bool down)
{
    auto b = button.getLocalBounds().toFloat().reduced (0.5f);
    auto base = button.getToggleState() ? gold : panelHi;
    if (down)             base = base.darker (0.2f);
    else if (highlighted) base = base.brighter (0.12f);

    g.setColour (base);
    g.fillRoundedRectangle (b, 5.0f);
    g.setColour (goldDim);
    g.drawRoundedRectangle (b, 5.0f, 1.0f);
}

void TerminatorLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                                              bool highlighted, bool)
{
    auto b   = button.getLocalBounds().toFloat();
    auto box = b.removeFromLeft (24.0f).withSizeKeepingCentre (16.0f, 16.0f);

    g.setColour (highlighted ? panelHi.brighter (0.1f) : panelHi);
    g.fillRoundedRectangle (box, 3.0f);
    g.setColour (goldDim);
    g.drawRoundedRectangle (box, 3.0f, 1.0f);

    if (button.getToggleState())
    {
        g.setColour (gold);
        g.fillRoundedRectangle (box.reduced (4.0f), 2.0f);
    }

    g.setColour (text);
    g.setFont (mkFont (13.0f, true));
    g.drawText (button.getButtonText(), b.withTrimmedLeft (6.0f).toNearestInt(), juce::Justification::centredLeft);
}

//==============================================================================
// EQ display
//==============================================================================
EQDisplay::EQDisplay (TerminatorQAudioProcessor& p, juce::ComboBox& presetBox)
    : processor (p), presets (presetBox)
{
    startTimerHz (30);
}

juce::Rectangle<float> EQDisplay::plot() const
{
    return getLocalBounds().toFloat().reduced (10.0f, 12.0f).withTrimmedRight (30.0f).withTrimmedBottom (14.0f);
}

float EQDisplay::xForFreq (float f) const
{
    auto p = plot();
    return p.getX() + p.getWidth() * std::log (f / TQ::minHz) / std::log (TQ::maxHz / TQ::minHz);
}

float EQDisplay::freqForX (float x) const
{
    auto p = plot();
    const float t = (x - p.getX()) / p.getWidth();
    return TQ::minHz * std::exp (t * std::log (TQ::maxHz / TQ::minHz));
}

float EQDisplay::yForGain (float g) const
{
    auto p = plot();
    return juce::jmap (g, -10.0f, 10.0f, p.getBottom(), p.getY());
}

float EQDisplay::gainForY (float y) const
{
    auto p = plot();
    return juce::jmap (y, p.getBottom(), p.getY(), -10.0f, 10.0f);
}

float EQDisplay::dbToY (float db) const
{
    auto p = plot();
    return juce::jmap (juce::jlimit (-110.0f, -30.0f, db), -110.0f, -30.0f,
                       p.getBottom(), p.getY() + p.getHeight() * 0.12f);
}

float EQDisplay::amountFactor() const
{
    return processor.apvts.getRawParameterValue ("amount")->load() * 0.01f;
}

float EQDisplay::effectiveGain (int band) const
{
    return processor.apvts.getRawParameterValue ("g" + juce::String (band))->load() * amountFactor();
}

int EQDisplay::nodeAt (juce::Point<float> pt) const
{
    int best = -1;
    float bestDist = 14.0f;
    for (int i = 0; i < TQ::numBands; ++i)
    {
        juce::Point<float> n (xForFreq (TQ::bandFreqs[(size_t) i]), yForGain (effectiveGain (i)));
        const float d = n.getDistanceFrom (pt);
        if (d < bestDist) { bestDist = d; best = i; }
    }
    return best;
}

void EQDisplay::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    g.setColour (panel);
    g.fillRoundedRectangle (area, 8.0f);
    g.setColour (juce::Colour (0xff26262d));
    g.drawRoundedRectangle (area.reduced (0.5f), 8.0f, 1.0f);

    auto p = plot();

    // ---- rejilla
    static const float gridF[] = { 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000 };
    static const char* gridL[] = { "20", "50", "100", "200", "500", "1k", "2k", "5k", "10k", "20k" };
    g.setFont (mkFont (10.0f));
    for (int i = 0; i < 10; ++i)
    {
        const float x = xForFreq (gridF[i]);
        g.setColour (juce::Colour (0x14ffffff));
        g.drawVerticalLine ((int) x, p.getY(), p.getBottom());
        g.setColour (textDim);
        g.drawText (gridL[i], juce::Rectangle<float> (x - 15.0f, p.getBottom() + 2.0f, 30.0f, 12.0f).toNearestInt(),
                    juce::Justification::centred);
    }

    static const float gridG[] = { 9, 6, 3, 0, -3, -6, -9 };
    for (float gg : gridG)
    {
        const float y = yForGain (gg);
        g.setColour (gg == 0.0f ? juce::Colour (0x33ffffff) : juce::Colour (0x14ffffff));
        g.drawHorizontalLine ((int) y, p.getX(), p.getRight());
        g.setColour (textDim);
        g.drawText ((gg > 0 ? "+" : "") + juce::String ((int) gg),
                    juce::Rectangle<float> (p.getRight() + 3.0f, y - 7.0f, 26.0f, 14.0f).toNearestInt(),
                    juce::Justification::centredLeft);
    }

    g.saveState();
    g.reduceClipRegion (p.expanded (0.0f, 6.0f).toNearestInt());

    // ---- espectro en vivo
    {
        juce::Path fill, line;
        fill.startNewSubPath (p.getX(), p.getBottom());
        for (int b = 0; b < TQ::numLogBins; ++b)
        {
            const float x = xForFreq (TQ::logBinCentre (b));
            const float y = dbToY (processor.spectrumDb[(size_t) b].load());
            fill.lineTo (x, y);
            if (b == 0) line.startNewSubPath (x, y); else line.lineTo (x, y);
        }
        fill.lineTo (p.getRight(), p.getBottom());
        fill.closeSubPath();

        g.setGradientFill (juce::ColourGradient (gold.withAlpha (0.33f), 0.0f, p.getY(),
                                                 gold.withAlpha (0.02f), 0.0f, p.getBottom(), false));
        g.fillPath (fill);
        g.setColour (gold.withAlpha (0.55f));
        g.strokePath (line, juce::PathStrokeType (1.0f));
    }

    // ---- análisis promedio + target
    if (processor.hasAnalysis())
    {
        std::array<float, TQ::numLogBins> tgt {}, ana {};
        const int pi = juce::jmax (0, presets.getSelectedItemIndex());
        for (int b = 0; b < TQ::numLogBins; ++b)
        {
            tgt[(size_t) b] = TQ::targetDb (TQ::presets[pi], TQ::logBinCentre (b));
            ana[(size_t) b] = processor.avgDb[(size_t) b].load();
        }
        const float meanT = TQ::meanInRange (tgt), meanA = TQ::meanInRange (ana);

        juce::Path anaPath, tgtPath;
        for (int b = 0; b < TQ::numLogBins; ++b)
        {
            const float x = xForFreq (TQ::logBinCentre (b));
            const float ya = dbToY (ana[(size_t) b]);
            const float yt = dbToY (tgt[(size_t) b] - meanT + meanA);
            if (b == 0) { anaPath.startNewSubPath (x, ya); tgtPath.startNewSubPath (x, yt); }
            else        { anaPath.lineTo (x, ya);          tgtPath.lineTo (x, yt); }
        }

        g.setColour (blue.withAlpha (0.9f));
        g.strokePath (anaPath, juce::PathStrokeType (1.6f));

        juce::Path dashed;
        const float dashes[] = { 5.0f, 4.0f };
        juce::PathStrokeType (1.2f).createDashedStroke (dashed, tgtPath, dashes, 2);
        g.setColour (juce::Colours::white.withAlpha (0.65f));
        g.fillPath (dashed);
    }

    // ---- curva del EQ (respuesta real de los filtros)
    {
        const double sr = processor.getSampleRate() > 0.0 ? processor.getSampleRate() : 44100.0;
        std::array<juce::dsp::IIR::Coefficients<float>::Ptr, TQ::numBands> cs;
        for (int i = 0; i < TQ::numBands; ++i)
            cs[(size_t) i] = TQ::makeBandCoeffs (i, effectiveGain (i), sr);

        juce::Path curve;
        bool first = true;
        for (float x = p.getX(); x <= p.getRight(); x += 2.0f)
        {
            const double f = (double) freqForX (x);
            double mag = 1.0;
            for (auto& c : cs)
                mag *= c->getMagnitudeForFrequency (f, sr);

            const float db = juce::Decibels::gainToDecibels ((float) mag);
            const float y  = yForGain (juce::jlimit (-10.0f, 10.0f, db));
            if (first) { curve.startNewSubPath (x, y); first = false; }
            else         curve.lineTo (x, y);
        }

        g.setColour (gold.withAlpha (0.16f));
        g.strokePath (curve, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (gold);
        g.strokePath (curve, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // ---- nodos
    for (int i = 0; i < TQ::numBands; ++i)
    {
        const float x = xForFreq (TQ::bandFreqs[(size_t) i]);
        const float y = yForGain (effectiveGain (i));
        const float r = (i == hovered || i == dragging) ? 7.0f : 5.0f;

        g.setColour (gold.withAlpha (0.25f));
        g.fillEllipse (x - r - 3.0f, y - r - 3.0f, (r + 3.0f) * 2.0f, (r + 3.0f) * 2.0f);
        g.setColour (gold);
        g.fillEllipse (x - r, y - r, r * 2.0f, r * 2.0f);
        g.setColour (juce::Colours::black.withAlpha (0.8f));
        g.drawEllipse (x - r, y - r, r * 2.0f, r * 2.0f, 1.3f);
    }

    g.restoreState();
}

void EQDisplay::mouseMove (const juce::MouseEvent& e)
{
    const int h = nodeAt (e.position);
    if (h != hovered)
    {
        hovered = h;
        setMouseCursor (h >= 0 ? juce::MouseCursor::UpDownResizeCursor : juce::MouseCursor::NormalCursor);
    }
}

void EQDisplay::mouseDown (const juce::MouseEvent& e)
{
    dragging = nodeAt (e.position);
    if (dragging >= 0)
        if (auto* prm = processor.apvts.getParameter ("g" + juce::String (dragging)))
            prm->beginChangeGesture();
}

void EQDisplay::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging < 0) return;

    const float eff    = juce::jlimit (-TQ::maxGainDb, TQ::maxGainDb, gainForY (e.position.y));
    const float factor = juce::jmax (amountFactor(), 0.05f);
    const float stored = juce::jlimit (-TQ::maxGainDb, TQ::maxGainDb, eff / factor);

    if (auto* prm = processor.apvts.getParameter ("g" + juce::String (dragging)))
        prm->setValueNotifyingHost (prm->convertTo0to1 (stored));
}

void EQDisplay::mouseUp (const juce::MouseEvent&)
{
    if (dragging >= 0)
        if (auto* prm = processor.apvts.getParameter ("g" + juce::String (dragging)))
            prm->endChangeGesture();
    dragging = -1;
}

void EQDisplay::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int idx = nodeAt (e.position);
    if (idx >= 0)
        processor.setBandGain (idx, 0.0f);
}

//==============================================================================
// Editor
//==============================================================================
TerminatorQAudioProcessorEditor::TerminatorQAudioProcessorEditor (TerminatorQAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processor (p), display (p, presetBox)
{
    setLookAndFeel (&laf);
    setSize (900, 540);

    for (int i = 0; i < TQ::numPresets; ++i)
        presetBox.addItem (TQ::presets[i].name, i + 1);
    addAndMakeVisible (presetBox);
    addAndMakeVisible (prevButton);
    addAndMakeVisible (nextButton);
    addAndMakeVisible (analyzeButton);
    addAndMakeVisible (resetButton);
    addAndMakeVisible (bypassButton);
    addAndMakeVisible (display);

    auto setupKnob = [this] (juce::Slider& s, juce::Label& l, const juce::String& name, const juce::String& suffix)
    {
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
        s.setTextValueSuffix (suffix);
        s.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        addAndMakeVisible (s);

        l.setText (name, juce::dontSendNotification);
        l.setJustificationType (juce::Justification::centred);
        l.setFont (mkFont (12.0f, true));
        l.attachToComponent (&s, false);
    };
    setupKnob (amountKnob, amountLabel, "AMOUNT", " %");
    setupKnob (outputKnob, outputLabel, "OUTPUT", " dB");

    amountAtt = std::make_unique<SliderAtt> (p.apvts, "amount", amountKnob);
    outputAtt = std::make_unique<SliderAtt> (p.apvts, "output", outputKnob);
    bypassAtt = std::make_unique<ButtonAtt> (p.apvts, "bypass", bypassButton);
    presetAtt = std::make_unique<ComboAtt>  (p.apvts, "preset", presetBox);

    prevButton.onClick = [this] { stepPreset (-1); };
    nextButton.onClick = [this] { stepPreset (+1); };

    presetBox.onChange = [this]
    {
        if (processor.hasAnalysis() && ! processor.isAnalyzing())
        {
            if (processor.computeMatch (presetBox.getSelectedItemIndex()))
            {
                analyzed = true;
                statusText = "ANALYZED";
            }
        }
    };

    analyzeButton.onClick = [this]
    {
        if (! processor.isAnalyzing())
        {
            processor.startAnalysis();
            analyzed = false;
            statusText = "ANALYZING";
        }
        else
        {
            processor.stopAnalysis();
            if (processor.computeMatch (presetBox.getSelectedItemIndex()))
            {
                analyzed = true;
                statusText = "ANALYZED";
            }
            else
            {
                analyzed = false;
                statusText = "NO SIGNAL";
            }
        }
    };

    resetButton.onClick = [this]
    {
        processor.resetAllBands();
        analyzed = false;
        statusText = "READY";
    };

    if (processor.hasAnalysis())
    {
        analyzed = true;
        statusText = "ANALYZED";
    }

    startTimerHz (15);
}

TerminatorQAudioProcessorEditor::~TerminatorQAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void TerminatorQAudioProcessorEditor::stepPreset (int delta)
{
    const int n   = TQ::numPresets;
    const int idx = juce::jmax (0, presetBox.getSelectedItemIndex());
    presetBox.setSelectedItemIndex ((idx + delta + n) % n, juce::sendNotificationSync);
}

void TerminatorQAudioProcessorEditor::timerCallback()
{
    const bool a = processor.isAnalyzing();
    if (a) statusText = "ANALYZING";

    analyzeButton.setButtonText (a ? "STOP & MATCH" : "ANALYZE");
    analyzeButton.setToggleState (a, juce::dontSendNotification);
    repaint (badgeBounds);
}

void TerminatorQAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff131318), 0.0f, 0.0f,
                                             bg, 0.0f, (float) getHeight(), false));
    g.fillAll();

    // panel inferior
    auto bottom = getLocalBounds().removeFromBottom (150).reduced (12, 6);
    g.setColour (panel);
    g.fillRoundedRectangle (bottom.toFloat(), 8.0f);
    g.setColour (juce::Colour (0xff26262d));
    g.drawRoundedRectangle (bottom.toFloat().reduced (0.5f), 8.0f, 1.0f);

    // título
    g.setColour (gold);
    g.setFont (mkFont (26.0f, true));
    g.drawText ("TERMINATOR Q", headerBounds.withTrimmedLeft (22), juce::Justification::centredLeft);

    g.setColour (textDim);
    g.setFont (mkFont (11.0f, true));
    g.drawText ("VST3  |  10-BAND TARGET EQ", headerBounds.withTrimmedRight (22), juce::Justification::centredRight);

    // insignia de estado
    auto badge = badgeBounds.toFloat();
    g.setGradientFill (juce::ColourGradient (gold.withAlpha (0.28f), 0.0f, badge.getY(),
                                             gold.withAlpha (0.05f), 0.0f, badge.getBottom(), false));
    g.fillRoundedRectangle (badge, 6.0f);
    g.setColour (gold.withAlpha (0.8f));
    g.drawRoundedRectangle (badge.reduced (0.5f), 6.0f, 1.2f);

    g.setColour (statusText == "NO SIGNAL" ? juce::Colour (0xffff6b5e) : gold);
    g.setFont (mkFont (14.0f, true));
    g.drawText (statusText, badgeBounds, juce::Justification::centred);
}

void TerminatorQAudioProcessorEditor::resized()
{
    auto r = getLocalBounds();
    headerBounds = r.removeFromTop (56);
    badgeBounds  = juce::Rectangle<int> (0, 0, 200, 32).withCentre (headerBounds.getCentre());

    auto bottom = r.removeFromBottom (150).reduced (28, 16);
    display.setBounds (r.reduced (12, 4));

    auto left = bottom.removeFromLeft (390);
    auto row1 = left.removeFromTop (34);
    prevButton.setBounds (row1.removeFromLeft (34));
    nextButton.setBounds (row1.removeFromRight (34));
    presetBox.setBounds (row1.reduced (6, 0));

    left.removeFromTop (12);
    auto row2 = left.removeFromTop (40);
    analyzeButton.setBounds (row2.removeFromLeft (220));
    row2.removeFromLeft (10);
    resetButton.setBounds (row2.removeFromLeft (120));

    left.removeFromTop (10);
    bypassButton.setBounds (left.removeFromTop (26).removeFromLeft (120));

    auto knobs = bottom.removeFromRight (320);
    amountKnob.setBounds (knobs.removeFromLeft (160).withTrimmedTop (18));
    outputKnob.setBounds (knobs.withTrimmedTop (18));
}
