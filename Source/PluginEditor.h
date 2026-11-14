#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

// Draws the arc-style knobs from the web UI
class KnobLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h, float pos,
                          float startAngle, float endAngle, juce::Slider& s) override;
};

class Knob : public juce::Component
{
public:
    Knob(juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId, const juce::String& labelText,
         juce::Colour accent, std::function<juce::String(double)> formatter, bool large, KnobLookAndFeel& laf);
    void resized() override;
    int preferredWidth() const { return large ? 96 : 80; }

private:
    bool large;
    juce::Slider slider { juce::Slider::RotaryVerticalDrag, juce::Slider::NoTextBox };
    juce::Label name, value;
    std::function<juce::String(double)> fmt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

class SpectrumView : public juce::Component, private juce::Timer
{
public:
    explicit SpectrumView(StudioReverbAudioProcessor& p);
    ~SpectrumView() override { stopTimer(); }
    void paint(juce::Graphics& g) override;

private:
    void timerCallback() override;

    static constexpr int fftOrder = 8, fftSize = 1 << fftOrder, numBars = 32;
    StudioReverbAudioProcessor& proc;
    juce::dsp::FFT fft { fftOrder };
    juce::dsp::WindowingFunction<float> window { (size_t) fftSize, juce::dsp::WindowingFunction<float>::blackman };
    std::array<float, fftSize> ring {};
    std::array<float, fftSize * 2> fftData {};
    std::array<float, fftSize / 2> smoothed {};
    std::array<float, 4096> scratch {};
    int ringPos = 0;
};

class StudioReverbAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit StudioReverbAudioProcessorEditor(StudioReverbAudioProcessor&);
    ~StudioReverbAudioProcessorEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    StudioReverbAudioProcessor& proc;
    KnobLookAndFeel laf;
    SpectrumView spectrum;
    juce::TextButton resetButton { "Reset" };
    std::vector<std::unique_ptr<Knob>> spaceKnobs, toneKnobs;

    juce::Rectangle<int> spacePanel, tonePanel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StudioReverbAudioProcessorEditor)
};
