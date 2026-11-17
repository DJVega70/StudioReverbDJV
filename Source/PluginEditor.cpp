#include "PluginEditor.h"

namespace
{
    juce::String fmtHz(double v)
    {
        return v >= 1000.0 ? juce::String(v / 1000.0, v >= 10000.0 ? 1 : 2) + " kHz" : juce::String(juce::roundToInt(v)) + " Hz";
    }
    juce::String fmtSec(double v) { return juce::String(v, 2) + " s"; }
    juce::String fmtPct(double v) { return juce::String(v, 1) + "%"; }
    juce::String fmtMs(double v)  { return juce::String(v, 1) + " ms"; }
    juce::String fmtRate(double v){ return juce::String(v, 2) + " Hz"; }
}

//==============================================================================
void SpectrumView::refresh()
{
    const int got = proc.pullSpectrumSamples(scratch.data(), (int) scratch.size());
    for (int i = 0; i < got; ++i)
    {
        ring[(size_t) ringPos] = scratch[(size_t) i];
        ringPos = (ringPos + 1) % fftSize;
    }
    if (got > 0)
    {
        for (int i = 0; i < fftSize; ++i) fftData[(size_t) i] = ring[(size_t) ((ringPos + i) % fftSize)];
        std::fill(fftData.begin() + fftSize, fftData.end(), 0.0f);
        window.multiplyWithWindowingTable(fftData.data(), (size_t) fftSize);
        fft.performFrequencyOnlyForwardTransform(fftData.data());
        const double fs = juce::jmax(8000.0, proc.getSampleRate());
        for (int k = 1; k <= fftSize / 2; ++k)
        {
            const double f = (double) k * fs / fftSize;
            const float db = vegaui::gainToDb(fftData[(size_t) k] / (fftSize * 0.25f))
                             + 3.0f * (float) std::log2(juce::jmax(f, 20.0) / 1000.0);
            auto& d = display[(size_t) k];
            d = db > d ? db : d - 1.4f;
        }
    }
    else
        for (auto& d : display) d -= 1.4f;
    repaint();
}

void SpectrumView::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    vegaui::drawScreen(g, b);
    auto plot = b.reduced(10.0f, 8.0f).withTrimmedBottom(12.0f);
    const double fs = juce::jmax(8000.0, proc.getSampleRate());
    auto xFor = [&](double f) { return plot.getX() + (float) (std::log(f / 20.0) / std::log(1000.0)) * plot.getWidth(); };

    g.setFont(VegaFonts::ui(9.5f));
    for (double f : { 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0 })
    {
        const float x = xFor(f);
        g.setColour(juce::Colours::white.withAlpha(0.06f));
        g.drawVerticalLine((int) x, plot.getY(), plot.getBottom());
        g.setColour(VegaColours::textDim);
        g.drawText(f >= 1000 ? juce::String((int) (f / 1000)) + "k" : juce::String((int) f),
                   (int) x - 15, (int) plot.getBottom() + 1, 30, 12, juce::Justification::centred);
    }

    // shaded cut‑filter region
    const float lc = proc.apvts.getRawParameterValue("lowcut")->load();
    const float hc = proc.apvts.getRawParameterValue("highcut")->load();
    g.setColour(juce::Colours::black.withAlpha(0.35f));
    g.fillRect(juce::Rectangle<float>::leftTopRightBottom(plot.getX(), plot.getY(), xFor(lc), plot.getBottom()));
    g.fillRect(juce::Rectangle<float>::leftTopRightBottom(xFor(hc), plot.getY(), plot.getRight(), plot.getBottom()));
    g.setColour(VegaColours::accent.withAlpha(0.6f));
    g.drawVerticalLine((int) xFor(lc), plot.getY(), plot.getBottom());
    g.drawVerticalLine((int) xFor(hc), plot.getY(), plot.getBottom());

    juce::Path spec;
    const int W = (int) plot.getWidth();
    for (int px = 0; px <= W; px += 2)
    {
        const double f0 = 20.0 * std::pow(1000.0, (double) px / W), f1 = 20.0 * std::pow(1000.0, (double) (px + 2) / W);
        const int k0 = juce::jlimit(1, fftSize / 2, (int) (f0 * fftSize / fs));
        const int k1 = juce::jlimit(k0, fftSize / 2, (int) (f1 * fftSize / fs));
        float mx = -200.0f;
        for (int k = k0; k <= k1; ++k) mx = juce::jmax(mx, display[(size_t) k]);
        const float y = juce::jmap(juce::jlimit(-84.0f, 6.0f, mx), -84.0f, 6.0f, plot.getBottom(), plot.getY());
        if (px == 0) { spec.startNewSubPath(plot.getX(), plot.getBottom()); spec.lineTo(plot.getX(), y); }
        else spec.lineTo(plot.getX() + (float) px, y);
    }
    juce::Path fill(spec);
    fill.lineTo(plot.getRight(), plot.getBottom()); fill.closeSubPath();
    g.setGradientFill(juce::ColourGradient(juce::Colours::white.withAlpha(0.2f), 0, plot.getY(),
                                           juce::Colours::white.withAlpha(0.02f), 0, plot.getBottom(), false));
    g.fillPath(fill);
    g.setColour(juce::Colours::white.withAlpha(0.55f));
    g.strokePath(spec, juce::PathStrokeType(1.0f));

    g.setColour(VegaColours::textDim);
    g.setFont(VegaFonts::ui(10.0f, true));
    g.drawText("REVERB OUTPUT SPECTRUM", (int) plot.getX() + 4, (int) plot.getY() + 2, 200, 12, juce::Justification::left);
}

//==============================================================================
StudioReverbAudioProcessorEditor::StudioReverbAudioProcessorEditor(StudioReverbAudioProcessor& p)
    : AudioProcessorEditor(&p), proc(p),
      decay   (p.apvts, "decay",    "Decay",     true,  fmtSec),
      size    (p.apvts, "size",     "Room Size", true,  fmtPct),
      damping (p.apvts, "damping",  "Damping",   false, fmtPct),
      predelay(p.apvts, "predelay", "Pre-Delay", false, fmtMs),
      modRate (p.apvts, "modrate",  "Mod Rate",  false, fmtRate),
      modDepth(p.apvts, "moddepth", "Mod Depth", false, fmtPct),
      lowCut  (p.apvts, "lowcut",   "Low Cut",   false, fmtHz),
      highCut (p.apvts, "highcut",  "High Cut",  false, fmtHz),
      mix     (p.apvts, "mix",      "Mix",       true,  fmtPct),
      output  (p.apvts, "output",   "Output",    false, fmtPct),
      spectrum(p)
{
    setLookAndFeel(&lnf);
    for (auto* c : { &decay, &size, &damping, &predelay, &modRate, &modDepth, &lowCut, &highCut, &mix, &output })
        addAndMakeVisible(c);
    addAndMakeVisible(meter);
    addAndMakeVisible(oled);
    addAndMakeVisible(spectrum);

    resetButton.onClick = [this] { vegaui::resetAllParameters(proc); };
    resetButton.setTooltip("Reset all controls to default");
    addAndMakeVisible(resetButton);

    setSize(W, H);
    startTimerHz(30);
}

StudioReverbAudioProcessorEditor::~StudioReverbAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void StudioReverbAudioProcessorEditor::timerCallback()
{
    meter.setLevels(proc.outPeakL.exchange(0.0f), proc.outPeakR.exchange(0.0f));
    spectrum.refresh();

    auto v = [this](const char* id) { return proc.apvts.getRawParameterValue(id)->load(); };
    oled.setLines("DECAY TIME", juce::String(v("decay"), 2) + " s",
                  "PRE-DELAY", juce::String(juce::roundToInt(v("predelay"))) + " ms",
                  "WET MIX", juce::String(juce::roundToInt(v("mix"))) + " %");
}

void StudioReverbAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(VegaColours::background);
    vegaui::drawTopBar(g, W, "REVERB  VP-2");
    vegaui::drawBody(g, { 0, 35, W, 315 });

    const char* titles[] { "SPACE", "CHARACTER", "TONE", "OUTPUT", "OUT" };
    for (int i = 0; i < 5; ++i)
        vegaui::drawSection(g, { sectionX[(size_t) i], 40, sectionX[(size_t) i + 1] - sectionX[(size_t) i], 304 }, titles[i], i > 0);

    g.setColour(VegaColours::textDim.withAlpha(0.35f));
    g.setFont(VegaFonts::ui(10.0f, true));
    g.drawText("V E G A   P R O D U C T I O N S", sectionX[0], 322, sectionX[1] - sectionX[0], 14, juce::Justification::centred);

    vegaui::drawBottomBar(g, { 0, H - 30, W, 30 }, "STEREO ALGORITHMIC REVERB");
}

void StudioReverbAudioProcessorEditor::resized()
{
    sectionX = { 0, 340, 600, 740, 990, W };
    resetButton.setBounds(W - 70, 7, 60, 20);

    // SPACE — two big knobs
    decay.setBounds(20, 60, 150, 190);
    size .setBounds(175, 60, 150, 190);

    // CHARACTER — 2 x 2
    const int cw = (sectionX[2] - sectionX[1]) / 2;
    damping .setBounds(sectionX[1] + 10,      60,  cw - 20, 120);
    predelay.setBounds(sectionX[1] + cw + 10, 60,  cw - 20, 120);
    modRate .setBounds(sectionX[1] + 10,      190, cw - 20, 120);
    modDepth.setBounds(sectionX[1] + cw + 10, 190, cw - 20, 120);

    // TONE — stacked
    lowCut .setBounds(sectionX[2] + 15, 60,  sectionX[3] - sectionX[2] - 30, 120);
    highCut.setBounds(sectionX[2] + 15, 190, sectionX[3] - sectionX[2] - 30, 120);

    // OUTPUT — big Mix, Output
    mix   .setBounds(sectionX[3] + 10,  60, 140, 190);
    output.setBounds(sectionX[3] + 150, 100, 90, 120);

    // meters
    meter.setBounds(sectionX[4] + 18, 52, 70, 280);

    // analysis row
    spectrum.setBounds(10, 358, 760, 162);
    oled.setBounds(780, 358, 290, 162);
}

