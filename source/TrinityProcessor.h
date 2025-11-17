#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <vector>

#include "models/SoloMode.h"

class TrinityAudioProcessor : public AudioProcessor
{
public:
    TrinityAudioProcessor();
    // Expose SoloMode as a nested type for call sites that expect
    // TrinityAudioProcessor::SoloMode::X while keeping the enum defined
    // in models/SoloMode.h as the single source of truth.
    using SoloMode = ::SoloMode;
    static void initDspProcessSpec(double sampleRate, int samplesPerBlock,
                                      dsp::ProcessSpec& spec);
    void initCrossoverFilters(dsp::ProcessSpec spec);
    ~TrinityAudioProcessor() override = default;

    const String getName() const override { return "Trinity"; }

    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }

    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const String getProgramName(int) override { return "Default"; }
    void changeProgramName(int, const String&) override {}

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(AudioBuffer<float>&, MidiBuffer&) override;
    void processBlock(AudioBuffer<double>&, MidiBuffer&) override;

    AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    void getStateInformation(MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    float getRMSLevel() const noexcept { return rmsLevel.load(); }

    float getTotalLevel() const noexcept { return totalLevel.load(); }
    float getLowLevel() const noexcept { return lowLevel.load(); }
    float getMidLevel() const noexcept { return midLevel.load(); }
    float getHighLevel() const noexcept { return highLevel.load(); }

    // Copy latest spectrum magnitudes [0..1] into dest (thread-safe).
    void copySpectrum (std::vector<float>& dest) const;
    void addBandFrequencyData(double bandStartHz, double bandEndHz, int bin0,
                              int bin1);

    void setSoloMode (SoloMode mode) noexcept { soloMode.store (mode); }
    SoloMode getSoloMode() const noexcept      { return soloMode.load(); }

    // Display range helper for the editor (upper frequency bound after guards)
    double getDisplayMaxHz() const noexcept { return displayMaxHz; }

    // Debug export: copy last few FFT tail bins (after processing) and current bands
    void copyDebugData(std::vector<float>& tailBins,
                        std::vector<float>& bands,
                        int& outHiGuard,
                        double& outSampleRate,
                        int& outFftSize,
                        double& outDisplayMaxHz) const;

    // Built-in test signal generator controls (Standalone convenience)
    // TODO: Move to its own file.
    enum TestSignalType
    {
        kOff = 0,
        kSine17k = 1,
        kSine19k = 2,
        kWhiteNoise = 3,
        kPinkNoise = 4,
        kLogSweep = 5
    };

    void setTestEnabled (bool enabled) noexcept { testEnabled.store (enabled); }
    void setTestType (int type) noexcept { testType.store (type); }

    // Enable/disable debug capture of tail bins and pre-smooth bands to avoid
    // allocations in the audio thread when not needed (prevents crackles).
    void setDebugCaptureEnabled (bool enabled) noexcept { debugCaptureEnabled.store (enabled); }

private:
    std::atomic<float> rmsLevel { 0.0f };

    std::atomic<float> totalLevel { 0.0f };
    std::atomic<float> lowLevel { 0.0f };
    std::atomic<float> midLevel { 0.0f };
    std::atomic<float> highLevel { 0.0f };

    std::atomic<SoloMode> soloMode { SoloMode::None };

    dsp::LinkwitzRileyFilter<float> lowMidCrossover[2];
    dsp::LinkwitzRileyFilter<float> midHighCrossover[2];

    dsp::ProcessSpec processSpec {};

    // ===== Realtime FFT for spectrum =====
    static constexpr int fftOrder = 11;              // 2^11 = 2048 for better low-end resolution
    static constexpr int fftSize  = 1 << fftOrder;   // 2048 samples

    std::vector<float> fifo;        // incoming mono samples
    int fifoIndex { 0 };
    std::vector<float> fftTime;     // windowed time-domain buffer (size fftSize)
    std::vector<float> fftData;     // interleaved real/imag for JUCE FFT (size 2*fftSize)
    std::vector<float> spectrum;    // magnitudes [0..1], log-averaged bands for UI
    std::vector<float> spectrumPowerSmoothed; // smoothed linear power per FFT bin (size fftSize/2)

    std::unique_ptr<dsp::FFT> fft;
    std::unique_ptr<dsp::WindowingFunction<float>> window;
    float specSmoothing { 0.2f };  // simple one-pole smoothing in processor

    // Amplitude calibration
    // Overall scale to convert raw FFT magnitudes to approximately input peak units.
    // oneSided(2/N) and Hann coherent gain compensation (~0.5) are folded here.
    float fftAmplitudeScale { 1.0f };

    // DC remover: leaky mean estimator (high-pass) applied to mono mix before FFT
    float dcMean { 0.0f };
    float dcAlpha { 0.0f }; // close to 1.0 -> very low cutoff (~5 Hz)

    // Log-frequency band aggregation for UI (improves perceived low-end accuracy)
    int numBands { 96 };
    double currentSampleRate { 44100.0 };
    std::vector<int> bandBinStart; // per band [inclusive] (legacy/diagnostic)
    std::vector<int> bandBinEnd;   // per band [inclusive] (legacy/diagnostic)
    // Fractional band edges in Hz for accurate aggregation (precomputed)
    std::vector<double> bandF0Hz;  // per band start frequency (Hz)
    std::vector<double> bandF1Hz;  // per band end frequency (Hz)

    void buildLogBands();

    // ===== Edge-handling / display helpers =====
    int hiGuardBins { 0 };                 // number of highest bins to ignore
    double displayMaxHz { 20000.0 };       // upper frequency actually displayed (post-guard)

    // Edge/smoothing controls (debug/Standalone only)
    std::atomic<bool> freqSmoothEnabled { true };   // enable frequency-domain smoothing across bins
    std::atomic<bool> bandSmoothEnabled { true };   // enable band-domain smoothing after aggregation
    std::atomic<float> guardPercent { 0.06f };      // fraction of half-spectrum to guard near Nyquist (0..0.2)
    std::atomic<float> taperPercent { 0.02f };      // fraction of half-spectrum for cosine taper before guard (0..0.1)

public:
    // Debug/Standalone tuning controls
    void setFreqSmoothingEnabled (bool newFreqSmoothEnabled) noexcept
    {
        this->freqSmoothEnabled.store(newFreqSmoothEnabled);
    }
    void setBandSmoothingEnabled(bool newBandSmoothingEnabled) noexcept
    {
        this->bandSmoothEnabled.store(newBandSmoothingEnabled);
    }
    void setGuardPercent (float newGuardPercent) noexcept;
    void setTaperPercent (float newTaperPercent) noexcept
    {
        this->taperPercent.store(jlimit(0.0f, 0.2f, newTaperPercent));
    }
    void setSpecSmoothing(float newSpecSmoothing) noexcept
    {
        this->specSmoothing = jlimit(0.0f, 1.0f, newSpecSmoothing);
    }

    // ===== Test signal generator (Standalone convenience) =====
    std::atomic<bool> testEnabled { false };
    std::atomic<int> testType { kOff };
    float testPhase { 0.0f };
    float sweepPhase { 0.0f };
    long long sweepSampleCount { 0 };
    long long sweepTotalSamples { 0 };
    Random rng;
    // simple pink-ish noise one-pole filter state
    float pinkZ1 { 0.0f };
    float pinkCoeff { 0.02f }; // very light tilt, not critical (debug use)

    void generateTestSignal (AudioBuffer<float>& buffer);

    // ===== Debug capture =====
    mutable SpinLock spectrumLock;  // protect spectrum access
    std::atomic<bool> debugCaptureEnabled { false }; // gate debug vectors
    // Debug arrays
    std::vector<float> debugTailBinsPreSmooth;   // tail bins before freq smoothing/taper (after temporal smooth)
    std::vector<float> debugTailBinsPostSmooth;  // tail bins after freq smoothing (pre-taper)
    std::vector<float> debugTailBinsPostTaper;   // tail bins after taper
    std::vector<float> debugBandsPreBandSmooth;  // bands before band-domain smoothing

    // ===== Temporary buffers to avoid allocations in the audio thread =====
    std::vector<float> tempPowerForAggregation;   // size numBins
    std::vector<float> tempBands;                 // size numBands
    std::vector<float> tempBandsPreSmooth;        // size numBands
    std::vector<float> tempBandsSmooth;           // size numBands (for smoothing output)
    juce::AudioBuffer<float> tempDoubleBuffer;    // reused in processBlock(double)

public:
    // Extended debug export
    void copyDebugData (std::vector<float>& tailPreSmooth,
                        std::vector<float>& tailPostSmooth,
                        std::vector<float>& tailPostTaper,
                        std::vector<float>& bandsPreBandSmooth,
                        std::vector<float>& bandsFinal,
                        int& outHiGuard,
                        int& outAllowedEndBin,
                        double& outAllowedEndHz,
                        double& outSampleRate,
                        int& outFftSize,
                        double& outDisplayMaxHz) const;

private:
    // Small helpers to reduce repetition and improve clarity
    static float mixDownToMonoSample(const AudioBuffer<float>& buffer, int sampleIndex) noexcept
    {
        const int channels = buffer.getNumChannels();
        if (channels <= 0) { return 0.0f; }
        float sum = 0.0f;
        for (int channel = 0; channel < channels; ++channel)
        {
            sum += buffer.getReadPointer(channel)[sampleIndex];
        }
        return sum / static_cast<float>(jmax(1, channels));
    }

    static void writeSampleToAllChannels(AudioBuffer<float>& buffer, int sampleIndex, float value) noexcept
    {
        const int channels = buffer.getNumChannels();
        for (int channel = 0; channel < channels; ++channel)
        {
            buffer.getWritePointer(channel)[sampleIndex] = value;
        }
    }
};

AudioProcessor* JUCE_CALLTYPE createPluginFilter();
