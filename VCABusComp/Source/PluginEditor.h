#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "VegaUI.h"

class VcaBusCompAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit VcaBusCompAudioProcessorEditor(VcaBusCompAudioProcessor&);
    ~VcaBusCompAudioProcessorEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;

    static constexpr int W = 940, H = 420;

private:
    void timerCallback() override;

    VcaBusCompAudioProcessor& proc;
    VegaLookAndFeel lnf;

    vegaui::Knob threshold, ratio, attack, release, scHpf, makeup, mix;
    vegaui::Toggle voicing;
    vegaui::GrColumn gr;
    vegaui::StereoMeter meter;
    vegaui::Oled oled;
    juce::TextButton resetButton { "RESET" };
    std::array<int, 6> sectionX {};
    float grShown = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VcaBusCompAudioProcessorEditor)
};
