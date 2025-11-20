#pragma once

#include <JuceHeader.h>
#include <atomic>
#include "models/TestSignalType.h"

class AudioTestProcessor
{
public:

    void prepare(double sampleRate, double displayMaxHz)
    {
        this->sampleRateHz = sampleRate > 0.0 ? sampleRate : 44100.0;
        this->displayMaxHz = displayMaxHz;
        this->testPhase = 0.0f;
        this->sweepPhase = 0.0f;
        this->sweepSampleCount = 0;
        this->sweepTotalSamples = static_cast<long long>(this->sampleRateHz * 10.0); // 10-second sweep
        this->pinkZ1 = 0.0f;
    }

    void setEnabled(bool enabled) noexcept
    {
        this->enabled.store(enabled);
    }
    bool isEnabled() const noexcept
    {
        return this->enabled.load();
    }

    void setType(int type) noexcept
    {
        this->type.store(type);
    }
    int getType() const noexcept
    {
        return this->type.load();
    }

    void generate(AudioBuffer<float>& buffer)
    {
        const int numSamples = buffer.getNumSamples();
        const float amplitude = 0.25f;
        const int selectedType = this->type.load();

        switch (selectedType)
        {
            case kSine17k:
            case kSine19k:
                handleSine(buffer, numSamples, amplitude, selectedType);
                break;
            case kWhiteNoise:
                handleWhiteNoise(buffer, numSamples, amplitude);
                break;
            case kPinkNoise:
                handlePinkNoise(buffer, numSamples, amplitude);
                break;
            case kLogSweep:
                handleSweep(buffer, numSamples, amplitude);
                break;
            default:
                break; // kOff or unknown: keep incoming audio
        }
    }

private:
    static void writeSampleToAllChannels(AudioBuffer<float>& buffer, int sampleIndex, float value) noexcept
    {
        const int channels = buffer.getNumChannels();
        for (int channel = 0; channel < channels; ++channel) {
            buffer.getWritePointer(channel)[sampleIndex] = value;
        }
    }

    void handleSine(AudioBuffer<float>& buffer, int numSamples, float amplitude, int selectedType)
    {
        const double frequencyHz = selectedType == kSine17k ? 17000.0 : 19000.0;
        const float phaseIncrement = MathConstants<float>::twoPi * static_cast<float>(frequencyHz / this->sampleRateHz);
        for (int i = 0; i < numSamples; ++i)
        {
            const float sample = std::sin(this->testPhase) * amplitude;
            this->testPhase += phaseIncrement;
            if (this->testPhase > MathConstants<float>::twoPi)
                this->testPhase -= MathConstants<float>::twoPi;
            writeSampleToAllChannels(buffer, i, sample);
        }
    }

    void handleWhiteNoise(AudioBuffer<float>& buffer, int numSamples, float amplitude)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            const float sample = (this->rng.nextFloat() * 2.0f - 1.0f) * amplitude;
            writeSampleToAllChannels(buffer, i, sample);
        }
    }

    void handlePinkNoise(AudioBuffer<float>& buffer, int numSamples, float amplitude)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            const float white = (this->rng.nextFloat() * 2.0f - 1.0f) * amplitude;
            // crude pink-ish: low-pass the white slightly
            this->pinkZ1 = this->pinkZ1 + this->pinkCoeff * (white - this->pinkZ1);
            writeSampleToAllChannels(buffer, i, this->pinkZ1);
        }
    }

    void handleSweep(AudioBuffer<float>& buffer, int numSamples, float amplitude)
    {
        const double sweepStartHz = 20.0;
        const double sweepEndHz = this->displayMaxHz > sweepStartHz ? this->displayMaxHz : this->sampleRateHz * 0.5 * 0.97;
        const double sweepDurationSeconds = this->sweepTotalSamples > 0 ? static_cast<double>(this->sweepTotalSamples) / this->sampleRateHz : 10.0;
        for (int i = 0; i < numSamples; ++i)
        {
            const double timeSeconds = static_cast<double>(this->sweepSampleCount) / this->sampleRateHz;
            const double sweepRatio = std::pow(sweepEndHz / sweepStartHz, 1.0 / sweepDurationSeconds);
            const double instantFreqHz = sweepStartHz * std::pow(sweepRatio, timeSeconds);
            const float phaseIncrement = MathConstants<float>::twoPi * static_cast<float>(instantFreqHz / this->sampleRateHz);
            this->sweepPhase += phaseIncrement;
            if (this->sweepPhase > MathConstants<float>::twoPi) {
                this->sweepPhase -= MathConstants<float>::twoPi;
            }
            const float sample = std::sin(this->sweepPhase) * amplitude;
            writeSampleToAllChannels(buffer, i, sample);
            ++this->sweepSampleCount;
            if (this->sweepSampleCount >= this->sweepTotalSamples)
                this->sweepSampleCount = 0;
        }
    }

private:
    std::atomic<bool> enabled { false };
    std::atomic<int> type { kOff };

    float testPhase { 0.0f };
    float sweepPhase { 0.0f };
    long long sweepSampleCount { 0 };
    long long sweepTotalSamples { 0 };

    Random rng;
    float pinkZ1 { 0.0f };
    float pinkCoeff { 0.02f };

    double sampleRateHz { 44100.0 };
    double displayMaxHz { 20000.0 };
};
