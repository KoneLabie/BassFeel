#include "PluginEditor.h"

namespace
{
    struct Preset { const char* name; float weight, punch, tight, grit, width; };

    // Edit these freely - values use the same units as the knobs
    const Preset presets[] =
    {
        { "Clean",      0.f,   0.f,   0.f,   0.f, 100.f },
        { "Deep",       4.f, -10.f,  20.f,  10.f, 100.f },
        { "Punchy",     1.f,  55.f,  40.f,  25.f, 100.f },
        { "Fat & Warm", 3.f,  10.f,   0.f,  45.f, 110.f },
        { "808 Tight",  2.f,  30.f,  70.f,  20.f, 100.f },
    };

    void setParam (juce::AudioProcessorValueTreeState& apvts, const char* id, float value)
    {
        if (auto* p = apvts.getParameter (id))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 (value));
            p->endChangeGesture();
        }
    }
}

BassFeelEditor::BassFeelEditor (BassFeelProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      spectrum (p), meter (p),
      weight (p.apvts, IDs::weight, "Weight", "Adds or removes sub-bass weight (the low band below the crossover)"),
      punch  (p.apvts, IDs::punch,  "Punch",  "Right = sharper attack, left = softer, rounder attack"),
      tight  (p.apvts, IDs::tight,  "Tight",  "Shortens how long notes ring out so bass and kick don't smear"),
      grit   (p.apvts, IDs::grit,   "Grit",   "Adds harmonics so the bass is audible on phone and laptop speakers"),
      width  (p.apvts, IDs::width,  "Width",  "Stereo width of the upper bass. Lows always stay centred"),
      crossover (p.apvts, IDs::crossover, "Crossover", "Where 'sub' ends and 'upper bass' begins"),
      mono   (p.apvts, IDs::mono,   "Mono Lows", "How much of the low band is collapsed to mono (100% = fully mono)"),
      output (p.apvts, IDs::output, "Output", "Output level. Use it to match loudness when comparing")
{
    setLookAndFeel (&laf);

    addAndMakeVisible (spectrum);
    addAndMakeVisible (meter);
    for (auto* k : { &weight, &punch, &tight, &grit, &width, &crossover, &mono, &output })
        addAndMakeVisible (k);

    int idx = 0;
    for (auto& preset : presets)
    {
        auto* b = chips.add (new juce::TextButton (preset.name));
        b->setClickingTogglesState (true);
        b->setRadioGroupId (1001);
        b->setTooltip ("Load the '" + juce::String (preset.name) + "' starting point");
        const int i = idx++;
        b->onClick = [this, i] { applyPreset (i); };
        addAndMakeVisible (b);
    }

    advancedButton.setClickingTogglesState (true);
    advancedButton.setTooltip ("Show crossover, mono-lows and output controls");
    advancedButton.onClick = [this] { setAdvancedVisible (advancedButton.getToggleState()); };
    addAndMakeVisible (advancedButton);

    for (auto* k : { &crossover, &mono, &output })
        k->setVisible (false);

    setResizable (true, true);
    setResizeLimits (640, 460, 1300, 900);
    setSize (760, 520);
}

BassFeelEditor::~BassFeelEditor()
{
    setLookAndFeel (nullptr);
}

void BassFeelEditor::applyPreset (int i)
{
    const auto& p = presets[i];
    setParam (proc.apvts, IDs::weight, p.weight);
    setParam (proc.apvts, IDs::punch,  p.punch);
    setParam (proc.apvts, IDs::tight,  p.tight);
    setParam (proc.apvts, IDs::grit,   p.grit);
    setParam (proc.apvts, IDs::width,  p.width);
}

void BassFeelEditor::setAdvancedVisible (bool show)
{
    if (show == showAdvanced) return;
    showAdvanced = show;
    for (auto* k : { &crossover, &mono, &output })
        k->setVisible (show);
    setSize (getWidth(), getHeight() + (show ? advancedExtra : -advancedExtra));
}

void BassFeelEditor::paint (juce::Graphics& g)
{
    g.fillAll (Theme::bg);

    auto header = getLocalBounds().reduced (24).removeFromTop (48);
    g.setColour (Theme::text);
    g.setFont (Theme::font (26.0f, true));
    g.drawText ("Bass", header.getX(), header.getY(), 64, 36, juce::Justification::centredLeft);
    g.setColour (Theme::accent);
    g.drawText ("Feel", header.getX() + 60, header.getY(), 70, 36, juce::Justification::centredLeft);
    g.setColour (Theme::muted);
    g.setFont (Theme::font (13.0f));
    g.drawText ("Low-end tuner", header.getX() + 130, header.getY() + 6, 140, 28, juce::Justification::centredLeft);
    g.drawText ("OUT", header.getRight() - 210, header.getY(), 34, 36, juce::Justification::centredLeft);
}

void BassFeelEditor::resized()
{
    auto r = getLocalBounds().reduced (24);

    auto header = r.removeFromTop (48);
    meter.setBounds (header.removeFromRight (160).withSizeKeepingCentre (160, 8).translated (0, -2));

    r.removeFromTop (6);
    auto presetRow = r.removeFromTop (34);
    for (auto* c : chips)
    {
        c->setBounds (presetRow.removeFromLeft (108));
        presetRow.removeFromLeft (8);
    }

    r.removeFromTop (16);

    auto footer = r.removeFromBottom (32);
    advancedButton.setBounds (footer.removeFromLeft (120));

    if (showAdvanced)
    {
        r.removeFromBottom (6);
        auto adv = r.removeFromBottom (advancedExtra - 8);
        const int w = adv.getWidth() / 5;
        adv.removeFromLeft (w / 2);                    // centre the 3 knobs under the 5 above
        for (auto* k : { &crossover, &mono, &output })
            k->setBounds (adv.removeFromLeft (w).reduced (6, 0));
    }

    auto knobRow = r.removeFromBottom (150);
    const int kw = knobRow.getWidth() / 5;
    for (auto* k : { &weight, &punch, &tight, &grit, &width })
        k->setBounds (knobRow.removeFromLeft (kw).reduced (6, 0));

    r.removeFromBottom (16);
    spectrum.setBounds (r);
}
