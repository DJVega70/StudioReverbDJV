#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "VegaUI.h"

// Log-frequency spectrum of the reverb output, styled like the Vega analyser
class SpectrumView : public juce::Component
{
public:
    explicit SpectrumView(StudioReverbAudioProcessor& p) : proc(p) { display.fill(-120.0f); }
    void refresh();
    void paint(juce::Graphics& g) override;

private:
    static constexpr int fftOrder = 11, fftSize = 1 << fftOrder;
    StudioReverbAudioProcessor& proc;
    juce::dsp::FFT fft { fftOrder };
    juce::dsp::WindowingFunction<float> window { (size_t) fftSize, juce::dsp::WindowingFunction<float>::hann };
    std::array<float, fftSize> ring {};
    std::array<float, fftSize * 2> fftData {};
    std::array<float, fftSize / 2 + 1> display {};
    std::array<float, 4096> scratch {};
    int ringPos = 0;
};

class StudioReverbAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit StudioReverbAudioProcessorEditor(StudioReverbAudioProcessor&);
    ~StudioReverbAudioProcessorEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;

    static constexpr int W = 1080, H = 560;

private:
    void timerCallback() override;

    StudioReverbAudioProcessor& proc;
    VegaLookAndFeel lnf;

    vegaui::Knob decay, size, damping, predelay, modRate, modDepth, lowCut, highCut, mix, output;
    vegaui::StereoMeter meter;
    vegaui::Oled oled;
    SpectrumView spectrum;
    juce::TextButton resetButton { "RESET" };
    std::array<int, 6> sectionX {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StudioReverbAudioProcessorEditor)
};
