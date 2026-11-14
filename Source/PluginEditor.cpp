#include "PluginEditor.h"

namespace
{
    const juce::Colour zinc950 { 0xff09090b }, zinc900 { 0xff18181b }, zinc800 { 0xff27272a },
                       zinc700 { 0xff3f3f46 }, zinc400 { 0xffa1a1aa }, zinc300 { 0xffd4d4d8 },
                       sky400 { 0xff38bdf8 };

    juce::String fmtHz(double v)
    {
        return v >= 1000.0 ? juce::String(v / 1000.0, 1) + "k" : juce::String(juce::roundToInt(v)) + "Hz";
    }
}

//==============================================================================
void KnobLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h, float pos,
                                       float startAngle, float endAngle, juce::Slider& s)
{
    const auto accent = s.findColour(juce::Slider::rotarySliderFillColourId);
    const auto bounds = juce::Rectangle<float>((float) x, (float) y, (float) w, (float) h).reduced(1.0f);
    const float dim = juce::jmin(bounds.getWidth(), bounds.getHeight());
    const auto c = bounds.getCentre();
    const float strokeW = dim > 70.0f ? 4.0f : 3.0f;
    const float radius = dim * 0.5f - strokeW;

    // body + border
    g.setColour(zinc950.withAlpha(0.8f));
    g.fillEllipse(c.x - dim * 0.5f, c.y - dim * 0.5f, dim, dim);
    g.setColour(zinc800);
    g.drawEllipse(c.x - dim * 0.5f, c.y - dim * 0.5f, dim, dim, 1.0f);

    // track + value arc
    juce::Path track, fill;
    track.addCentredArc(c.x, c.y, radius, radius, 0.0f, startAngle, endAngle, true);
    g.setColour(zinc800);
    g.strokePath(track, juce::PathStrokeType(strokeW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float angle = startAngle + pos * (endAngle - startAngle);
    if (pos > 0.001f)
    {
        fill.addCentredArc(c.x, c.y, radius, radius, 0.0f, startAngle, angle, true);
        g.setColour(accent);
        g.strokePath(fill, juce::PathStrokeType(strokeW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // inner cap with pointer
    const float capD = dim - 16.0f;
    g.setGradientFill(juce::ColourGradient(zinc800, c.x, c.y - capD * 0.5f, zinc900, c.x, c.y + capD * 0.5f, false));
    g.fillEllipse(c.x - capD * 0.5f, c.y - capD * 0.5f, capD, capD);
    g.setColour(zinc700.withAlpha(0.6f));
    g.drawEllipse(c.x - capD * 0.5f, c.y - capD * 0.5f, capD, capD, 1.0f);

    juce::Path pointer;
    pointer.addRoundedRectangle(-1.0f, -capD * 0.5f + 3.0f, 2.0f, 8.0f, 1.0f);
    g.setColour(accent);
    g.fillPath(pointer, juce::AffineTransform::rotation(angle).translated(c.x, c.y));
}

//==============================================================================
Knob::Knob(juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId, const juce::String& labelText,
           juce::Colour accent, std::function<juce::String(double)> formatter, bool isLarge, KnobLookAndFeel& laf)
    : large(isLarge), fmt(std::move(formatter))
{
    slider.setLookAndFeel(&laf);
    slider.setColour(juce::Slider::rotarySliderFillColourId, accent);
    slider.setRotaryParameters(juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    slider.setMouseDragSensitivity(large ? 180 : 130);
    addAndMakeVisible(slider);

    name.setText(labelText.toUpperCase(), juce::dontSendNotification);
    name.setJustificationType(juce::Justification::centred);
    name.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    name.setColour(juce::Label::textColourId, zinc300);
    name.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(name);

    value.setJustificationType(juce::Justification::centred);
    value.setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 11.0f, juce::Font::plain));
    value.setColour(juce::Label::textColourId, zinc400);
    value.setColour(juce::Label::backgroundColourId, zinc900.withAlpha(0.8f));
    value.setColour(juce::Label::outlineColourId, zinc800);
    value.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(value);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, paramId, slider);
    if (auto* p = apvts.getParameter(paramId))
        slider.setDoubleClickReturnValue(true, p->convertFrom0to1(p->getDefaultValue()));

    slider.onValueChange = [this] { value.setText(fmt(slider.getValue()), juce::dontSendNotification); };
    slider.onValueChange();
}

void Knob::resized()
{
    auto r = getLocalBounds();
    const int dim = large ? 76 : 60;
    slider.setBounds(r.removeFromTop(dim).withSizeKeepingCentre(dim, dim));
    r.removeFromTop(4);
    name.setBounds(r.removeFromTop(16));
    r.removeFromTop(2);
    value.setBounds(r.removeFromTop(18).reduced(4, 0));
}

//==============================================================================
SpectrumView::SpectrumView(StudioReverbAudioProcessor& p) : proc(p) { startTimerHz(30); }

void SpectrumView::timerCallback()
{
    const int got = proc.pullSpectrumSamples(scratch.data(), (int) scratch.size());
    for (int i = 0; i < got; ++i)
    {
        ring[(size_t) ringPos] = scratch[(size_t) i];
        ringPos = (ringPos + 1) % fftSize;
    }

    // Unroll ring (oldest -> newest), window, FFT
    for (int i = 0; i < fftSize; ++i)
        fftData[(size_t) i] = ring[(size_t) ((ringPos + i) % fftSize)];
    std::fill(fftData.begin() + fftSize, fftData.end(), 0.0f);
    window.multiplyWithWindowingTable(fftData.data(), (size_t) fftSize);
    fft.performFrequencyOnlyForwardTransform(fftData.data());

    for (int k = 0; k < fftSize / 2; ++k)
    {
        const float mag = fftData[(size_t) k] / (float) fftSize;
        smoothed[(size_t) k] = 0.8f * smoothed[(size_t) k] + 0.2f * mag;
    }
    repaint();
}

void SpectrumView::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour(zinc950.withAlpha(0.9f));
    g.fillRoundedRectangle(b, 10.0f);

    g.setColour(zinc900);
    for (float y = 15.0f; y < b.getHeight(); y += 20.0f)
        g.drawHorizontalLine((int) y, 0.0f, b.getWidth());

    const float w = b.getWidth(), h = b.getHeight();
    const float barW = w / numBars - 2.0f;
    const int step = (fftSize / 2) / numBars;

    for (int i = 0; i < numBars; ++i)
    {
        const float mag = smoothed[(size_t) (i * step)];
        const float db = juce::Decibels::gainToDecibels(mag, -100.0f);
        const float val = juce::jlimit(0.0f, 1.0f, (db + 100.0f) / 70.0f); // -100 dB .. -30 dB like Web Audio
        const float barH = juce::jmax(4.0f, val * (h - 10.0f));
        const float x = i * (barW + 2.0f), y = h - barH;
        juce::ColourGradient grad(sky400, 0, y, juce::Colour(0xff082f49), 0, h, false);
        grad.addColour(0.6, juce::Colour(0xff0284c7));
        g.setGradientFill(grad);
        g.fillRect(x, y, barW, barH);
    }

    g.setColour(zinc400);
    g.setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 10.0f, juce::Font::plain));
    g.drawText("REAL-TIME REVERB SPECTRUM", 12, 6, 300, 14, juce::Justification::left);

    g.setColour(zinc800);
    g.drawRoundedRectangle(b.reduced(0.5f), 10.0f, 1.0f);
}

//==============================================================================
StudioReverbAudioProcessorEditor::StudioReverbAudioProcessorEditor(StudioReverbAudioProcessor& p)
    : AudioProcessorEditor(&p), proc(p), spectrum(p)
{
    auto& a = proc.apvts;
    const auto s = [](double v) { return juce::String(v, 1) + "s"; };
    const auto pct = [](double v) { return juce::String(juce::roundToInt(v)) + "%"; };
    const auto ms = [](double v) { return juce::String(juce::roundToInt(v)) + "ms"; };
    const auto hz1 = [](double v) { return juce::String(v, 1) + "Hz"; };

    const juce::Colour sky { 0xff38bdf8 }, blue { 0xff60a5fa }, purple { 0xffc084fc },
                       green { 0xff34d399 }, amber { 0xfff59e0b }, violet { 0xffa855f7 };

    spaceKnobs.push_back(std::make_unique<Knob>(a, "decay",    "Decay",     sky,    s,   true,  laf));
    spaceKnobs.push_back(std::make_unique<Knob>(a, "size",     "Room Size", sky,    pct, true,  laf));
    spaceKnobs.push_back(std::make_unique<Knob>(a, "damping",  "Damping",   blue,   pct, false, laf));
    spaceKnobs.push_back(std::make_unique<Knob>(a, "predelay", "Pre-Delay", blue,   ms,  false, laf));
    spaceKnobs.push_back(std::make_unique<Knob>(a, "modrate",  "Mod Rate",  purple, hz1, false, laf));
    spaceKnobs.push_back(std::make_unique<Knob>(a, "moddepth", "Mod Depth", purple, pct, false, laf));

    toneKnobs.push_back(std::make_unique<Knob>(a, "lowcut",  "Low Cut",  green,  fmtHz, false, laf));
    toneKnobs.push_back(std::make_unique<Knob>(a, "highcut", "High Cut", green,  fmtHz, false, laf));
    toneKnobs.push_back(std::make_unique<Knob>(a, "mix",     "Mix",      amber,  pct,   false, laf));
    toneKnobs.push_back(std::make_unique<Knob>(a, "output",  "Output",   violet, pct,   false, laf));

    for (auto& k : spaceKnobs) addAndMakeVisible(*k);
    for (auto& k : toneKnobs) addAndMakeVisible(*k);
    addAndMakeVisible(spectrum);

    resetButton.setColour(juce::TextButton::buttonColourId, zinc800.withAlpha(0.8f));
    resetButton.setColour(juce::TextButton::textColourOffId, zinc300);
    resetButton.onClick = [this]
    {
        for (auto* prm : proc.getParameters())
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*>(prm))
            {
                rp->beginChangeGesture();
                rp->setValueNotifyingHost(rp->getDefaultValue());
                rp->endChangeGesture();
            }
    };
    addAndMakeVisible(resetButton);

    setSize(920, 520);
}

StudioReverbAudioProcessorEditor::~StudioReverbAudioProcessorEditor() {}

void StudioReverbAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(zinc950);

    auto card = getLocalBounds().toFloat().reduced(12.0f);
    g.setColour(zinc900.withAlpha(0.9f));
    g.fillRoundedRectangle(card, 16.0f);
    g.setColour(zinc800);
    g.drawRoundedRectangle(card, 16.0f, 1.0f);

    // header
    g.setColour(juce::Colour(0xff38bdf8).withAlpha(0.1f));
    g.fillRoundedRectangle(32.0f, 30.0f, 48.0f, 48.0f, 12.0f);
    g.setColour(juce::Colour(0xff38bdf8).withAlpha(0.3f));
    g.drawRoundedRectangle(32.0f, 30.0f, 48.0f, 48.0f, 12.0f, 1.0f);
    // simple wave glyph
    juce::Path wave;
    wave.startNewSubPath(42.0f, 54.0f);
    for (float x = 0.0f; x <= 28.0f; x += 1.0f)
        wave.lineTo(42.0f + x, 54.0f + std::sin(x / 28.0f * juce::MathConstants<float>::twoPi * 1.5f) * 7.0f);
    g.setColour(sky400);
    g.strokePath(wave, juce::PathStrokeType(2.0f));

    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(22.0f, juce::Font::bold));
    g.drawText("Studio Reverb", 94, 30, 300, 26, juce::Justification::left);
    g.setColour(zinc400);
    g.setFont(juce::FontOptions(12.0f));
    g.drawText("Stereo algorithmic room & hall processor with real-time convolution & tone filters",
               94, 56, 600, 18, juce::Justification::left);

    g.setColour(zinc800);
    g.drawHorizontalLine(92, 32.0f, (float) getWidth() - 32.0f);

    // panels
    for (auto r : { spacePanel, tonePanel })
    {
        g.setColour(zinc950.withAlpha(0.5f));
        g.fillRoundedRectangle(r.toFloat(), 12.0f);
        g.setColour(zinc800.withAlpha(0.9f));
        g.drawRoundedRectangle(r.toFloat().reduced(0.5f), 12.0f, 1.0f);
    }
    g.setColour(zinc300);
    g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    g.drawText("SPACE & ACOUSTICS", spacePanel.getX() + 20, spacePanel.getY() + 12, 300, 16, juce::Justification::left);
    g.drawText("TONE & OUTPUT", tonePanel.getX() + 20, tonePanel.getY() + 12, 300, 16, juce::Justification::left);
}

void StudioReverbAudioProcessorEditor::resized()
{
    auto r = getLocalBounds().reduced(32);
    resetButton.setBounds(getWidth() - 32 - 80, 42, 80, 26);
    r.removeFromTop(70);
    spectrum.setBounds(r.removeFromTop(96));
    r.removeFromTop(18);

    spacePanel = r.removeFromLeft((int) (r.getWidth() * 0.58f));
    r.removeFromLeft(16);
    tonePanel = r;

    auto layoutRow = [](juce::Rectangle<int> panel, std::vector<std::unique_ptr<Knob>>& knobs)
    {
        auto area = panel.reduced(12, 0).withTrimmedTop(44).withHeight(124);
        const int n = (int) knobs.size();
        const int slot = area.getWidth() / n;
        for (int i = 0; i < n; ++i)
        {
            auto cell = juce::Rectangle<int>(area.getX() + i * slot, area.getY(), slot, area.getHeight());
            const int kw = knobs[(size_t) i]->preferredWidth();
            knobs[(size_t) i]->setBounds(cell.withSizeKeepingCentre(kw, cell.getHeight()));
        }
    };
    layoutRow(spacePanel, spaceKnobs);
    layoutRow(tonePanel, toneKnobs);
}
