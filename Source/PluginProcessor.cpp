#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    const char* kIrParams[] = { "decay", "size", "damping", "predelay" };
}

juce::AudioProcessorValueTreeState::ParameterLayout StudioReverbAudioProcessor::createLayout()
{
    using P = juce::AudioParameterFloat;
    using R = juce::NormalisableRange<float>;
    juce::AudioProcessorValueTreeState::ParameterLayout l;

    l.add(std::make_unique<P>(juce::ParameterID{ "decay", 1 },    "Decay",      R(0.2f, 10.0f, 0.01f), 2.8f));
    l.add(std::make_unique<P>(juce::ParameterID{ "size", 1 },     "Room Size",  R(10.0f, 100.0f, 0.1f), 65.0f));
    l.add(std::make_unique<P>(juce::ParameterID{ "damping", 1 },  "Damping",    R(0.0f, 100.0f, 0.1f), 40.0f));
    l.add(std::make_unique<P>(juce::ParameterID{ "predelay", 1 }, "Pre-Delay",  R(0.0f, 200.0f, 0.1f), 25.0f));
    l.add(std::make_unique<P>(juce::ParameterID{ "modrate", 1 },  "Mod Rate",   R(0.1f, 5.0f, 0.01f), 0.8f));
    l.add(std::make_unique<P>(juce::ParameterID{ "moddepth", 1 }, "Mod Depth",  R(0.0f, 100.0f, 0.1f), 25.0f));
    l.add(std::make_unique<P>(juce::ParameterID{ "lowcut", 1 },   "Low Cut",    R(20.0f, 1000.0f, 1.0f), 120.0f));
    l.add(std::make_unique<P>(juce::ParameterID{ "highcut", 1 },  "High Cut",   R(1000.0f, 20000.0f, 1.0f), 7500.0f));
    l.add(std::make_unique<P>(juce::ParameterID{ "mix", 1 },      "Mix",        R(0.0f, 100.0f, 0.1f), 40.0f));
    l.add(std::make_unique<P>(juce::ParameterID{ "output", 1 },   "Output",     R(0.0f, 100.0f, 0.1f), 85.0f));
    return l;
}

StudioReverbAudioProcessor::StudioReverbAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "STATE", createLayout())
{
    for (auto* id : kIrParams)
        apvts.addParameterListener(id, this);
    startTimer(50);
}

StudioReverbAudioProcessor::~StudioReverbAudioProcessor()
{
    stopTimer();
    for (auto* id : kIrParams)
        apvts.removeParameterListener(id, this);
}

bool StudioReverbAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

double StudioReverbAudioProcessor::getTailLengthSeconds() const
{
    return (double) get("decay") + (double) get("predelay") / 1000.0;
}

// Same synthetic IR as the web version: decaying, damped, decorrelated stereo noise
juce::AudioBuffer<float> StudioReverbAudioProcessor::buildImpulseResponse(double sr, float decaySeconds,
                                                                          float sizePercent, float dampingPercent,
                                                                          float preDelayMs)
{
    const int length = juce::jmax((int) (sr * 0.1), (int) std::floor(sr * juce::jmin(decaySeconds, 10.0f)));
    juce::AudioBuffer<float> ir(2, length);
    auto* left = ir.getWritePointer(0);
    auto* right = ir.getWritePointer(1);

    const int preDelaySamples = (int) std::floor((preDelayMs / 1000.0) * sr);
    const float sizeFactor = 0.5f + (sizePercent / 100.0f) * 0.7f;
    const float dampFactor = 0.05f + (dampingPercent / 100.0f) * 0.9f;
    const float decayRate = 3.0f / (decaySeconds * sizeFactor);

    juce::Random rng(0x5EEDBEEF); // fixed seed: same settings always sound the same

    for (int i = 0; i < length; ++i)
    {
        if (i < preDelaySamples)
        {
            left[i] = right[i] = 0.0f;
            continue;
        }
        const float t = (float) ((i - preDelaySamples) / sr);
        const float env = std::exp(-decayRate * t);
        const float damp = std::exp(-t * dampFactor * 8.0f);
        left[i]  = (rng.nextFloat() * 2.0f - 1.0f) * damp * env;
        right[i] = (rng.nextFloat() * 2.0f - 1.0f) * damp * env;
    }
    return ir;
}

void StudioReverbAudioProcessor::rebuildImpulseResponse()
{
    auto ir = buildImpulseResponse(sampleRate, get("decay"), get("size"), get("damping"), get("predelay"));
    convolution.loadImpulseResponse(std::move(ir), sampleRate,
                                    juce::dsp::Convolution::Stereo::yes,
                                    juce::dsp::Convolution::Trim::no,
                                    juce::dsp::Convolution::Normalise::yes);
}

void StudioReverbAudioProcessor::parameterChanged(const juce::String&, float)
{
    lastIrChangeMs = juce::Time::currentTimeMillis();
    irDirty = true;
}

void StudioReverbAudioProcessor::timerCallback()
{
    // Debounce: rebuild once the knob has been still for a moment
    if (irDirty.load() && juce::Time::currentTimeMillis() - lastIrChangeMs.load() > 120)
    {
        irDirty = false;
        rebuildImpulseResponse();
    }
}

void StudioReverbAudioProcessor::prepareToPlay(double sr, int samplesPerBlock)
{
    sampleRate = sr;
    blockSize = samplesPerBlock;

    juce::dsp::ProcessSpec spec { sr, (juce::uint32) samplesPerBlock, 2 };

    convolution.prepare(spec);
    rebuildImpulseResponse();

    lowCutFilter.prepare(spec);
    lowCutFilter.setType(juce::dsp::StateVariableTPTFilterType::highpass);
    lowCutFilter.setResonance(1.0f);
    highCutFilter.prepare(spec);
    highCutFilter.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
    highCutFilter.setResonance(1.0f);

    chorusDelay.setMaximumDelayInSamples((int) (0.05 * sr) + 4);
    chorusDelay.prepare(spec);
    chorusDelay.reset();

    wetBuffer.setSize(2, samplesPerBlock);
    lfoPhase = 0.0f;

    const double ramp = 0.05;
    lowCutSm.reset(sr, ramp);  lowCutSm.setCurrentAndTargetValue(get("lowcut"));
    highCutSm.reset(sr, ramp); highCutSm.setCurrentAndTargetValue(get("highcut"));
    depthSm.reset(sr, ramp);   depthSm.setCurrentAndTargetValue(get("moddepth"));
    rateSm.reset(sr, ramp);    rateSm.setCurrentAndTargetValue(get("modrate"));
    dryGainSm.reset(sr, ramp);
    wetGainSm.reset(sr, ramp);
    masterSm.reset(sr, ramp);
    const float m = get("mix") / 100.0f * juce::MathConstants<float>::halfPi;
    dryGainSm.setCurrentAndTargetValue(std::cos(m));
    wetGainSm.setCurrentAndTargetValue(std::sin(m));
    masterSm.setCurrentAndTargetValue(get("output") / 100.0f);
    irDirty = false;
}

void StudioReverbAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (buffer.getNumChannels() < 2 || n == 0)
        return;

    if (wetBuffer.getNumSamples() < n)
        wetBuffer.setSize(2, n, false, false, true);

    // Targets
    lowCutSm.setTargetValue(get("lowcut"));
    highCutSm.setTargetValue(get("highcut"));
    depthSm.setTargetValue(get("moddepth"));
    rateSm.setTargetValue(get("modrate"));
    const float m = get("mix") / 100.0f * juce::MathConstants<float>::halfPi;
    dryGainSm.setTargetValue(std::cos(m));
    wetGainSm.setTargetValue(std::sin(m));
    masterSm.setTargetValue(get("output") / 100.0f);

    // Wet path: copy input -> low cut -> high cut -> convolution
    for (int ch = 0; ch < 2; ++ch)
        wetBuffer.copyFrom(ch, 0, buffer, ch, 0, n);

    juce::dsp::AudioBlock<float> wetBlock(wetBuffer);
    auto block = wetBlock.getSubBlock(0, (size_t) n);
    juce::dsp::ProcessContextReplacing<float> ctx(block);

    lowCutFilter.setCutoffFrequency(lowCutSm.skip(n));
    highCutFilter.setCutoffFrequency(juce::jmin(highCutSm.skip(n), (float) (sampleRate * 0.45)));
    lowCutFilter.process(ctx);
    highCutFilter.process(ctx);
    convolution.process(ctx);

    // Stereo chorus on the wet signal (L/R delay LFOs in opposite phase)
    auto* wl = wetBuffer.getWritePointer(0);
    auto* wr = wetBuffer.getWritePointer(1);
    auto* ol = buffer.getWritePointer(0);
    auto* or_ = buffer.getWritePointer(1);

    for (int i = 0; i < n; ++i)
    {
        const float depthSec = depthSm.getNextValue() / 100.0f * 0.0035f;
        const float rate = rateSm.getNextValue();
        const float lfo = std::sin(lfoPhase);
        lfoPhase += juce::MathConstants<float>::twoPi * rate / (float) sampleRate;
        if (lfoPhase > juce::MathConstants<float>::twoPi)
            lfoPhase -= juce::MathConstants<float>::twoPi;

        const float dL = (0.012f + depthSec * lfo) * (float) sampleRate;
        const float dR = (0.016f - depthSec * lfo) * (float) sampleRate;

        chorusDelay.pushSample(0, wl[i]);
        chorusDelay.pushSample(1, wr[i]);
        const float cl = chorusDelay.popSample(0, dL);
        const float cr = chorusDelay.popSample(1, dR);

        const float dry = dryGainSm.getNextValue();
        const float wet = wetGainSm.getNextValue();
        const float master = masterSm.getNextValue();

        ol[i]  = (ol[i]  * dry + cl * wet) * master;
        or_[i] = (or_[i] * dry + cr * wet) * master;
    }

    // Feed spectrum display (mono tap)
    {
        int start1, size1, start2, size2;
        spectrumFifo.prepareToWrite(n, start1, size1, start2, size2);
        int idx = 0;
        for (int i = 0; i < size1; ++i, ++idx) spectrumData[(size_t) (start1 + i)] = 0.5f * (ol[idx] + or_[idx]);
        for (int i = 0; i < size2; ++i, ++idx) spectrumData[(size_t) (start2 + i)] = 0.5f * (ol[idx] + or_[idx]);
        spectrumFifo.finishedWrite(size1 + size2);
    }

    // Output peak meters for the editor
    {
        float pl = 0.0f, pr = 0.0f;
        for (int i = 0; i < n; ++i) { pl = std::max(pl, std::abs(ol[i])); pr = std::max(pr, std::abs(or_[i])); }
        auto atomicMax = [] (std::atomic<float>& a, float v)
        {
            float prev = a.load();
            while (prev < v && ! a.compare_exchange_weak(prev, v)) {}
        };
        atomicMax(outPeakL, pl);
        atomicMax(outPeakR, pr);
    }
}

int StudioReverbAudioProcessor::pullSpectrumSamples(float* dest, int maxNum)
{
    int start1, size1, start2, size2;
    const int want = juce::jmin(maxNum, spectrumFifo.getNumReady());
    spectrumFifo.prepareToRead(want, start1, size1, start2, size2);
    int idx = 0;
    for (int i = 0; i < size1; ++i) dest[idx++] = spectrumData[(size_t) (start1 + i)];
    for (int i = 0; i < size2; ++i) dest[idx++] = spectrumData[(size_t) (start2 + i)];
    spectrumFifo.finishedRead(size1 + size2);
    return size1 + size2;
}

juce::AudioProcessorEditor* StudioReverbAudioProcessor::createEditor()
{
    return new StudioReverbAudioProcessorEditor(*this);
}

void StudioReverbAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary(*xml, destData);
}

void StudioReverbAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(apvts.state.getType()))
        {
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
            lastIrChangeMs = 0;
            irDirty = true;
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new StudioReverbAudioProcessor();
}
