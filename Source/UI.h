#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
// Palette: dark theme, one accent colour
//==============================================================================
namespace Theme
{
    const juce::Colour bg      { 0xff0e1014 };
    const juce::Colour panel   { 0xff161921 };
    const juce::Colour track   { 0xff2a2f3b };
    const juce::Colour accent  { 0xff3ee6c1 };
    const juce::Colour text    { 0xffe8eaf0 };
    const juce::Colour muted   { 0xff7d8596 };

    inline juce::Font font (float size, bool bold = false)
    {
        return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
    }
}

//==============================================================================
// Look and feel: flat arc knobs and pill buttons
//==============================================================================
class BassLookAndFeel : public juce::LookAndFeel_V4
{
public:
    BassLookAndFeel()
    {
        setColour (juce::TextButton::textColourOffId, Theme::text);
        setColour (juce::TextButton::textColourOnId,  Theme::bg);
        setColour (juce::TooltipWindow::backgroundColourId, Theme::panel);
        setColour (juce::TooltipWindow::textColourId,       Theme::text);
        setColour (juce::TooltipWindow::outlineColourId,    Theme::track);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider& s) override
    {
        auto b = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (6.0f);
        const float radius = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f;
        const auto c = b.getCentre();
        const float arcR = radius - 3.0f;
        const float angle = startAngle + pos * (endAngle - startAngle);

        // track
        juce::Path track;
        track.addCentredArc (c.x, c.y, arcR, arcR, 0.f, startAngle, endAngle, true);
        g.setColour (Theme::track);
        g.strokePath (track, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));

        // value arc (bipolar knobs fill from the centre)
        const double mn = s.getMinimum(), mx = s.getMaximum();
        const float zeroProp = mn < 0.0 ? (float) ((0.0 - mn) / (mx - mn)) : 0.0f;
        const float zeroAngle = startAngle + zeroProp * (endAngle - startAngle);

        juce::Path val;
        val.addCentredArc (c.x, c.y, arcR, arcR, 0.f,
                           juce::jmin (zeroAngle, angle), juce::jmax (zeroAngle, angle), true);
        g.setColour (s.isEnabled() ? Theme::accent : Theme::muted);
        g.strokePath (val, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));

        // knob body
        const float inner = arcR - 11.0f;
        g.setColour (Theme::panel.brighter (0.08f));
        g.fillEllipse (c.x - inner, c.y - inner, inner * 2.f, inner * 2.f);
        g.setColour (Theme::track);
        g.drawEllipse (c.x - inner, c.y - inner, inner * 2.f, inner * 2.f, 1.0f);

        // pointer
        juce::Path p;
        p.addRoundedRectangle (-1.5f, -inner + 4.0f, 3.0f, inner * 0.42f, 1.5f);
        g.setColour (Theme::text);
        g.fillPath (p, juce::AffineTransform::rotation (angle).translated (c.x, c.y));
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                               bool highlighted, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        const bool on = b.getToggleState();
        const float corner = r.getHeight() * 0.5f;

        if (on)
            g.setColour (Theme::accent);
        else
            g.setColour (Theme::panel.brighter (down ? 0.2f : (highlighted ? 0.12f : 0.04f)));
        g.fillRoundedRectangle (r, corner);

        if (! on)
        {
            g.setColour (Theme::track);
            g.drawRoundedRectangle (r, corner, 1.0f);
        }
    }

    juce::Font getTextButtonFont (juce::TextButton&, int) override { return Theme::font (14.0f, true); }
};

//==============================================================================
// A knob with a name and a live value readout
//==============================================================================
class KnobComp : public juce::Component
{
public:
    KnobComp (juce::AudioProcessorValueTreeState& apvts, const juce::String& id,
              const juce::String& title, const juce::String& tip)
        : attachment (apvts, id, slider)
    {
        slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f,
                                    juce::MathConstants<float>::pi * 2.8f, true);
        slider.setMouseDragSensitivity (200);
        slider.setTooltip (tip);

        if (auto* p = apvts.getParameter (id))
            slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));

        name.setText (title, juce::dontSendNotification);
        name.setJustificationType (juce::Justification::centred);
        name.setFont (Theme::font (15.0f, true));
        name.setColour (juce::Label::textColourId, Theme::text);
        name.setInterceptsMouseClicks (false, false);

        value.setJustificationType (juce::Justification::centred);
        value.setFont (Theme::font (13.0f));
        value.setColour (juce::Label::textColourId, Theme::muted);
        value.setInterceptsMouseClicks (false, false);

        slider.onValueChange = [this] { updateValue(); };

        addAndMakeVisible (slider);
        addAndMakeVisible (name);
        addAndMakeVisible (value);
        updateValue();
    }

    void resized() override
    {
        auto r = getLocalBounds();
        value.setBounds (r.removeFromBottom (18));
        name.setBounds  (r.removeFromBottom (22));
        slider.setBounds (r);
    }

private:
    void updateValue()
    {
        value.setText (slider.getTextFromValue (slider.getValue()), juce::dontSendNotification);
    }

    juce::Slider slider;
    juce::Label name, value;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
};

//==============================================================================
// Live low-end spectrum (20 Hz - 2 kHz) with the crossover marked
//==============================================================================
class SpectrumView : public juce::Component, private juce::Timer
{
public:
    explicit SpectrumView (BassFeelProcessor& p)
        : proc (p),
          fft (BassFeelProcessor::fftOrder),
          window ((size_t) BassFeelProcessor::fftSize, juce::dsp::WindowingFunction<float>::hann)
    {
        points.fill (0.0f);
        fftData.fill (0.0f);
        setInterceptsMouseClicks (false, false);
        startTimerHz (30);
    }

    void paint (juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat();
        g.setColour (Theme::panel);
        g.fillRoundedRectangle (area, 14.0f);

        auto plot = area.reduced (14.0f, 12.0f);
        plot.removeFromBottom (14.0f);

        // grid
        g.setFont (Theme::font (11.0f));
        for (float f : { 30.f, 50.f, 100.f, 200.f, 500.f, 1000.f })
        {
            const float x = plot.getX() + freqToProp (f) * plot.getWidth();
            g.setColour (Theme::track.withAlpha (0.6f));
            g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
            g.setColour (Theme::muted);
            const juce::String label = f >= 1000.f ? juce::String ((int) (f / 1000.f)) + "k" : juce::String ((int) f);
            g.drawText (label, (int) x - 20, (int) plot.getBottom() + 2, 40, 12, juce::Justification::centred);
        }

        // spectrum
        juce::Path line;
        for (int k = 0; k < numPoints; ++k)
        {
            const float x = plot.getX() + plot.getWidth() * (float) k / (float) (numPoints - 1);
            const float y = plot.getBottom() - points[(size_t) k] * plot.getHeight();
            if (k == 0) line.startNewSubPath (x, y); else line.lineTo (x, y);
        }
        juce::Path fill (line);
        fill.lineTo (plot.getRight(), plot.getBottom());
        fill.lineTo (plot.getX(), plot.getBottom());
        fill.closeSubPath();

        g.setGradientFill (juce::ColourGradient (Theme::accent.withAlpha (0.35f), 0.f, plot.getY(),
                                                 Theme::accent.withAlpha (0.0f), 0.f, plot.getBottom(), false));
        g.fillPath (fill);
        g.setColour (Theme::accent);
        g.strokePath (line, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved));

        // crossover marker
        if (auto* cx = proc.apvts.getRawParameterValue (IDs::crossover))
        {
            const float f = cx->load();
            const float x = plot.getX() + freqToProp (f) * plot.getWidth();
            g.setColour (Theme::text.withAlpha (0.55f));
            for (float yy = plot.getY(); yy < plot.getBottom(); yy += 8.0f)
                g.drawVerticalLine ((int) x, yy, yy + 4.0f);
            g.setFont (Theme::font (11.0f, true));
            g.drawText (juce::String ((int) f) + " Hz", (int) x + 4, (int) plot.getY(), 60, 14,
                        juce::Justification::left);
        }
    }

private:
    static constexpr int numPoints = 140;
    static constexpr float minF = 20.0f, maxF = 2000.0f;

    static float freqToProp (float f) { return std::log (f / minF) / std::log (maxF / minF); }

    void timerCallback() override
    {
        const double sr = proc.getSampleRate();
        if (sr <= 0.0) return;

        constexpr int N = BassFeelProcessor::fftSize;
        const int pos = proc.scopePos.load (std::memory_order_relaxed);
        for (int i = 0; i < N; ++i)
            fftData[(size_t) i] = proc.scopeBuf[(size_t) ((pos + i) & (N - 1))];
        std::fill (fftData.begin() + N, fftData.end(), 0.0f);

        window.multiplyWithWindowingTable (fftData.data(), (size_t) N);
        fft.performFrequencyOnlyForwardTransform (fftData.data());

        for (int k = 0; k < numPoints; ++k)
        {
            const float t0 = (float) k / (float) numPoints;
            const float t1 = (float) (k + 1) / (float) numPoints;
            const float f0 = minF * std::pow (maxF / minF, t0);
            const float f1 = minF * std::pow (maxF / minF, t1);
            const int b0 = juce::jlimit (1, N / 2 - 2, (int) std::floor (f0 * N / sr));
            const int b1 = juce::jlimit (b0, N / 2 - 1, (int) std::ceil  (f1 * N / sr));

            float mag = 0.0f;
            for (int b = b0; b <= b1; ++b)
                mag = juce::jmax (mag, fftData[(size_t) b]);

            const float db   = juce::Decibels::gainToDecibels (mag / (N / 4.0f), -100.0f);
            const float norm = juce::jlimit (0.0f, 1.0f, (db + 80.0f) / 70.0f);
            auto& p = points[(size_t) k];
            p = norm > p ? norm : p * 0.88f + norm * 0.12f;
        }
        repaint();
    }

    BassFeelProcessor& proc;
    juce::dsp::FFT fft;
    juce::dsp::WindowingFunction<float> window;
    std::array<float, 2 * BassFeelProcessor::fftSize> fftData;
    std::array<float, numPoints> points;
};

//==============================================================================
// Slim output level meter
//==============================================================================
class MeterBar : public juce::Component, private juce::Timer
{
public:
    explicit MeterBar (BassFeelProcessor& p) : proc (p) { startTimerHz (30); }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (Theme::track);
        g.fillRoundedRectangle (r, r.getHeight() * 0.5f);

        const float db = juce::Decibels::gainToDecibels (shown, -60.0f);
        const float prop = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
        g.setColour (db > -1.0f ? juce::Colour (0xffff6b6b) : Theme::accent);
        g.fillRoundedRectangle (r.withWidth (juce::jmax (r.getHeight(), r.getWidth() * prop)),
                                r.getHeight() * 0.5f);
    }

private:
    void timerCallback() override
    {
        const float v = proc.outLevel.load();
        shown = v > shown ? v : shown * 0.85f + v * 0.15f;
        repaint();
    }

    BassFeelProcessor& proc;
    float shown = 0.0f;
};
