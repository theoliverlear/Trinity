#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <vector>

class TrinityAudioProcessor : public AudioProcessor
{
public:
    TrinityAudioProcessor();
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
    void releaseResources() override {}

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

    // Display range helper for the editor (upper frequency bound after guards)
    double getDisplayMaxHz() const noexcept { return displayMaxHz; }

    // Debug export: copy last few FFT tail bins (after processing) and current bands
    void copyDebugData (std::vector<float>& tailBins,
                        std::vector<float>& bands,
                        int& outHiGuard,
                        double& outSampleRate,
                        int& outFftSize,
                        double& outDisplayMaxHz) const;

    // Built-in test signal generator controls (Standalone convenience)
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

private:
    std::atomic<float> rmsLevel { 0.0f };

    std::atomic<float> totalLevel { 0.0f };
    std::atomic<float> lowLevel { 0.0f };
    std::atomic<float> midLevel { 0.0f };
    std::atomic<float> highLevel { 0.0f };

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
    void setFreqSmoothingEnabled (bool b) noexcept { freqSmoothEnabled.store(b); }
    void setBandSmoothingEnabled (bool b) noexcept { bandSmoothEnabled.store(b); }
    void setGuardPercent (float p) noexcept;
    void setTaperPercent (float p) noexcept { taperPercent.store (juce::jlimit (0.0f, 0.2f, p)); }
    void setSpecSmoothing (float s) noexcept { specSmoothing = juce::jlimit (0.0f, 1.0f, s); }

    // ===== Test signal generator (Standalone convenience) =====
    std::atomic<bool> testEnabled { false };
    std::atomic<int>  testType { (int) kOff };
    float testPhase { 0.0f };
    float sweepPhase { 0.0f };
    long long sweepSampleCount { 0 };
    long long sweepTotalSamples { 0 };
    juce::Random rng;
    // simple pink-ish noise one-pole filter state
    float pinkZ1 { 0.0f };
    float pinkCoeff { 0.02f }; // very light tilt, not critical (debug use)

    void generateTestSignal (juce::AudioBuffer<float>& buffer);

    // ===== Debug capture =====
    mutable SpinLock spectrumLock;  // protect spectrum access
    // Debug arrays
    std::vector<float> debugTailBinsPreSmooth;   // tail bins before freq smoothing/taper (after temporal smooth)
    std::vector<float> debugTailBinsPostSmooth;  // tail bins after freq smoothing (pre-taper)
    std::vector<float> debugTailBinsPostTaper;   // tail bins after taper
    std::vector<float> debugBandsPreBandSmooth;  // bands before band-domain smoothing

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
};

AudioProcessor* JUCE_CALLTYPE createPluginFilter();
