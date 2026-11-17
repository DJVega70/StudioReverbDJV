#include "PluginEditor.h"

namespace
{
    juce::String fmtDb(double v)  { return juce::String(v, 1) + " dB"; }
    juce::String fmtPct(double v) { return juce::String(juce::roundToInt(v)) + "%"; }
}

VcaBusCompAudioProcessorEditor::VcaBusCompAudioProcessorEditor(VcaBusCompAudioProcessor& p)
    : AudioProcessorEditor(&p), proc(p),
      threshold(p.apvts, "threshold", "Threshold",   true,  fmtDb),
      ratio    (p.apvts, "ratio",     "Ratio",       false, {}, "2|4|10"),
      attack   (p.apvts, "attack",    "Attack (ms)", false, {}, ".1|.3|1|3|10|30"),
      release  (p.apvts, "release",   "Release (s)", false, {}, ".1|.2|.4|.8|1.2|A"),
      scHpf    (p.apvts, "schpf",     "SC HPF (Hz)", false, {}, "Off|60|90|150"),
      makeup   (p.apvts, "makeup",    "Make-up",     false, fmtDb),
      mix      (p.apvts, "mix",       "Mix",         false, fmtPct),
      voicing  (p.apvts, "mode",      "Voicing",     "G-Series|E-Series")
{
    setLookAndFeel(&lnf);
    for (auto* c : std::initializer_list<juce::Component*> { &threshold, &ratio, &attack, &release, &scHpf,
                                                             &makeup, &mix, &voicing, &gr, &meter, &oled })
        addAndMakeVisible(c);
    gr.maxDb = 20.0f;

    resetButton.onClick = [this] { vegaui::resetAllParameters(proc); };
    addAndMakeVisible(resetButton);

    setSize(W, H);
    startTimerHz(30);
}

VcaBusCompAudioProcessorEditor::~VcaBusCompAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void VcaBusCompAudioProcessorEditor::timerCallback()
{
    const float g = proc.grMeterDb.load();
    grShown = juce::jmax(g, grShown * 0.85f);
    gr.setGr(g);
    meter.setLevels(proc.outPeakL.exchange(0.0f), proc.outPeakR.exchange(0.0f));

    const int mode = juce::roundToInt(proc.apvts.getRawParameterValue("mode")->load());
    oled.setLines("GAIN REDUCTION", juce::String(grShown, 1) + " dB",
                  "VOICING", mode == 0 ? "G-SERIES" : "E-SERIES",
                  "MAKE-UP", juce::String(proc.apvts.getRawParameterValue("makeup")->load(), 1) + " dB");
}

void VcaBusCompAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(VegaColours::background);
    vegaui::drawTopBar(g, W, "BUS COMPRESSOR  VP-3");
    vegaui::drawBody(g, { 0, 35, W, H - 65 });

    const char* titles[] { "THRESHOLD", "DYNAMICS", "OUTPUT", "", "OUT" };
    for (int i = 0; i < 5; ++i)
        vegaui::drawSection(g, { sectionX[(size_t) i], 40, sectionX[(size_t) i + 1] - sectionX[(size_t) i], H - 76 }, titles[i], i > 0);

    vegaui::drawBottomBar(g, { 0, H - 30, W, 30 }, "VCA BUS COMPRESSOR");
}

void VcaBusCompAudioProcessorEditor::resized()
{
    sectionX = { 0, 260, 600, 740, 830, W };
    resetButton.setBounds(W - 70, 7, 60, 20);

    // THRESHOLD — big knob + OLED
    threshold.setBounds(30, 56, 200, 200);
    oled.setBounds(20, 268, 220, 80);

    // DYNAMICS — stepped knobs
    const int x0 = sectionX[1] + 10, cw = (sectionX[2] - sectionX[1] - 20) / 3;
    ratio  .setBounds(x0,          60, cw, 140);
    attack .setBounds(x0 + cw,     60, cw, 140);
    release.setBounds(x0 + 2 * cw, 60, cw, 140);
    scHpf  .setBounds(x0 + cw / 2, 210, cw, 140);
    voicing.setBounds(x0 + cw / 2 + cw + 10, 240, cw, 70);

    // OUTPUT
    makeup.setBounds(sectionX[2] + 15, 60,  sectionX[3] - sectionX[2] - 30, 130);
    mix   .setBounds(sectionX[2] + 15, 210, sectionX[3] - sectionX[2] - 30, 130);

    // meters
    gr   .setBounds(sectionX[3] + 22, 52, 50, 300);
    meter.setBounds(sectionX[4] + 18, 58, 70, 300);
}
