#include "TrinityProcessor.h"
#include "TrinityEditor.h"
#include <cmath>

TrinityAudioProcessor::TrinityAudioProcessor()
    : AudioProcessor(
        BusesProperties()
            .withInput("Input",  AudioChannelSet::stereo(), true)
            .withOutput("Output", AudioChannelSet::stereo(), true))
{
}

void TrinityAudioProcessor::initDspProcessSpec(double sampleRate, int samplesPerBlock, dsp::ProcessSpec& spec)
{
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<uint32> (samplesPerBlock);
    spec.numChannels = 1;
}

void TrinityAudioProcessor::initCrossoverFilters(dsp::ProcessSpec spec)
{
    for (auto& filter : this->lowMidCrossover)
    {
        filter.reset();
        filter.setType (dsp::LinkwitzRileyFilterType::lowpass);
        filter.setCutoffFrequency (250.0f);
        filter.prepare(spec);
    }

    for (auto& filter : this->midHighCrossover)
    {
        filter.reset();
        filter.setType (dsp::LinkwitzRileyFilterType::lowpass);
        filter.setCutoffFrequency(2000.0f);
        filter.prepare(spec);
    }
}

void TrinityAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    ignoreUnused (samplesPerBlock);
    dsp::ProcessSpec spec;
    initDspProcessSpec(sampleRate, samplesPerBlock, spec);
    this->initCrossoverFilters(spec);

    // Initialise FFT resources
    this->fft = std::make_unique<dsp::FFT> (fftOrder);
    // Use non-normalised Hann and handle coherent gain explicitly in our scaling (4/N one-sided)
    this->window = std::make_unique<dsp::WindowingFunction<float>> (fftSize, dsp::WindowingFunction<float>::hann, false);

    this->fifo.assign (fftSize, 0.0f);
    this->fftTime.assign (fftSize, 0.0f);
    this->fftData.assign ((size_t) (2 * fftSize), 0.0f);
    // spectrum holds log-averaged bands for UI; will be sized in buildLogBands
    this->spectrum.clear();
    this->spectrumPowerSmoothed.assign ((size_t) (fftSize / 2), 0.0f);
    this->fifoIndex = 0;

    // Set current SR and (re)build band mapping for log display
    this->currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    // Compute guarded bins near Nyquist based on guardPercent (proportional with minimum)
    {
        const int numBins = fftSize / 2;
        const float guardPercentClamped = juce::jlimit (0.0f, 0.2f, guardPercent.load());
        this->hiGuardBins = juce::jmax (8, (int) std::floor ((double) numBins * (double) guardPercentClamped));
    }
    this->buildLogBands();

    // Reset DC remover (leaky mean)
    this->dcMean = 0.0f;
    // Choose ~5 Hz DC removal cutoff
    {
        const double cutoffHz = 5.0;
        const double sampleRateHz = this->currentSampleRate;
        // One-pole smoothing factor for mean estimator
        this->dcAlpha = (float) (1.0 - std::exp(-2.0 * juce::double_Pi * cutoffHz / sampleRateHz));
        this->dcAlpha = juce::jlimit(0.00001f, 1.0f, this->dcAlpha);
    }

    // Amplitude calibration: one-sided FFT scaling and Hann coherent gain compensation
    // For Hann, coherent gain ~= 0.5. Apply 2/N for one-sided spectrum and divide by 0.5 => 4/N.
    this->fftAmplitudeScale = 4.0f / static_cast<float>(fftSize);

    // Test generator reset
    this->testPhase = 0.0f;
    this->sweepPhase = 0.0f;
    this->sweepSampleCount = 0;
    this->sweepTotalSamples = (long long) (this->currentSampleRate * 10.0); // 10-second sweep
    this->pinkZ1 = 0.0f;
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
    ignoreUnused (midi);

    for (int channel = getTotalNumInputChannels(); channel < getTotalNumOutputChannels(); ++channel)
    {
        buffer.clear(channel, 0, buffer.getNumSamples());
    }

    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    // Optional: generate built-in test signal (Standalone convenience)
    if (this->testEnabled.load())
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

    float lowPeak  = 0.0f;
    float midPeak  = 0.0f;
    float highPeak = 0.0f;

    for (int channel = 0; channel < numChannels; ++channel)
    {
        const float* readPtr = buffer.getReadPointer(channel);

        for (int sampleIndex = 0; sampleIndex < numSamples; ++sampleIndex)
        {
            float lowSample  = 0.0f;
            float residual = 0.0f;
            float midSample  = 0.0f;
            float highSample = 0.0f;

            this->lowMidCrossover[channel].processSample (0, readPtr[sampleIndex], lowSample, residual);

            this->midHighCrossover[channel].processSample (0, residual, midSample, highSample);

            const float lowSamplePeak = std::abs(lowSample);
            const float midSamplePeak = std::abs(midSample);
            const float highSamplePeak = std::abs(highSample);

            if (lowSamplePeak > lowPeak) lowPeak  = lowSamplePeak;
            if (midSamplePeak > midPeak) midPeak  = midSamplePeak;
            if (highSamplePeak > highPeak) highPeak = highSamplePeak;
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
            float mixedSample = 0.0f;
            for (int channelIndex = 0; channelIndex < numChannels; ++channelIndex)
                mixedSample += buffer.getReadPointer(channelIndex)[sampleIndex];
            mixedSample /= std::max(1, numChannels);

            // DC removal via leaky mean estimator (very low cutoff)
            this->dcMean += this->dcAlpha * (mixedSample - this->dcMean);
            mixedSample = mixedSample - this->dcMean;

            this->fifo[(size_t) this->fifoIndex++] = mixedSample;

            if (this->fifoIndex >= fftSize)
            {
                // Prepare time-domain buffer
                juce::FloatVectorOperations::copy(this->fftTime.data(), this->fifo.data(), fftSize);
                this->window->multiplyWithWindowingTable(this->fftTime.data(), fftSize);

                // Copy to complex buffer (real, imag)
                juce::FloatVectorOperations::clear(this->fftData.data(), (int) this->fftData.size());
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
                    const float real = this->fftData[(size_t) (2 * bin)];
                    const float imag = this->fftData[(size_t) (2 * bin + 1)];
                    // One-sided scaling with Hann coherent gain compensation:
                    // bins 1..N/2-1 use 4/N; DC and Nyquist would use 2/N, but we skip them.
                    const float perBinScale = 4.0f / static_cast<float>(fftSize);
                    const float mag = std::sqrt(real * real + imag * imag) * perBinScale;
                    const float power = mag * mag; // linear power

                    const float prev = this->spectrumPowerSmoothed[(size_t) bin];
                    const float smoothingCoeff = this->specSmoothing;
                    this->spectrumPowerSmoothed[(size_t) bin] = prev * (1.0f - smoothingCoeff) + power * smoothingCoeff;
                }

                // Capture tail before any freq smoothing/taper for CSV
                const int captureCount = 64;
                const int allowedEndRaw = numBins - 1; // before guard/taper
                const int startRaw = juce::jlimit (0, allowedEndRaw, allowedEndRaw - (captureCount - 1));
                {
                    const juce::SpinLock::ScopedLockType sl (this->spectrumLock);
                    this->debugTailBinsPreSmooth.resize ((size_t) (allowedEndRaw - startRaw + 1));
                    int destIndex = 0;
                    for (int bin = startRaw; bin <= allowedEndRaw; ++bin)
                        this->debugTailBinsPreSmooth[(size_t) destIndex++] = this->spectrumPowerSmoothed[(size_t) bin];
                }

                // Zero out guarded tail bins to avoid any leakage from the Nyquist edge
                const int firstGuarded = juce::jlimit (0, numBins, numBins - this->hiGuardBins);
                for (int bin = firstGuarded; bin < numBins; ++bin)
                    this->spectrumPowerSmoothed[(size_t) bin] = 0.0f;

                // Frequency-domain smoothing to reduce isolated spikes (esp. near HF)
                std::vector<float> powerForAggregation;
                powerForAggregation.resize((size_t) numBins);
                if (this->freqSmoothEnabled.load() && numBins >= 5)
                {
                    // Adaptive triangular kernel [1,2,3,2,1] normalised; shrink near edge, stop before guard
                    const int lastValid = juce::jmax (2, numBins - this->hiGuardBins - 1);
                    for (int bin = 0; bin < numBins; ++bin)
                    {
                        if (bin < 2 || bin >= lastValid - 2)
                        {
                            powerForAggregation[(size_t) bin] = this->spectrumPowerSmoothed[(size_t) bin];
                            continue;
                        }

                        int radius = 2;
                        if (bin + radius >= lastValid) radius = juce::jmax (0, lastValid - 1 - bin);
                        if (radius < 2)
                        {
                            // Fallback to smaller kernel
                            const float acc = this->spectrumPowerSmoothed[(size_t) (bin - 1)] * 1.0f
                                            + this->spectrumPowerSmoothed[(size_t) bin] * 2.0f
                                            + this->spectrumPowerSmoothed[(size_t) (bin + 1)] * 1.0f;
                            const float norm = 1.0f + 2.0f + 1.0f;
                            powerForAggregation[(size_t) bin] = acc / norm;
                        }
                        else
                        {
                            const float w2 = 1.0f, w1 = 2.0f, w0 = 3.0f;
                            const float acc = w2 * this->spectrumPowerSmoothed[(size_t) (bin - 2)]
                                           + w1 * this->spectrumPowerSmoothed[(size_t) (bin - 1)]
                                           + w0 * this->spectrumPowerSmoothed[(size_t) (bin    )]
                                           + w1 * this->spectrumPowerSmoothed[(size_t) (bin + 1)]
                                           + w2 * this->spectrumPowerSmoothed[(size_t) (bin + 2)];
                            const float norm = w2 + w1 + w0 + w1 + w2; // 9
                            powerForAggregation[(size_t) bin] = acc / norm;
                        }
                    }
                }
                else
                {
                    powerForAggregation.assign(this->spectrumPowerSmoothed.begin(), this->spectrumPowerSmoothed.end());
                }

                // Capture tail after freq smoothing (pre-taper)
                {
                    const juce::SpinLock::ScopedLockType sl (this->spectrumLock);
                    const int allowedEnd = numBins - this->hiGuardBins - 1;
                    const int endBin = juce::jlimit (0, numBins - 1, allowedEnd);
                    const int startBin = juce::jlimit (0, endBin, endBin - (captureCount - 1));
                    this->debugTailBinsPostSmooth.resize ((size_t) (endBin - startBin + 1));
                    int destIndex = 0;
                    for (int bin = startBin; bin <= endBin; ++bin)
                        this->debugTailBinsPostSmooth[(size_t) destIndex++] = powerForAggregation[(size_t) bin];
                }

                // Apply gentle cosine taper over the last ~2% of allowed bins
                {
                    const int allowedEnd = juce::jlimit (0, numBins - 1, numBins - this->hiGuardBins - 1);
                    const float taperPercentClamped = juce::jlimit (0.0f, 0.2f, this->taperPercent.load());
                    const int taperBins = juce::jmax (1, (int) std::floor ((double) numBins * (double) taperPercentClamped));
                    const int taperStart = juce::jlimit (0, allowedEnd, allowedEnd - taperBins + 1);
                    for (int bin = taperStart; bin <= allowedEnd; ++bin)
                    {
                        const float taperProgress = (taperBins <= 1) ? 1.0f : (float) (bin - taperStart) / (float) (taperBins - 1);
                        // taperWeight from 1.0 at start to 0.0 at end
                        const float taperWeight = 0.5f * (1.0f + std::cos (juce::MathConstants<float>::pi * taperProgress));
                        powerForAggregation[(size_t) bin] *= juce::jlimit (0.0f, 1.0f, taperWeight);
                    }
                    // Zero strictly guarded region
                    for (int bin = allowedEnd + 1; bin < numBins; ++bin)
                        powerForAggregation[(size_t) bin] = 0.0f;
                }

                // Capture tail after taper
                int allowedEndBin = juce::jlimit (0, numBins - 1, numBins - this->hiGuardBins - 1);
                {
                    const juce::SpinLock::ScopedLockType sl (this->spectrumLock);
                    const int endBin = allowedEndBin;
                    const int startBin = juce::jlimit (0, endBin, endBin - (captureCount - 1));
                    this->debugTailBinsPostTaper.resize ((size_t) (endBin - startBin + 1));
                    int destIndex = 0;
                    for (int bin = startBin; bin <= endBin; ++bin)
                        this->debugTailBinsPostTaper[(size_t) destIndex++] = powerForAggregation[(size_t) bin];
                }

                // Aggregate linear bins into perceptual log-spaced bands for UI accuracy, esp. low-end
                if ((int) this->bandBinStart.size() != this->numBands || (int) this->bandBinEnd.size() != this->numBands)
                    this->buildLogBands();

                std::vector<float> bands;
                bands.assign((size_t) this->numBands, 0.0f);
                std::vector<float> bandsPreSmooth;
                bandsPreSmooth.assign((size_t) this->numBands, 0.0f);

                const int allowedEnd = juce::jlimit (1, numBins - 2, numBins - this->hiGuardBins - 1);
                const double binHz = this->currentSampleRate / (double) fftSize;
                for (int bandIndex = 0; bandIndex < this->numBands; ++bandIndex)
                {
                    // Fractional, bandwidth-normalised power aggregation per band
                    double bandStartHz = this->bandF0Hz.size() == (size_t) this->numBands ? this->bandF0Hz[(size_t) bandIndex] : 20.0;
                    double bandEndHz   = this->bandF1Hz.size() == (size_t) this->numBands ? this->bandF1Hz[(size_t) bandIndex] : this->currentSampleRate * 0.5;
                    if (bandEndHz <= bandStartHz) bandEndHz = bandStartHz + binHz; // ensure positive width

                    // Map to bin intervals [k*binHz, (k+1)*binHz)
                    int binStartIndex = juce::jlimit (1, allowedEnd, (int) std::floor (bandStartHz / binHz));
                    int binEndIndex   = juce::jlimit (1, allowedEnd, (int) std::floor (bandEndHz   / binHz));
                    if (binEndIndex < binStartIndex) binEndIndex = binStartIndex;

                    double sumPower = 0.0;
                    double weightSum = 0.0;
                    for (int binIndex = binStartIndex; binIndex <= binEndIndex; ++binIndex)
                    {
                        const double binStartHzEdge = (double) binIndex * binHz;
                        const double binEndHzEdge   = (double) (binIndex + 1) * binHz;
                        const double overlapStart = std::max (bandStartHz, binStartHzEdge);
                        const double overlapEnd   = std::min (bandEndHz,   binEndHzEdge);
                        const double overlapWidth = std::max (0.0, overlapEnd - overlapStart);
                        if (overlapWidth > 0.0)
                        {
                            sumPower  += (double) powerForAggregation[(size_t) binIndex] * overlapWidth;
                            weightSum += overlapWidth;
                        }
                    }

                    if (weightSum <= 0.0)
                    {
                        // Fallback: use nearest bin
                        const int nearestBinIndex = juce::jlimit (1, allowedEnd, (int) std::floor ((0.5 * (bandStartHz + bandEndHz)) / binHz));
                        sumPower = powerForAggregation[(size_t) nearestBinIndex];
                        weightSum = 1.0;
                    }

                    const double meanPower = sumPower / weightSum;
                    const float meanMag = (float) std::sqrt(juce::jmax(0.0, meanPower)) + 1e-20f;

                    float db = Decibels::gainToDecibels(meanMag, minDb);
                    db = jlimit(minDb, maxDb, db);
                    float normalised = jmap(db, minDb, maxDb, 0.0f, 1.0f);
                    // Keep a little visual headroom before 1.0f
                    normalised = jmin(normalised * 0.92f, 0.98f);
                    const float normClamped = juce::jlimit(0.0f, 1.0f, normalised);
                    bands[(size_t) bandIndex] = normClamped;
                    bandsPreSmooth[(size_t) bandIndex] = normClamped;
                }

                // Light band-domain smoothing to discourage isolated spikes at the top end
                if (this->bandSmoothEnabled.load() && this->numBands >= 3)
                {
                    // 3-point median filter on the top octave (no <algorithm> dependency)
                    auto median3 = [] (float valueA, float valueB, float valueC) -> float
                    {
                        float minValue = valueA;
                        if (valueB < minValue) minValue = valueB;
                        if (valueC < minValue) minValue = valueC;
                        float maxValue = valueA;
                        if (valueB > maxValue) maxValue = valueB;
                        if (valueC > maxValue) maxValue = valueC;
                        return (valueA + valueB + valueC) - minValue - maxValue;
                    };

                    std::vector<float> bandsSmooth((size_t) this->numBands, 0.0f);
                    const int startTop = (int) std::floor (this->numBands * 0.85); // top ~15%
                    bandsSmooth[0] = bands[0];
                    for (int bandIndex = 1; bandIndex < this->numBands - 1; ++bandIndex)
                    {
                        if (bandIndex >= startTop)
                            bandsSmooth[(size_t) bandIndex] = median3 (bands[(size_t) (bandIndex - 1)], bands[(size_t) bandIndex], bands[(size_t) (bandIndex + 1)]);
                        else
                            bandsSmooth[(size_t) bandIndex] = (bands[(size_t) (bandIndex - 1)] + 2.0f * bands[(size_t) bandIndex] + bands[(size_t) (bandIndex + 1)]) * 0.25f;
                    }
                    bandsSmooth[(size_t) (this->numBands - 1)] = (bands[(size_t) (this->numBands - 2)] + bands[(size_t) (this->numBands - 1)]) * 0.5f;
                    bands.swap(bandsSmooth);
                }

                {
                    const juce::SpinLock::ScopedLockType sl (this->spectrumLock);
                    this->spectrum = std::move(bands);
                    this->debugBandsPreBandSmooth = std::move(bandsPreSmooth);
                }

                this->fifoIndex = 0; // reset FIFO to start gathering next block
            }
        }
    }
}

void TrinityAudioProcessor::processBlock(AudioBuffer<double>& buffer,
                                         MidiBuffer& midi)
{
    AudioBuffer<float> temp(buffer.getNumChannels(), buffer.getNumSamples());

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        const double* src = buffer.getReadPointer(channel);
        float* dest = temp.getWritePointer(channel);

        for (int sampleIndex = 0; sampleIndex < buffer.getNumSamples(); ++sampleIndex)
        {
            dest[sampleIndex] = static_cast<float>(src[sampleIndex]);
        }
    }
    this->processBlock(temp, midi);
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        const float* src = temp.getReadPointer(channel);
        double* dest = buffer.getWritePointer(channel);

        for (int sampleIndex = 0; sampleIndex < buffer.getNumSamples(); ++sampleIndex)
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
    ignoreUnused (destData);
}

void TrinityAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    ignoreUnused (data, sizeInBytes);
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TrinityAudioProcessor();
}

void TrinityAudioProcessor::copySpectrum (std::vector<float>& dest) const
{
    const juce::SpinLock::ScopedLockType sl (this->spectrumLock);
    dest = this->spectrum;
}

void TrinityAudioProcessor::buildLogBands()
{
    const int numBins = fftSize / 2;
    this->bandBinStart.clear();
    this->bandBinEnd.clear();
    this->bandF0Hz.clear();
    this->bandF1Hz.clear();
    this->bandBinStart.reserve((size_t) this->numBands);
    this->bandBinEnd.reserve((size_t) this->numBands);
    this->bandF0Hz.reserve((size_t) this->numBands);
    this->bandF1Hz.reserve((size_t) this->numBands);

    const double binHz = this->currentSampleRate / (double) fftSize;
    const double fMin = 20.0; // start around 20 Hz
    // Map UI bands only up to the last allowed (unguarded) bin
    const int allowedEndBin = juce::jlimit (0, numBins - 1, numBins - this->hiGuardBins - 1);
    // Use the end of the last allowed bin as the display cap
    const double allowedEndHz = (double) (allowedEndBin + 1) * binHz;
    this->displayMaxHz = juce::jmax (fMin * 2.0, allowedEndHz);
    const double fMax = this->displayMaxHz;
    const double logMin = std::log(fMin);
    const double logMax = std::log(fMax);

    for (int bandIndex = 0; bandIndex < this->numBands; ++bandIndex)
    {
        // t spans [0,1]
        const double fractionStart = (double) bandIndex / (double) this->numBands;
        const double fractionEnd   = (double) (bandIndex + 1) / (double) this->numBands;
        double bandStartHz = std::exp(logMin + (logMax - logMin) * fractionStart);
        double bandEndHz   = std::exp(logMin + (logMax - logMin) * fractionEnd);
        // Clamp to [fMin .. allowedEndHz]
        bandStartHz = juce::jlimit (fMin, allowedEndHz, bandStartHz);
        bandEndHz   = juce::jlimit (fMin, allowedEndHz, bandEndHz);
        if (bandEndHz <= bandStartHz) bandEndHz = juce::jmin (allowedEndHz, bandStartHz + binHz);

        int bin0 = (int) std::floor(bandStartHz / binHz + 0.5); // round to nearest bin
        int bin1 = (int) std::floor(bandEndHz   / binHz + 0.5);

        bin0 = juce::jlimit(0, numBins - 1, bin0);
        bin1 = juce::jlimit(0, numBins - 1, bin1);
        if (bin1 < bin0) bin1 = bin0;

        this->bandBinStart.push_back(bin0);
        this->bandBinEnd.push_back(bin1);
        this->bandF0Hz.push_back(bandStartHz);
        this->bandF1Hz.push_back(bandEndHz);
    }

    // Resize UI spectrum buffer to match number of bands
    const juce::SpinLock::ScopedLockType sl (this->spectrumLock);
    this->spectrum.assign((size_t) this->numBands, 0.0f);
}

void TrinityAudioProcessor::copyDebugData (std::vector<float>& tailPreSmooth,
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
    const juce::SpinLock::ScopedLockType sl (this->spectrumLock);
    tailPreSmooth = this->debugTailBinsPreSmooth;
    tailPostSmooth = this->debugTailBinsPostSmooth;
    tailPostTaper = this->debugTailBinsPostTaper;
    bandsPreBandSmooth = this->debugBandsPreBandSmooth;
    bandsFinal = this->spectrum;
    outHiGuard = this->hiGuardBins;
    const int numBins = fftSize / 2;
    const int allowedEndBin = juce::jlimit (0, numBins - 1, numBins - this->hiGuardBins - 1);
    outAllowedEndBin = allowedEndBin;
    outAllowedEndHz = (double) (allowedEndBin + 1) * (this->currentSampleRate / (double) fftSize);
    outSampleRate = this->currentSampleRate;
    outFftSize = fftSize;
    outDisplayMaxHz = this->displayMaxHz;
}

void TrinityAudioProcessor::setGuardPercent (float p) noexcept
{
    const float clamped = jlimit (0.0f, 0.2f, p);
    this->guardPercent.store (clamped);
    const int numBins = fftSize / 2;
    this->hiGuardBins = jmax(8, static_cast<int>(std::floor(static_cast<double>(numBins) * static_cast<double>(clamped))));
    this->buildLogBands();
}

void TrinityAudioProcessor::generateTestSignal (AudioBuffer<float>& buffer)
{
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();
    const double sampleRateHz = this->currentSampleRate;
    const float amplitude = 0.25f;
    const int selectedType = this->testType.load();

    switch (selectedType)
    {
        case kSine17k:
        case kSine19k:
        {
            const double frequencyHz = selectedType == kSine17k ? 17000.0 : 19000.0;
            const double phaseIncrement = 2.0 * MathConstants<double>::pi * frequencyHz / sampleRateHz;
            for (int sampleIndex = 0; sampleIndex < numSamples; ++sampleIndex)
            {
                float sample = std::sin (this->testPhase) * amplitude;
                this->testPhase += phaseIncrement;
                if (this->testPhase > 2.0 * MathConstants<double>::pi)
                {
                    this->testPhase -= 2.0 * MathConstants<double>::pi;
                }
                for (int channelIndex = 0; channelIndex < numChannels; ++channelIndex)
                {
                    buffer.getWritePointer(channelIndex)[sampleIndex] = sample;
                }
            }
            break;
        }
        case kWhiteNoise:
        {
            for (int sampleIndex = 0; sampleIndex < numSamples; ++sampleIndex)
            {
                float sample = (this->rng.nextFloat() * 2.0f - 1.0f) * amplitude;
                for (int channelIndex = 0; channelIndex < numChannels; ++channelIndex)
                    buffer.getWritePointer(channelIndex)[sampleIndex] = sample;
            }
            break;
        }
        case kPinkNoise:
        {
            for (int sampleIndex = 0; sampleIndex < numSamples; ++sampleIndex)
            {
                float white = (this->rng.nextFloat() * 2.0f - 1.0f) * amplitude;
                // crude pink-ish: low-pass the white slightly
                this->pinkZ1 = this->pinkZ1 + this->pinkCoeff * (white - this->pinkZ1);
                float sample = this->pinkZ1;
                for (int channelIndex = 0; channelIndex < numChannels; ++channelIndex)
                    buffer.getWritePointer(channelIndex)[sampleIndex] = sample;
            }
            break;
        }
        case kLogSweep:
        {
            const double sweepStartHz = 20.0;
            const double sweepEndHz = this->displayMaxHz > sweepStartHz ? this->displayMaxHz : (sampleRateHz * 0.5 * 0.97);
            const double sweepDurationSeconds = (this->sweepTotalSamples > 0 ? (double) this->sweepTotalSamples / sampleRateHz : 10.0);
            for (int sampleIndex = 0; sampleIndex < numSamples; ++sampleIndex)
            {
                const double timeSeconds = (double) this->sweepSampleCount / sampleRateHz;
                const double sweepRatio = std::pow (sweepEndHz / sweepStartHz, 1.0 / sweepDurationSeconds);
                const double instantFreqHz = sweepStartHz * std::pow (sweepRatio, timeSeconds);
                this->sweepPhase += 2.0 * MathConstants<double>::pi * instantFreqHz / sampleRateHz;
                if (this->sweepPhase > 2.0 * MathConstants<double>::pi)
                {
                    this->sweepPhase -= 2.0 * MathConstants<double>::pi;
                }
                float sample = std::sin (this->sweepPhase) * amplitude;
                for (int channelIndex = 0; channelIndex < numChannels; ++channelIndex)
                {
                    buffer.getWritePointer(channelIndex)[sampleIndex] = sample;
                }
                ++this->sweepSampleCount;
                if (this->sweepSampleCount >= this->sweepTotalSamples)
                {
                    this->sweepSampleCount = 0;
                }
            }
            break;
        }
        default:
            // kOff or unknown: do nothing, keep incoming audio
            break;
    }
}
