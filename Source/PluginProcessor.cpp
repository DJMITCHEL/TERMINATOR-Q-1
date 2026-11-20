#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout TerminatorQAudioProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    for (int i = 0; i < TQ::numBands; ++i)
    {
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { "g" + juce::String (i), 1 },
            "Band " + juce::String (i + 1) + " Gain",
            juce::NormalisableRange<float> (-TQ::maxGainDb, TQ::maxGainDb, 0.01f),
            0.0f,
            juce::AudioParameterFloatAttributes().withLabel ("dB")));
    }

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "amount", 1 }, "Amount",
        juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 100.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "output", 1 }, "Output",
        juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "bypass", 1 }, "Bypass", false));

    juce::StringArray names;
    for (int i = 0; i < TQ::numPresets; ++i)
        names.add (TQ::presets[i].name);

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "preset", 1 }, "Target", names, 0));

    return layout;
}

//==============================================================================
TerminatorQAudioProcessor::TerminatorQAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    for (auto& s : spectrumDb) s.store (-120.0f);
    for (auto& a : avgDb)      a.store (-120.0f);

    amountParam = apvts.getRawParameterValue ("amount");
    outputParam = apvts.getRawParameterValue ("output");
    bypassParam = apvts.getRawParameterValue ("bypass");

    for (int i = 0; i < TQ::numBands; ++i)
        gainParams[(size_t) i] = apvts.getRawParameterValue ("g" + juce::String (i));
}

bool TerminatorQAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet()  == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

//==============================================================================
void TerminatorQAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) juce::jmax (1, samplesPerBlock), 2 };

    for (auto& f : filters)
    {
        f.prepare (spec);
        f.reset();
    }
    lastGain.fill (1000.0f); // fuerza actualizar los coeficientes en el primer bloque

    // rangos de bins FFT para cada banda logarítmica
    const float df = (float) (sampleRate / TQ::fftSize);
    const int lastBin = TQ::fftSize / 2 - 1;
    for (int b = 0; b < TQ::numLogBins; ++b)
    {
        const float f1 = TQ::logBinEdge ((float) b);
        const float f2 = TQ::logBinEdge ((float) b + 1.0f);
        binK1[(size_t) b]  = (int) std::ceil (f1 / df);
        binK2[(size_t) b]  = juce::jmin (lastBin, (int) std::floor (f2 / df));
        binPos[(size_t) b] = TQ::logBinCentre (b) / df;
    }

    ring.fill (0.0f);
    writePos = 0;
    hopCounter = 0;
}

//==============================================================================
void TerminatorQAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    // el análisis siempre mira la señal de entrada (antes del EQ)
    pushForAnalysis (buffer);

    if (bypassParam->load() > 0.5f)
        return;

    const float amount = amountParam->load() * 0.01f;
    const double sr = getSampleRate();

    for (int i = 0; i < TQ::numBands; ++i)
    {
        const float g = juce::jlimit (-TQ::maxGainDb, TQ::maxGainDb, gainParams[(size_t) i]->load() * amount);
        if (std::abs (g - lastGain[(size_t) i]) > 0.001f)
        {
            lastGain[(size_t) i] = g;
            auto coeffs = TQ::makeBandCoeffs (i, g, sr);
            *filters[(size_t) i].state = *coeffs;
        }
    }

    juce::dsp::AudioBlock<float> block (buffer);
    juce::dsp::ProcessContextReplacing<float> ctx (block);
    for (auto& f : filters)
        f.process (ctx);

    buffer.applyGain (juce::Decibels::decibelsToGain (outputParam->load()));
}

//==============================================================================
void TerminatorQAudioProcessor::pushForAnalysis (const juce::AudioBuffer<float>& buffer)
{
    const int numCh = buffer.getNumChannels();
    if (numCh == 0) return;

    const float* l = buffer.getReadPointer (0);
    const float* r = numCh > 1 ? buffer.getReadPointer (1) : l;

    for (int n = 0; n < buffer.getNumSamples(); ++n)
    {
        ring[(size_t) writePos] = 0.5f * (l[n] + r[n]);
        writePos = (writePos + 1) & (TQ::fftSize - 1);

        if (++hopCounter >= TQ::hopSize)
        {
            hopCounter = 0;
            runFFT();
        }
    }
}

void TerminatorQAudioProcessor::runFFT()
{
    if (resetAvg.exchange (false))
    {
        powerSum.fill (0.0);
        frameCount = 0;
        analyzedFrames.store (0);
    }

    for (int i = 0; i < TQ::fftSize; ++i)
        fftData[(size_t) i] = ring[(size_t) ((writePos + i) & (TQ::fftSize - 1))];
    std::fill (fftData.begin() + TQ::fftSize, fftData.end(), 0.0f);

    window.multiplyWithWindowingTable (fftData.data(), (size_t) TQ::fftSize);
    fft.performFrequencyOnlyForwardTransform (fftData.data());

    const float scale = 4.0f / (float) TQ::fftSize;   // 0 dB = seno a fondo de escala (ventana Hann)
    const bool doAnalyze = analyzing.load();

    std::array<float, TQ::numLogBins> pw {};
    double totalPower = 0.0;

    for (int b = 0; b < TQ::numLogBins; ++b)
    {
        float p;
        const int k1 = binK1[(size_t) b], k2 = binK2[(size_t) b];

        if (k2 >= k1)
        {
            double s = 0.0;
            for (int k = k1; k <= k2; ++k)
            {
                const float m = fftData[(size_t) k] * scale;
                s += (double) m * m;
            }
            p = (float) (s / (double) (k2 - k1 + 1));
        }
        else
        {
            const float pos = binPos[(size_t) b];
            const int k = juce::jlimit (0, TQ::fftSize / 2 - 1, (int) pos);
            const float frac = pos - (float) k;
            const float m = (fftData[(size_t) k] * (1.0f - frac) + fftData[(size_t) k + 1] * frac) * scale;
            p = m * m;
        }

        pw[(size_t) b] = p;
        totalPower += p;

        const float db  = 10.0f * std::log10 (p + 1.0e-12f);
        const float old = spectrumDb[(size_t) b].load();
        const float coef = db > old ? 0.6f : 0.2f;
        spectrumDb[(size_t) b].store (old + (db - old) * coef);
    }

    // no promediar silencio
    if (doAnalyze && totalPower > 1.0e-9)
    {
        for (int b = 0; b < TQ::numLogBins; ++b)
            powerSum[(size_t) b] += (double) pw[(size_t) b];

        ++frameCount;
        for (int b = 0; b < TQ::numLogBins; ++b)
            avgDb[(size_t) b].store (10.0f * std::log10 ((float) (powerSum[(size_t) b] / frameCount) + 1.0e-12f));

        analyzedFrames.store (frameCount);
    }
}

//==============================================================================
void TerminatorQAudioProcessor::startAnalysis()
{
    analyzedFrames.store (0);
    resetAvg.store (true);
    analyzing.store (true);
}

void TerminatorQAudioProcessor::stopAnalysis()
{
    analyzing.store (false);
}

void TerminatorQAudioProcessor::setBandGain (int band, float db)
{
    if (auto* p = apvts.getParameter ("g" + juce::String (band)))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (juce::jlimit (-TQ::maxGainDb, TQ::maxGainDb, db)));
        p->endChangeGesture();
    }
}

void TerminatorQAudioProcessor::resetAllBands()
{
    for (int i = 0; i < TQ::numBands; ++i)
        setBandGain (i, 0.0f);
}

bool TerminatorQAudioProcessor::computeMatch (int presetIndex)
{
    if (! hasAnalysis())
        return false;

    presetIndex = juce::jlimit (0, TQ::numPresets - 1, presetIndex);
    const auto& preset = TQ::presets[presetIndex];

    std::array<float, TQ::numLogBins> tgt {}, ana {}, diff {};
    for (int b = 0; b < TQ::numLogBins; ++b)
    {
        tgt[(size_t) b] = TQ::targetDb (preset, TQ::logBinCentre (b));
        ana[(size_t) b] = avgDb[(size_t) b].load();
    }

    const float meanT = TQ::meanInRange (tgt);
    const float meanA = TQ::meanInRange (ana);

    for (int b = 0; b < TQ::numLogBins; ++b)
        diff[(size_t) b] = (tgt[(size_t) b] - meanT) - (ana[(size_t) b] - meanA);

    // cada banda del EQ promedia la diferencia a su alrededor (gaussiana de 0.6 octavas)
    for (int i = 0; i < TQ::numBands; ++i)
    {
        double wSum = 0.0, gSum = 0.0;
        for (int b = 0; b < TQ::numLogBins; ++b)
        {
            const float d = std::log2 (TQ::logBinCentre (b) / TQ::bandFreqs[(size_t) i]) / 0.6f;
            const double w = std::exp (-0.5 * (double) d * d);
            wSum += w;
            gSum += w * (double) diff[(size_t) b];
        }
        setBandGain (i, (float) (gSum / juce::jmax (1.0e-9, wSum)));
    }

    return true;
}

//==============================================================================
juce::AudioProcessorEditor* TerminatorQAudioProcessor::createEditor()
{
    return new TerminatorQAudioProcessorEditor (*this);
}

void TerminatorQAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void TerminatorQAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TerminatorQAudioProcessor();
}
