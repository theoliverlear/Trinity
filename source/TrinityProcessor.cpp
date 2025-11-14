#include "TrinityProcessor.h"
#include "TrinityEditor.h"

TrinityAudioProcessor::TrinityAudioProcessor()
    : AudioProcessor(
        BusesProperties()
            .withInput("Input",  AudioChannelSet::stereo(), true)
            .withOutput("Output", AudioChannelSet::stereo(), true))
{
}

void TrinityAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    ignoreUnused(sampleRate, samplesPerBlock);
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

    for (int channel = this->getTotalNumInputChannels(); channel < this->getTotalNumOutputChannels(); ++channel)
    {
        buffer.clear(channel, 0, buffer.getNumSamples());
    }

    float peak = 0.0f;
    const int numChannels = jmin(this->getTotalNumInputChannels(), buffer.getNumChannels());
    const int numSamples = buffer.getNumSamples();

    for (int channel = 0; channel < numChannels; ++channel)
    {
        const float* data = buffer.getReadPointer(channel);
        for (int i = 0; i < numSamples; ++i)
        {
            const float absValue = std::abs(data[i]);
            if (absValue > peak)
            {
                peak = absValue;
            }
        }
    }

    peak = jlimit(0.0f, 1.0f, peak);
    this->rmsLevel.store(peak);
}

void TrinityAudioProcessor::processBlock(AudioBuffer<double>& buffer,
                                         MidiBuffer& midi)
{
    AudioBuffer<float> temp(buffer.getNumChannels(), buffer.getNumSamples());

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        const double* src = buffer.getReadPointer(channel);
        float* dest = temp.getWritePointer(channel);

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            dest[i] = static_cast<float>(src[i]);
        }
    }

    processBlock(temp, midi);

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        const float* src = temp.getReadPointer(channel);
        double* dest = buffer.getWritePointer(channel);

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            dest[i] = static_cast<double>(src[i]);
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
