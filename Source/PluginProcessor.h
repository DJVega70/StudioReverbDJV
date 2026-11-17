#pragma once
#include <JuceHeader.h>

class StudioReverbAudioProcessor : public juce::AudioProcessor,
                                   private juce::AudioProcessorValueTreeState::Listener,
                                   private juce::Timer
{
public:
    StudioReverbAudioProcessor();
    ~StudioReverbAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override;

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::AudioProcessorValueTreeState apvts;

    // Mono output tap for the editor's spectrum display
    int pullSpectrumSamples(float* dest, int maxNum);

    // Output peaks since last read (editor meters)
    std::atomic<float> outPeakL { 0.0f }, outPeakR { 0.0f };

private:
    void parameterChanged(const juce::String& id, float) override;
    void timerCallback() override;
    void rebuildImpulseResponse();
    static juce::AudioBuffer<float> buildImpulseResponse(double sr, float decaySeconds, float sizePercent,
                                                         float dampingPercent, float preDelayMs);
    float get(const char* id) const { return apvts.getRawParameterValue(id)->load(); }

    juce::dsp::Convolution convolution;
    juce::dsp::StateVariableTPTFilter<float> lowCutFilter, highCutFilter;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> chorusDelay { 4096 };

    juce::AudioBuffer<float> wetBuffer;

    juce::SmoothedValue<float> lowCutSm, highCutSm, dryGainSm, wetGainSm, masterSm, depthSm, rateSm;
    float lfoPhase = 0.0f;
    double sampleRate = 44100.0;
    int blockSize = 512;

    std::atomic<bool> irDirty { false };
    std::atomic<juce::int64> lastIrChangeMs { 0 };

    juce::AbstractFifo spectrumFifo { 8192 };
    std::array<float, 8192> spectrumData {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StudioReverbAudioProcessor)
};
