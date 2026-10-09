#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "UI.h"

class BassFeelEditor : public juce::AudioProcessorEditor
{
public:
    explicit BassFeelEditor (BassFeelProcessor&);
    ~BassFeelEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void applyPreset (int index);
    void setAdvancedVisible (bool show);

    BassFeelProcessor& proc;
    BassLookAndFeel laf;
    juce::TooltipWindow tooltips { this, 450 };

    SpectrumView spectrum;
    MeterBar meter;

    // macro knobs
    KnobComp weight, punch, tight, grit, width;
    // advanced knobs
    KnobComp crossover, mono, output;

    juce::OwnedArray<juce::TextButton> chips;
    juce::TextButton advancedButton { "Advanced" };
    bool showAdvanced = false;

    static constexpr int advancedExtra = 138;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BassFeelEditor)
};
