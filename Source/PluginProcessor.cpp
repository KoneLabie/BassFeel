#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
// Parameters
//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout BassFeelProcessor::createLayout()
{
    using P = juce::AudioParameterFloat;
    using R = juce::NormalisableRange<float>;
    using A = juce::AudioParameterFloatAttributes;

    auto dbStr  = [] (float v, int) { return juce::String (v, 1) + " dB"; };
    auto pctStr = [] (float v, int) { return juce::String ((int) std::round (v)) + "%"; };
    auto hzStr  = [] (float v, int) { return juce::String ((int) std::round (v)) + " Hz"; };

    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<P> (juce::ParameterID { IDs::weight, 1 }, "Weight",
                R (-12.f, 12.f, 0.1f), 0.f, A().withStringFromValueFunction (dbStr)));
    layout.add (std::make_unique<P> (juce::ParameterID { IDs::punch, 1 }, "Punch",
                R (-100.f, 100.f, 1.f), 0.f, A().withStringFromValueFunction (pctStr)));
    layout.add (std::make_unique<P> (juce::ParameterID { IDs::tight, 1 }, "Tight",
                R (0.f, 100.f, 1.f), 0.f, A().withStringFromValueFunction (pctStr)));
    layout.add (std::make_unique<P> (juce::ParameterID { IDs::grit, 1 }, "Grit",
                R (0.f, 100.f, 1.f), 0.f, A().withStringFromValueFunction (pctStr)));
    layout.add (std::make_unique<P> (juce::ParameterID { IDs::width, 1 }, "Width",
                R (0.f, 200.f, 1.f), 100.f, A().withStringFromValueFunction (pctStr)));
    layout.add (std::make_unique<P> (juce::ParameterID { IDs::crossover, 1 }, "Crossover",
                R (60.f, 250.f, 1.f, 0.7f), 120.f, A().withStringFromValueFunction (hzStr)));
    layout.add (std::make_unique<P> (juce::ParameterID { IDs::mono, 1 }, "Mono Lows",
                R (0.f, 100.f, 1.f), 100.f, A().withStringFromValueFunction (pctStr)));
    layout.add (std::make_unique<P> (juce::ParameterID { IDs::output, 1 }, "Output",
                R (-12.f, 12.f, 0.1f), 0.f, A().withStringFromValueFunction (dbStr)));
    return layout;
}

BassFeelProcessor::BassFeelProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    pWeight = apvts.getRawParameterValue (IDs::weight);
    pPunch  = apvts.getRawParameterValue (IDs::punch);
    pTight  = apvts.getRawParameterValue (IDs::tight);
    pGrit   = apvts.getRawParameterValue (IDs::grit);
    pWidth  = apvts.getRawParameterValue (IDs::width);
    pCross  = apvts.getRawParameterValue (IDs::crossover);
    pMono   = apvts.getRawParameterValue (IDs::mono);
    pOut    = apvts.getRawParameterValue (IDs::output);
}

bool BassFeelProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in  = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();
    if (in != out) return false;
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

//==============================================================================
// Prepare
//==============================================================================
void BassFeelProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    maxBlock = juce::jmax (1, samplesPerBlock);

    juce::dsp::ProcessSpec spec { sr, (juce::uint32) maxBlock, 2 };

    xover.prepare (spec);
    xover.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
    satHP.prepare (spec);
    satHP.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);

    // 2x oversampling with linear-phase FIR so latency is exact and we can align the other paths
    oversampling = std::make_unique<juce::dsp::Oversampling<float>> (
        2, 1, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true);
    oversampling->initProcessing ((size_t) maxBlock);
    oversampling->reset();

    const int latency = (int) std::ceil (oversampling->getLatencyInSamples());
    setLatencySamples (latency);

    for (auto* d : { &lowDelay, &highDelay })
    {
        d->setMaximumDelayInSamples (latency + 1);
        d->prepare (spec);
        d->setDelay ((float) latency);
        d->reset();
    }

    lowBuf.setSize  (2, maxBlock);
    highBuf.setSize (2, maxBlock);
    satBuf.setSize  (2, maxBlock);
    gritBuf.assign ((size_t) maxBlock, 0.f);

    const double smoothTime = 0.03;
    weightSm.reset (sr, smoothTime); punchSm.reset (sr, smoothTime); tightSm.reset (sr, smoothTime);
    gritSm.reset (sr, smoothTime);   widthSm.reset (sr, smoothTime); monoSm.reset (sr, smoothTime);
    outSm.reset (sr, smoothTime);

    weightSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pWeight->load()));
    punchSm.setCurrentAndTargetValue  (pPunch->load() * 0.01f);
    tightSm.setCurrentAndTargetValue  (pTight->load() * 0.01f);
    gritSm.setCurrentAndTargetValue   (pGrit->load()  * 0.01f);
    widthSm.setCurrentAndTargetValue  (pWidth->load() * 0.01f);
    monoSm.setCurrentAndTargetValue   (pMono->load()  * 0.01f);
    outSm.setCurrentAndTargetValue    (juce::Decibels::decibelsToGain (pOut->load()));

    auto coef = [this] (float ms) { return std::exp (-1.0f / (ms * 0.001f * (float) sr)); };
    aTF = coef (1.0f);   rTF = coef (60.0f);  rTR = coef (400.0f);   // tightness detector
    aPF = coef (0.5f);   rPF = coef (30.0f);                         // punch: fast
    aPS = coef (25.0f);  rPS = coef (120.0f);                        // punch: slow

    tightFast = tightRef = punchFast = punchSlow = 0.f;
}

//==============================================================================
// Process
//==============================================================================
void BassFeelProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int total = buffer.getNumSamples();
    const int nCh   = juce::jmin (2, getTotalNumInputChannels());

    for (int ch = nCh; ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, total);

    for (int start = 0; start < total; start += maxBlock)
        processChunk (buffer, start, juce::jmin (maxBlock, total - start), nCh);
}

static inline float follow (float env, float x, float a, float r)
{
    const float c = x > env ? a : r;
    return c * env + (1.0f - c) * x;
}

void BassFeelProcessor::processChunk (juce::AudioBuffer<float>& buffer, int offset, int n, int nCh)
{
    weightSm.setTargetValue (juce::Decibels::decibelsToGain (pWeight->load()));
    punchSm.setTargetValue  (pPunch->load() * 0.01f);
    tightSm.setTargetValue  (pTight->load() * 0.01f);
    gritSm.setTargetValue   (pGrit->load()  * 0.01f);
    widthSm.setTargetValue  (pWidth->load() * 0.01f);
    monoSm.setTargetValue   (pMono->load()  * 0.01f);
    outSm.setTargetValue    (juce::Decibels::decibelsToGain (pOut->load()));

    const float xf = pCross->load();
    xover.setCutoffFrequency (xf);
    satHP.setCutoffFrequency (xf);

    float* lo[2]; float* hi[2]; float* sa[2];
    for (int ch = 0; ch < 2; ++ch)
    {
        lo[ch] = lowBuf.getWritePointer (ch);
        hi[ch] = highBuf.getWritePointer (ch);
        sa[ch] = satBuf.getWritePointer (ch);
    }

    // 1) Split into low / high with a Linkwitz-Riley crossover (sums flat)
    for (int ch = 0; ch < 2; ++ch)
    {
        const float* in = ch < nCh ? buffer.getReadPointer (ch) + offset : nullptr;
        for (int i = 0; i < n; ++i)
        {
            const float x = in != nullptr ? in[i] : 0.0f;
            float l, h;
            xover.processSample (ch, x, l, h);
            lo[ch][i] = l;
            hi[ch][i] = h;
            sa[ch][i] = x;                       // saturator is fed the FULL signal
        }
    }

    // 2) Oversampled saturation. Feeding the full signal means a pure sub sine still
    //    generates harmonics; we then keep only what lands above the crossover.
    for (int i = 0; i < n; ++i)
        gritBuf[(size_t) i] = gritSm.getNextValue();

    {
        juce::dsp::AudioBlock<float> block (satBuf);
        auto sub = block.getSubBlock (0, (size_t) n);
        auto up  = oversampling->processSamplesUp (sub);
        const int un    = (int) up.getNumSamples();
        const int ratio = juce::jmax (1, un / n);

        for (int ch = 0; ch < 2; ++ch)
        {
            float* p = up.getChannelPointer ((size_t) ch);
            for (int j = 0; j < un; ++j)
            {
                const float g     = gritBuf[(size_t) juce::jmin (n - 1, j / ratio)];
                const float drive = 1.0f + g * 10.0f;
                const float bias  = 0.2f * g;                  // asymmetry -> even harmonics
                p[j] = (std::tanh (drive * p[j] + bias) - std::tanh (bias)) / std::sqrt (drive);
            }
        }
        oversampling->processSamplesDown (sub);
    }

    // 3) Recombine sample by sample
    float* outPtr[2] = { nullptr, nullptr };
    for (int ch = 0; ch < nCh; ++ch)
        outPtr[ch] = buffer.getWritePointer (ch) + offset;

    float peak = 0.f;
    int scopeIdx = scopePos.load (std::memory_order_relaxed);

    for (int i = 0; i < n; ++i)
    {
        const float wG = weightSm.getNextValue();
        const float pn = punchSm.getNextValue();
        const float tg = tightSm.getNextValue();
        const float gr = gritBuf[(size_t) i];
        const float wd = widthSm.getNextValue();
        const float mn = monoSm.getNextValue();
        const float og = outSm.getNextValue();

        float lw[2], hg[2];
        for (int ch = 0; ch < 2; ++ch)
        {
            lowDelay.pushSample  (ch, lo[ch][i]);
            highDelay.pushSample (ch, hi[ch][i]);
            const float l = lowDelay.popSample  (ch);
            const float h = highDelay.popSample (ch);

            float dummy, satHigh;
            satHP.processSample (ch, sa[ch][i], dummy, satHigh);

            lw[ch] = l;
            hg[ch] = h + gr * (satHigh - h);     // grit: crossfade in saturated highs
        }

        // ---- Low band: tighten (expander on decay tail) + weight + mono ----
        float det = std::abs (lw[0]);
        if (nCh == 2) det = juce::jmax (det, std::abs (lw[1]));
        tightFast = follow (tightFast, det, aTF, rTF);
        tightRef  = follow (tightRef,  det, aTF, rTR);

        float tGain = 1.0f;
        if (tg > 0.001f && tightRef > 1.0e-4f)
        {
            const float ratio = juce::jlimit (0.02f, 1.0f, tightFast / tightRef);
            tGain = std::pow (ratio, tg * 2.0f);
        }
        lw[0] *= wG * tGain;
        lw[1] *= wG * tGain;

        if (nCh == 2)
        {
            const float m = 0.5f * (lw[0] + lw[1]);
            const float s = 0.5f * (lw[0] - lw[1]) * (1.0f - mn);
            lw[0] = m + s;  lw[1] = m - s;

            // ---- High band: stereo width ----
            const float hm = 0.5f * (hg[0] + hg[1]);
            const float hs = 0.5f * (hg[0] - hg[1]) * wd;
            hg[0] = hm + hs;  hg[1] = hm - hs;
        }

        float y[2] = { lw[0] + hg[0], lw[1] + hg[1] };

        // ---- Punch: transient shaper (fast vs slow envelope) ----
        float d2 = std::abs (y[0]);
        if (nCh == 2) d2 = juce::jmax (d2, std::abs (y[1]));
        punchFast = follow (punchFast, d2, aPF, rPF);
        punchSlow = follow (punchSlow, d2, aPS, rPS);

        float pGain = 1.0f;
        if (std::abs (pn) > 0.001f && punchSlow > 1.0e-5f)
        {
            float diffDb = 20.0f * std::log10 (juce::jmax (punchFast, 1.0e-6f) / punchSlow);
            diffDb = juce::jlimit (0.0f, 12.0f, diffDb);
            pGain  = juce::Decibels::decibelsToGain (pn * diffDb);
        }

        float scopeSample = 0.f;
        for (int ch = 0; ch < nCh; ++ch)
        {
            const float o = y[ch] * pGain * og;
            outPtr[ch][i] = o;
            peak = juce::jmax (peak, std::abs (o));
            scopeSample += o;
        }
        scopeBuf[(size_t) scopeIdx] = scopeSample / (float) juce::jmax (1, nCh);
        scopeIdx = (scopeIdx + 1) & (fftSize - 1);
    }

    scopePos.store (scopeIdx, std::memory_order_relaxed);
    outLevel.store (juce::jmax (peak, outLevel.load() * 0.92f), std::memory_order_relaxed);
}

//==============================================================================
juce::AudioProcessorEditor* BassFeelProcessor::createEditor()
{
    return new BassFeelEditor (*this);
}

void BassFeelProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void BassFeelProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BassFeelProcessor();
}
