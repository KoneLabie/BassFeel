#pragma once
#include <JuceHeader.h>
#include <array>

namespace IDs
{
    inline constexpr const char* weight    = "weight";
    inline constexpr const char* punch     = "punch";
    inline constexpr const char* tight     = "tight";
    inline constexpr const char* grit      = "grit";
    inline constexpr const char* width     = "width";
    inline constexpr const char* crossover = "crossover";
    inline constexpr const char* mono      = "mono";
    inline constexpr const char* output    = "output";
}

class BassFeelProcessor : public juce::AudioProcessor
{
public:
    BassFeelProcessor();
    ~BassFeelProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "BassFeel"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    // ---- data shared with the UI (lock-free, visual use only) ----
    static constexpr int fftOrder = 14;               // 16384 points -> ~2.7 Hz bins at 44.1k
    static constexpr int fftSize  = 1 << fftOrder;
    std::array<float, fftSize> scopeBuf {};
    std::atomic<int>   scopePos { 0 };
    std::atomic<float> outLevel { 0.0f };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void processChunk (juce::AudioBuffer<float>& buffer, int offset, int n, int nCh);

    // cached raw parameter pointers
    std::atomic<float> *pWeight, *pPunch, *pTight, *pGrit, *pWidth, *pCross, *pMono, *pOut;

    // DSP
    juce::dsp::LinkwitzRileyFilter<float> xover, satHP;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None> lowDelay, highDelay;

    juce::AudioBuffer<float> lowBuf, highBuf, satBuf;
    std::vector<float> gritBuf;

    juce::SmoothedValue<float> weightSm, punchSm, tightSm, gritSm, widthSm, monoSm, outSm;

    // envelope followers (linked across channels)
    float tightFast = 0, tightRef = 0, punchFast = 0, punchSlow = 0;
    float aTF = 0, rTF = 0, rTR = 0, aPF = 0, rPF = 0, aPS = 0, rPS = 0;

    double sr = 44100.0;
    int maxBlock = 512;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BassFeelProcessor)
};
