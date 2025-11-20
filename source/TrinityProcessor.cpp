#include "TrinityProcessor.h"
#include "TrinityEditor.h"
#include "models/BandFrequencies.h"
#include "services/SpectrumProcessing.h"
#include <cmath>
#include <spdlog/sinks/stdout_color_sinks-inl.h>



TrinityAudioProcessor::TrinityAudioProcessor()
    : AudioProcessor(
        BusesProperties()
            .withInput("Input",  AudioChannelSet::stereo(), true)
            .withOutput("Output", AudioChannelSet::stereo(), true))
{
    this->console->set_level(spdlog::level::debug);
    this->console->info("Trinity Audio Processor started");
}

void TrinityAudioProcessor::initDspProcessSpec(double sampleRate, int samplesPerBlock, dsp::ProcessSpec& spec)
{
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<uint32> (samplesPerBlock);
    spec.numChannels = 1;
}

void TrinityAudioProcessor::initCrossoverFilters(dsp::ProcessSpec spec)
{
    initCrossoverFilter(spec, this->lowMidCrossover, BandFrequencies::LowBandEndHz);
    initCrossoverFilter(spec, this->midHighCrossover, BandFrequencies::MidBandEndHz);
}

void TrinityAudioProcessor::initCrossoverFilter(dsp::ProcessSpec spec, auto& crossover, BandFrequencies bandCutoff)
{

    for (dsp::LinkwitzRileyFilter<float>& filter : crossover)
    {
        filter.reset();
        filter.setType(dsp::LinkwitzRileyFilterType::lowpass);
        float bandCutoffHz = static_cast<float>(bandCutoff);
        filter.setCutoffFrequency(bandCutoffHz);
        filter.prepare(spec);
    }
}

void TrinityAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    ignoreUnused(samplesPerBlock);
    dsp::ProcessSpec spec;
    initDspProcessSpec(sampleRate, samplesPerBlock, spec);
    this->initCrossoverFilters(spec);

    // Initialise FFT resources
    this->fft = std::make_unique<dsp::FFT> (fftOrder);
    // Use non-normalised Hann and handle coherent gain explicitly in our scaling (4/N one-sided)
    this->window = std::make_unique<dsp::WindowingFunction<float>> (fftSize, dsp::WindowingFunction<float>::hann, false);

    this->fifo.assign(fftSize, 0.0f);
    this->fftTime.assign(fftSize, 0.0f);
    this->fftData.assign(2 * fftSize, 0.0f);
    // spectrum holds log-averaged bands for UI; will be sized in buildLogBands
    this->spectrum.clear();
    this->spectrumPowerSmoothed.assign(fftSize / 2, 0.0f);
    this->fifoIndex = 0;

    // Set current SR and (re)build band mapping for log display
    this->currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    // Compute guarded bins near Nyquist based on guardPercent (proportional with minimum)
    {
        const int numBins = fftSize / 2;
        const float guardPercentClamped = jlimit(0.0f, 0.2f, this->guardPercent.load());
        this->hiGuardBins = jmax(8, static_cast<int>(std::floor(static_cast<double>(numBins) * static_cast<double>(guardPercentClamped))));
    }
    this->buildLogBands();

    this->testSignalGenerator.prepare(this->currentSampleRate, this->displayMaxHz);

    // Pre-size reusable temporary buffers to avoid allocations in audio thread
    {
        const int numBins = fftSize / 2;
        this->tempPowerForAggregation.assign (numBins, 0.0f);
        this->tempBands.assign (static_cast<size_t>(this->numBands), 0.0f);
        this->tempBandsPreSmooth.assign (static_cast<size_t>(this->numBands), 0.0f);
        this->tempBandsSmooth.assign (static_cast<size_t>(this->numBands), 0.0f);
        this->tempDoubleBuffer.setSize (0, 0); // ensure no lingering allocation
    }

    // Reset DC remover (leaky mean)
    this->dcMean = 0.0f;
    // Choose ~5 Hz DC removal cutoff
    {
        const double cutoffHz = 5.0;
        const double sampleRateHz = this->currentSampleRate;
        // One-pole smoothing factor for mean estimator
        this->dcAlpha = static_cast<float>(1.0 - std::exp(-2.0 * double_Pi * cutoffHz / sampleRateHz));
        this->dcAlpha = jlimit(0.00001f, 1.0f, this->dcAlpha);
    }

    // Amplitude calibration: one-sided FFT scaling and Hann coherent gain compensation
    // For Hann, coherent gain ~= 0.5. Apply 2/N for one-sided spectrum and divide by 0.5 => 4/N.
    this->fftAmplitudeScale = 4.0f / static_cast<float>(fftSize);

}

bool TrinityAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    auto mainOut = layouts.getMainOutputChannelSet();
    const bool isInvalidChannelSet = mainOut != AudioChannelSet::mono()
                                  && mainOut != AudioChannelSet::stereo();

    if (isInvalidChannelSet || layouts.getMainInputChannelSet() != mainOut)
    {
        return false;
    }
    return true;
}

void TrinityAudioProcessor::processBlock(AudioBuffer<float>& buffer,
                                         MidiBuffer& midi)
{
    ScopedNoDenormals noDenormals;
    ignoreUnused(midi);

    for (int channel = getTotalNumInputChannels(); channel < getTotalNumOutputChannels(); ++channel)
    {
        buffer.clear(channel, 0, buffer.getNumSamples());
    }

    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    // Optional: generate built-in test signal (Standalone convenience)
    if (this->testSignalGenerator.isEnabled())
    {
        this->generateTestSignal (buffer);
    }

    float totalPeak = 0.0f;

    for (int channel = 0; channel < numChannels; ++channel)
    {
        const float* readPtr = buffer.getReadPointer(channel);
        for (int sampleIndex = 0; sampleIndex < numSamples; ++sampleIndex)
        {
            const float absValue = std::abs(readPtr[sampleIndex]);
            if (absValue > totalPeak)
            {
                totalPeak = absValue;
            }
        }
    }

    float lowPeak = 0.0f;
    float midPeak = 0.0f;
    float highPeak = 0.0f;

    const auto currentSolo = this->soloMode.load();

    for (int channel = 0; channel < numChannels; ++channel)
    {
        float* writePtr = buffer.getWritePointer(channel);

        for (int sampleIndex = 0; sampleIndex < numSamples; ++sampleIndex)
        {
            const float inSample = writePtr[sampleIndex];

            float lowSample = 0.0f;
            float residual = 0.0f;
            float midSample = 0.0f;
            float highSample = 0.0f;

            this->lowMidCrossover[channel].processSample(0, inSample, lowSample, residual);
            this->midHighCrossover[channel].processSample(0, residual, midSample, highSample);

            const float lowSamplePeak = std::abs(lowSample);
            const float midSamplePeak = std::abs(midSample);
            const float highSamplePeak = std::abs(highSample);

            if (lowSamplePeak > lowPeak) lowPeak = lowSamplePeak;
            if (midSamplePeak > midPeak) midPeak = midSamplePeak;
            if (highSamplePeak > highPeak) highPeak = highSamplePeak;

            float outSample = 0.0f;
            switch (currentSolo)
            {
                case SoloMode::Low:
                    outSample = lowSample;
                    break;
                case SoloMode::Mid:
                    outSample = midSample;
                    break;
                case SoloMode::High:
                    outSample = highSample;
                    break;
                case SoloMode::None:
                default:
                    outSample = lowSample + midSample + highSample;
                    break;
            }

            writePtr[sampleIndex] = outSample;
        }
    }

    this->totalLevel.store(jlimit(0.0f, 1.0f, totalPeak));
    this->lowLevel.store(jlimit(0.0f, 1.0f, lowPeak));
    this->midLevel.store(jlimit(0.0f, 1.0f, midPeak));
    this->highLevel.store(jlimit(0.0f, 1.0f, highPeak));

    // ===== Accumulate mono samples for FFT =====
    if (this->fft && this->window)
    {
        for (int sampleIndex = 0; sampleIndex < numSamples; ++sampleIndex)
        {
            // simple mono mixdown: average of channels
            float mixedSample = this->mixDownToMonoSample(buffer, sampleIndex);

            // DC removal via leaky mean estimator (very low cutoff)
            this->dcMean += this->dcAlpha * (mixedSample - this->dcMean);
            mixedSample = mixedSample - this->dcMean;

            this->fifo[(size_t) this->fifoIndex++] = mixedSample;

            if (this->fifoIndex >= fftSize)
            {
                // Prepare time-domain buffer
                FloatVectorOperations::copy(this->fftTime.data(), this->fifo.data(), fftSize);
                this->window->multiplyWithWindowingTable(this->fftTime.data(), fftSize);

                // Copy to complex buffer (real, imag)
                FloatVectorOperations::clear(this->fftData.data(), (int) this->fftData.size());
                for (int timeSampleIndex = 0; timeSampleIndex < fftSize; ++timeSampleIndex)
                    this->fftData[(size_t) (2 * timeSampleIndex)] = this->fftTime[(size_t) timeSampleIndex];

                // Perform forward FFT in-place; ignore negative frequencies to avoid mirror artefacts
                this->fft->performRealOnlyForwardTransform(this->fftData.data(), true);

                // Compute magnitudes for first half (bins 0..N/2-1)
                const int numBins = fftSize / 2;
                // Provide extra dynamic range so reference comparisons align better
                constexpr float minDb = -120.0f;
                constexpr float maxDb = 0.0f;

                // Per-bin linear power smoothing (reduces bias and HF jitter)
                for (int bin = 0; bin < numBins; ++bin)
                {
                    const float real = this->fftData[static_cast<size_t>(2 * bin)];
                    const float imag = this->fftData[static_cast<size_t>(2 * bin + 1)];
                    // One-sided scaling with Hann coherent gain compensation:
                    // bins 1..N/2-1 use 4/N; DC and Nyquist would use 2/N, but we skip them.
                    const float perBinScale = 4.0f / static_cast<float>(fftSize);
                    const float mag = std::sqrt(real * real + imag * imag) * perBinScale;
                    const float power = mag * mag; // linear power

                    const float prev = this->spectrumPowerSmoothed[(size_t) bin];
                    const float smoothingCoeff = this->specSmoothing;
                    this->spectrumPowerSmoothed[(size_t) bin] = prev * (1.0f - smoothingCoeff) + power * smoothingCoeff;
                }

                // Capture tail before any freq smoothing/taper for CSV (optional)
                const int captureCount = 64;
                const int allowedEndRaw = numBins - 1; // before guard/taper
                const int startRaw = jlimit (0, allowedEndRaw, allowedEndRaw - (captureCount - 1));
                if (this->debugBin.debugCaptureEnabled.load())
                {
                    const SpinLock::ScopedLockType sl (this->spectrumLock);
                    this->debugBin.debugTailBinsPreSmooth.resize ((size_t) (allowedEndRaw - startRaw + 1));
                    int destIndex = 0;
                    for (int bin = startRaw; bin <= allowedEndRaw; ++bin)
                    {
                        this->debugBin.debugTailBinsPreSmooth[static_cast<size_t>(destIndex++)] = this->spectrumPowerSmoothed[static_cast<size_t>(bin)];
                    }
                }

                // Determine the last usable bin index based on BOTH Nyquist guard and 20 kHz cap
                const int allowedEnd = SpectrumProcessing::computeAllowedEndBin(this->currentSampleRate, fftSize, this->hiGuardBins);

                // Zero out any bins strictly above the allowed end (covers both guarded and >20 kHz regions)
                SpectrumProcessing::zeroStrictlyAbove(this->spectrumPowerSmoothed, allowedEnd);

                // Frequency-domain smoothing to reduce isolated spikes (esp. near HF)
                auto& powerForAggregation = this->tempPowerForAggregation;
                if (static_cast<int>(powerForAggregation.size()) != numBins)
                {
                    powerForAggregation.assign(numBins, 0.0f);
                }
                SpectrumProcessing::frequencySmoothTriangularIfEnabled(this->spectrumPowerSmoothed,
                                                                       powerForAggregation,
                                                                       allowedEnd,
                                                                       this->freqSmoothEnabled.load());

                // Capture tail after freq smoothing (pre-taper)
                if (this->debugBin.debugCaptureEnabled.load())
                {
                    const SpinLock::ScopedLockType sl (this->spectrumLock);
                    const int endBin = jlimit(0, numBins - 1, allowedEnd);
                    const int startBin = jlimit(0, endBin, endBin - (captureCount - 1));
                    this->debugBin.debugTailBinsPostSmooth.resize(static_cast<size_t>(endBin - startBin + 1));
                    int destIndex = 0;
                    for (int bin = startBin; bin <= endBin; ++bin)
                    {
                        this->debugBin.debugTailBinsPostSmooth[static_cast<size_t>(destIndex++)] = powerForAggregation[static_cast<size_t>(bin)];
                    }
                }

                // Apply gentle cosine taper and zero above allowed end in aggregation buffer
                SpectrumProcessing::applyCosineTaper(powerForAggregation, allowedEnd, this->taperPercent.load());
                SpectrumProcessing::zeroStrictlyAbove(powerForAggregation, allowedEnd);

                // Capture tail after taper
                int allowedEndBin = jlimit(0, numBins - 1, allowedEnd);
                if (this->debugBin.debugCaptureEnabled.load())
                {
                    const SpinLock::ScopedLockType spinLock (this->spectrumLock);
                    const int endBin = allowedEndBin;
                    const int startBin = jlimit(0, endBin, endBin - (captureCount - 1));
                    this->debugBin.debugTailBinsPostTaper.resize (static_cast<size_t>(endBin - startBin + 1));
                    int destIndex = 0;
                    for (int bin = startBin; bin <= endBin; ++bin)
                    {
                        this->debugBin.debugTailBinsPostTaper[static_cast<size_t>(destIndex++)] = powerForAggregation[static_cast<size_t>(bin)];
                    }
                }

                // Aggregate linear bins into perceptual log-spaced bands for UI accuracy, esp. low-end
                if (static_cast<int>(this->bandBinStart.size()) != this->numBands || static_cast<int>(this->bandBinEnd.size()) != this->numBands)
                {
                    this->buildLogBands();
                }

                auto& bands = this->tempBands;
                auto& bandsPreSmooth = this->tempBandsPreSmooth;
                // Hz-per-bin for current FFT configuration
                const double binHz = this->currentSampleRate / static_cast<double>(fftSize);
                SpectrumProcessing::aggregateBandsFractional(powerForAggregation,
                                                             allowedEnd,
                                                             binHz,
                                                             this->bandF0Hz,
                                                             this->bandF1Hz,
                                                             minDb,
                                                             maxDb,
                                                             bands,
                                                             bandsPreSmooth);

                // Light band-domain smoothing to discourage isolated spikes at the top end
                // Light band-domain smoothing to discourage isolated spikes at the top end
                if (this->bandSmoothEnabled.load())
                {
                    SpectrumProcessing::smoothBandsInPlace(bands, true);
                }

                {
                    const SpinLock::ScopedLockType sl (this->spectrumLock);
                    this->spectrum = bands;                 // copy from reusable buffer
                    if (this->debugBin.debugCaptureEnabled.load())
                    {
                        this->debugBin.debugTailBinsPreSmooth.shrink_to_fit(); // no-op safety; keep struct usage consistent
                        this->debugBin.debugBandsPreBandSmooth = bandsPreSmooth;
                    }
                }

                this->fifoIndex = 0; // reset FIFO to start gathering next block
            }
        }
    }
}

void TrinityAudioProcessor::processBlock(AudioBuffer<double>& buffer,
                                         MidiBuffer& midi)
{
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();
    if (this->tempDoubleBuffer.getNumChannels() != numChannels
        || this->tempDoubleBuffer.getNumSamples() != numSamples)
    {
        this->tempDoubleBuffer.setSize(jmax(1, numChannels), jmax(1, numSamples), false, false, true);
    }

    for (int channelIndex = 0; channelIndex < numChannels; ++channelIndex)
    {
        const double* src = buffer.getReadPointer(channelIndex);
        float* dest = this->tempDoubleBuffer.getWritePointer(channelIndex);
        for (int sampleIndex = 0; sampleIndex < numSamples; ++sampleIndex)
        {
            dest[sampleIndex] = static_cast<float>(src[sampleIndex]);
        }
    }

    this->processBlock(this->tempDoubleBuffer, midi);

    for (int channelIndex = 0; channelIndex < numChannels; ++channelIndex)
    {
        const float* src = this->tempDoubleBuffer.getReadPointer(channelIndex);
        double* dest = buffer.getWritePointer(channelIndex);
        for (int sampleIndex = 0; sampleIndex < numSamples; ++sampleIndex)
        {
            dest[sampleIndex] = static_cast<double>(src[sampleIndex]);
        }
    }
}

AudioProcessorEditor* TrinityAudioProcessor::createEditor()
{
    return new TrinityAudioProcessorEditor(*this);
}

void TrinityAudioProcessor::getStateInformation(MemoryBlock& destData)
{
    ignoreUnused(destData);
}

void TrinityAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    ignoreUnused(data, sizeInBytes);
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TrinityAudioProcessor();
}

void TrinityAudioProcessor::releaseResources()
{
    // Release heavy DSP resources
    this->console->info("Releasing resources...");
    this->fft.reset();
    this->window.reset();

    // Clear and shrink major vectors
    this->fifo.clear(); this->fifo.shrink_to_fit();
    this->fftTime.clear(); this->fftTime.shrink_to_fit();
    this->fftData.clear(); this->fftData.shrink_to_fit();
    this->spectrum.clear(); this->spectrum.shrink_to_fit();
    this->spectrumPowerSmoothed.clear(); this->spectrumPowerSmoothed.shrink_to_fit();
    this->bandBinStart.clear(); this->bandBinStart.shrink_to_fit();
    this->bandBinEnd.clear(); this->bandBinEnd.shrink_to_fit();
    this->bandF0Hz.clear(); this->bandF0Hz.shrink_to_fit();
    this->bandF1Hz.clear(); this->bandF1Hz.shrink_to_fit();

    this->tempPowerForAggregation.clear(); this->tempPowerForAggregation.shrink_to_fit();
    this->tempBands.clear(); this->tempBands.shrink_to_fit();
    this->tempBandsPreSmooth.clear(); this->tempBandsPreSmooth.shrink_to_fit();

    this->debugBin.debugTailBinsPreSmooth.clear(); this->debugBin.debugTailBinsPreSmooth.shrink_to_fit();
    this->debugBin.debugTailBinsPostSmooth.clear(); this->debugBin.debugTailBinsPostSmooth.shrink_to_fit();
    this->debugBin.debugTailBinsPostTaper.clear(); this->debugBin.debugTailBinsPostTaper.shrink_to_fit();
    this->debugBin.debugBandsPreBandSmooth.clear(); this->debugBin.debugBandsPreBandSmooth.shrink_to_fit();

    this->tempDoubleBuffer.setSize(0, 0);
    this->fifoIndex = 0;
}

void TrinityAudioProcessor::copySpectrum(std::vector<float>& dest) const
{
    const SpinLock::ScopedLockType spinLock (this->spectrumLock);
    dest = this->spectrum;
}

void TrinityAudioProcessor::addBandFrequencyData(double bandStartHz, double bandEndHz, int bin0, int bin1)
{
    this->bandBinStart.push_back(bin0);
    this->bandBinEnd.push_back(bin1);
    this->bandF0Hz.push_back(bandStartHz);
    this->bandF1Hz.push_back(bandEndHz);
}

void TrinityAudioProcessor::buildLogBands()
{
    const int numBins = fftSize / 2;
    this->bandBinStart.clear();
    this->bandBinEnd.clear();
    this->bandF0Hz.clear();
    this->bandF1Hz.clear();
    this->bandBinStart.reserve(static_cast<size_t>(this->numBands));
    this->bandBinEnd.reserve(static_cast<size_t>(this->numBands));
    this->bandF0Hz.reserve(static_cast<size_t>(this->numBands));
    this->bandF1Hz.reserve(static_cast<size_t>(this->numBands));

    const double binHz = this->currentSampleRate / static_cast<double>(fftSize);
    const double fMin = 20.0; // start around 20 Hz
    // Map UI bands only up to the last allowed bin considering both guard and a hard 20 kHz cap
    const int allowedEndByGuard = jlimit(0, numBins - 1, numBins - this->hiGuardBins - 1);
    const int capBy20k = jlimit(0, numBins - 1, static_cast<int>(std::floor(20000.0 / binHz)) - 1);
    const int allowedEndBin = jlimit(0, numBins - 1, jmin(allowedEndByGuard, capBy20k));
    // Use the end of the last allowed bin as the display cap but not above 20 kHz
    const double allowedEndHz   = jmin(20000.0, static_cast<double>(allowedEndBin + 1) * binHz);
    this->displayMaxHz = jmax(fMin * 2.0, allowedEndHz);
    const double fMax = this->displayMaxHz;
    const double logMin = std::log(fMin);
    const double logMax = std::log(fMax);

    for (int bandIndex = 0; bandIndex < this->numBands; ++bandIndex)
    {
        // t spans [0,1]
        const double fractionStart = static_cast<double>(bandIndex) / static_cast<double>(this->numBands);
        const double fractionEnd = static_cast<double>(bandIndex + 1) / static_cast<double>(this->numBands);
        double bandStartHz = std::exp(logMin + (logMax - logMin) * fractionStart);
        double bandEndHz = std::exp(logMin + (logMax - logMin) * fractionEnd);
        // Clamp to [fMin .. allowedEndHz]
        bandStartHz = jlimit(fMin, allowedEndHz, bandStartHz);
        bandEndHz   = jlimit(fMin, allowedEndHz, bandEndHz);
        if (bandEndHz <= bandStartHz) bandEndHz = jmin(allowedEndHz, bandStartHz + binHz);

        int bin0 = static_cast<int>(std::floor(bandStartHz / binHz + 0.5)); // round to nearest bin
        int bin1 = static_cast<int>(std::floor(bandEndHz / binHz + 0.5));

        bin0 = jlimit(0, numBins - 1, bin0);
        bin1 = jlimit(0, numBins - 1, bin1);
        if (bin1 < bin0)
        {
            bin1 = bin0;
        }

        addBandFrequencyData(bandStartHz, bandEndHz, bin0, bin1);
    }

    // Resize UI spectrum buffer to match number of bands
    const SpinLock::ScopedLockType spinLock (this->spectrumLock);
    this->spectrum.assign(static_cast<size_t>(this->numBands), 0.0f);
}

void TrinityAudioProcessor::copyDebugData(std::vector<float>& tailPreSmooth,
                                           std::vector<float>& tailPostSmooth,
                                           std::vector<float>& tailPostTaper,
                                           std::vector<float>& bandsPreBandSmooth,
                                           std::vector<float>& bandsFinal,
                                           int& outHiGuard,
                                           int& outAllowedEndBin,
                                           double& outAllowedEndHz,
                                           double& outSampleRate,
                                           int& outFftSize,
                                           double& outDisplayMaxHz) const
{
    const SpinLock::ScopedLockType scopedLock (this->spectrumLock);
    tailPreSmooth = this->debugBin.debugTailBinsPreSmooth;
    tailPostSmooth = this->debugBin.debugTailBinsPostSmooth;
    tailPostTaper = this->debugBin.debugTailBinsPostTaper;
    bandsPreBandSmooth = this->debugBin.debugBandsPreBandSmooth;
    bandsFinal = this->spectrum;
    outHiGuard = this->hiGuardBins;
    const int numBins = fftSize / 2;
    const double binHz = this->currentSampleRate / static_cast<double>(fftSize);
    const int allowedEndByGuard = jlimit(0, numBins - 1, numBins - this->hiGuardBins - 1);
    const int capBy20k = jlimit(0, numBins - 1, (int) std::floor(20000.0 / binHz) - 1);
    const int allowedEndBin = jlimit(0, numBins - 1, jmin (allowedEndByGuard, capBy20k));
    outAllowedEndBin = allowedEndBin;
    outAllowedEndHz = std::min(20000.0, static_cast<double>(allowedEndBin + 1) * binHz);
    outSampleRate = this->currentSampleRate;
    outFftSize = fftSize;
    outDisplayMaxHz = this->displayMaxHz;
}

void TrinityAudioProcessor::setGuardPercent(float newGuardPercent) noexcept
{
    const float clamped = jlimit(0.0f, 0.2f, newGuardPercent);
    this->guardPercent.store(clamped);
    const int numBins = fftSize / 2;
    this->hiGuardBins = jmax(8, static_cast<int>(std::floor(static_cast<double>(numBins) * static_cast<double>(clamped))));
    this->buildLogBands();
}

void TrinityAudioProcessor::generateTestSignal(AudioBuffer<float>& buffer)
{
    // Delegate to the outsourced generator
    this->testSignalGenerator.generate(buffer);
}
