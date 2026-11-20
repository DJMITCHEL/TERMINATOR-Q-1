#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <cmath>

namespace TQ
{
    constexpr int   numBands   = 10;
    constexpr int   fftOrder   = 12;
    constexpr int   fftSize    = 1 << fftOrder;
    constexpr int   hopSize    = 1024;
    constexpr int   numLogBins = 96;
    constexpr float minHz      = 20.0f;
    constexpr float maxHz      = 20000.0f;
    constexpr float maxGainDb  = 9.0f;

    inline constexpr std::array<float, numBands> bandFreqs
        { 40.f, 80.f, 160.f, 320.f, 640.f, 1250.f, 2500.f, 5000.f, 9000.f, 14000.f };

    inline float logBinEdge (float i)   { return minHz * std::pow (maxHz / minHz, i / (float) numLogBins); }
    inline float logBinCentre (int i)   { return logBinEdge ((float) i + 0.5f); }

    // ---------------------------------------------------------------- Targets
    struct TargetPreset
    {
        const char* name;
        float slope;      // dB/octava respecto a 1 kHz
        float bassDb;     // realce de graves
        float bassHz;
        float lowMidDb;   // corte/realce en 350 Hz
        float presDb;     // presencia en 3.5 kHz
        float airDb;      // shelf de aire > 10 kHz
    };

    inline constexpr TargetPreset presets[] =
    {
        { "Trap: Bass-heavy",        -4.5f, 5.0f, 55.f, -2.0f, 0.0f,  0.0f },
        { "Trap: Dark & Wide",       -5.5f, 4.0f, 50.f, -1.0f, -1.0f, -2.0f },
        { "Hip Hop: Full-bodied",    -4.5f, 3.0f, 70.f,  0.5f, 0.5f,  0.5f },
        { "Pop: Bright & Open",      -3.8f, 1.5f, 80.f, -1.5f, 1.5f,  2.5f },
        { "EDM: Punchy",             -4.2f, 3.5f, 60.f, -2.5f, 1.0f,  2.0f },
        { "Rock: Balanced",          -4.2f, 1.0f, 90.f, -1.0f, 2.0f,  1.0f },
        { "Flat: Pink noise",        -3.0f, 0.0f, 80.f,  0.0f, 0.0f,  0.0f }
    };
    inline constexpr int numPresets = (int) (sizeof (presets) / sizeof (presets[0]));

    inline float targetDb (const TargetPreset& p, float f)
    {
        auto oct = [] (float a, float b) { return std::log2 (a / b); };
        auto bump = [&] (float centreHz, float widthOct, float gain)
        {
            const float x = oct (f, centreHz) / widthOct;
            return gain * std::exp (-x * x);
        };

        // la pendiente se aplana por debajo de 70 Hz (el espectro real hace meseta en graves)
        float db = p.slope * oct (juce::jmax (f, 70.0f), 1000.0f);
        db += bump (p.bassHz, 1.0f, p.bassDb);
        db += bump (350.0f, 1.0f, p.lowMidDb);
        db += bump (3500.0f, 1.0f, p.presDb);
        db += p.airDb / (1.0f + std::exp (-3.0f * oct (f, 10000.0f)));

        if (f < 30.0f)
            db -= 12.0f * oct (30.0f, f);

        return db;
    }

    // media sobre 60 Hz - 12 kHz, para normalizar curvas antes de compararlas
    inline float meanInRange (const std::array<float, numLogBins>& v)
    {
        double sum = 0.0; int n = 0;
        for (int b = 0; b < numLogBins; ++b)
        {
            const float fc = logBinCentre (b);
            if (fc >= 60.0f && fc <= 12000.0f) { sum += v[(size_t) b]; ++n; }
        }
        return n > 0 ? (float) (sum / n) : 0.0f;
    }

    // ---------------------------------------------------------------- Filtros
    inline juce::dsp::IIR::Coefficients<float>::Ptr makeBandCoeffs (int band, float gainDb, double sr)
    {
        using C = juce::dsp::IIR::Coefficients<float>;
        const float gain = juce::Decibels::decibelsToGain (gainDb);
        const float f    = juce::jmin (bandFreqs[(size_t) band], (float) (sr * 0.45));

        if (band == 0)             return C::makeLowShelf  (sr, f, 0.7f, gain);
        if (band == numBands - 1)  return C::makeHighShelf (sr, f, 0.7f, gain);
        return C::makePeakFilter (sr, f, 1.0f, gain);
    }
}

//==============================================================================
class TerminatorQAudioProcessor : public juce::AudioProcessor
{
public:
    TerminatorQAudioProcessor();
    ~TerminatorQAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "TERMINATOR Q"; }
    bool acceptsMidi() const override  { return false; }
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

    //---------------------------------------------------------------- Análisis / matching
    void startAnalysis();
    void stopAnalysis();
    bool isAnalyzing() const        { return analyzing.load(); }
    bool hasAnalysis() const        { return analyzedFrames.load() >= 10; }

    // calcula las ganancias de las 10 bandas para acercar el audio analizado al target
    bool computeMatch (int presetIndex);
    void setBandGain (int band, float db);
    void resetAllBands();

    juce::AudioProcessorValueTreeState apvts;

    std::array<std::atomic<float>, TQ::numLogBins> spectrumDb;   // espectro en vivo (suavizado)
    std::array<std::atomic<float>, TQ::numLogBins> avgDb;        // media del análisis

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    void pushForAnalysis (const juce::AudioBuffer<float>& buffer);
    void runFFT();

    using Filter    = juce::dsp::IIR::Filter<float>;
    using Duplicator = juce::dsp::ProcessorDuplicator<Filter, juce::dsp::IIR::Coefficients<float>>;
    std::array<Duplicator, TQ::numBands> filters;
    std::array<float, TQ::numBands> lastGain {};

    std::atomic<float>* amountParam = nullptr;
    std::atomic<float>* outputParam = nullptr;
    std::atomic<float>* bypassParam = nullptr;
    std::array<std::atomic<float>*, TQ::numBands> gainParams {};

    // análisis
    juce::dsp::FFT fft { TQ::fftOrder };
    juce::dsp::WindowingFunction<float> window { (size_t) TQ::fftSize, juce::dsp::WindowingFunction<float>::hann };
    std::array<float, TQ::fftSize> ring {};
    std::array<float, 2 * TQ::fftSize> fftData {};
    int writePos = 0, hopCounter = 0;

    std::array<int, TQ::numLogBins>   binK1 {}, binK2 {};
    std::array<float, TQ::numLogBins> binPos {};

    std::array<double, TQ::numLogBins> powerSum {};
    int frameCount = 0;

    std::atomic<bool> analyzing { false };
    std::atomic<bool> resetAvg  { false };
    std::atomic<int>  analyzedFrames { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TerminatorQAudioProcessor)
};
