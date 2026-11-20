#pragma once

#include "PluginProcessor.h"

namespace TQColours
{
    const juce::Colour bg        { 0xff0a0a0c };
    const juce::Colour panel     { 0xff131317 };
    const juce::Colour panelHi   { 0xff1c1c22 };
    const juce::Colour gold      { 0xfff2b824 };
    const juce::Colour goldDim   { 0xff8a6a14 };
    const juce::Colour blue      { 0xff3b82f6 };
    const juce::Colour text      { 0xffe8e8ea };
    const juce::Colour textDim   { 0xff7c7c86 };
}

//==============================================================================
class TerminatorLookAndFeel : public juce::LookAndFeel_V4
{
public:
    TerminatorLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&,
                               bool highlighted, bool down) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
};

//==============================================================================
class EQDisplay : public juce::Component, private juce::Timer
{
public:
    explicit EQDisplay (TerminatorQAudioProcessor&, juce::ComboBox& presetBox);
    ~EQDisplay() override { stopTimer(); }

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    void timerCallback() override { repaint(); }

    juce::Rectangle<float> plot() const;
    float xForFreq (float f) const;
    float freqForX (float x) const;
    float yForGain (float g) const;
    float gainForY (float y) const;
    float dbToY (float db) const;
    float amountFactor() const;
    float effectiveGain (int band) const;
    int   nodeAt (juce::Point<float> p) const;

    TerminatorQAudioProcessor& processor;
    juce::ComboBox& presets;
    int hovered = -1, dragging = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EQDisplay)
};

//==============================================================================
class TerminatorQAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit TerminatorQAudioProcessorEditor (TerminatorQAudioProcessor&);
    ~TerminatorQAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void stepPreset (int delta);

    TerminatorQAudioProcessor& processor;
    TerminatorLookAndFeel laf;

    juce::ComboBox presetBox;
    juce::TextButton prevButton { "<" }, nextButton { ">" };
    juce::TextButton analyzeButton { "ANALYZE" }, resetButton { "RESET EQ" };
    juce::ToggleButton bypassButton { "BYPASS" };
    juce::Slider amountKnob, outputKnob;
    juce::Label amountLabel, outputLabel;

    EQDisplay display;

    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    std::unique_ptr<SliderAtt> amountAtt, outputAtt;
    std::unique_ptr<ButtonAtt> bypassAtt;
    std::unique_ptr<ComboAtt>  presetAtt;

    bool analyzed = false;
    juce::String statusText { "READY" };
    juce::Rectangle<int> headerBounds, badgeBounds;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TerminatorQAudioProcessorEditor)
};
