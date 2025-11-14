#include "TrinityProcessor.h"
#include "TrinityEditor.h"

TrinityAudioProcessor::TrinityAudioProcessor()
    : AudioProcessor (
        BusesProperties()
            .withInput("Input",  AudioChannelSet::stereo(), true)
            .withOutput("Output", AudioChannelSet::stereo(), true)
      )
{
}

void TrinityAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    ignoreUnused(sampleRate, samplesPerBlock);
}

bool TrinityAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    auto mainOut = layouts.getMainOutputChannelSet();

    const bool isInvalidChannelSet = mainOut != AudioChannelSet::mono() && mainOut != AudioChannelSet::stereo();
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

    float sumSquares = 0.0f;
    int totalSamples = buffer.getNumSamples() * buffer.getNumChannels();

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        const float* data = buffer.getReadPointer(channel);

        for (int sampleIndex = 0; sampleIndex < buffer.getNumSamples(); ++sampleIndex)
        {
            float sampleValue = data[sampleIndex];
            sumSquares += sampleValue * sampleValue;
        }
    }

    if (totalSamples > 0)
    {
        float rms = std::sqrt(sumSquares / static_cast<float> (totalSamples));

        rms = jlimit(0.0f, 1.0f, rms);
        this->rmsLevel.store(rms);
    }
}

void TrinityAudioProcessor::processBlock(AudioBuffer<double>& buffer,
                                          MidiBuffer& midi)
{
    AudioBuffer<float> temp (buffer.getNumChannels(), buffer.getNumSamples());

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        const double* source = buffer.getReadPointer(channel);
        float* dest = temp.getWritePointer(channel);

        for (int sampleIndex = 0; sampleIndex < buffer.getNumSamples(); ++sampleIndex)
        {
            dest[sampleIndex] = static_cast<float> (source[sampleIndex]);
        }
    }
    processBlock(temp, midi);

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        const float* source = temp.getReadPointer(channel);
        double* dest = buffer.getWritePointer(channel);

        for (int sampleIndex = 0; sampleIndex < buffer.getNumSamples(); ++sampleIndex)
        {
            dest[sampleIndex] = static_cast<double> (source[sampleIndex]);
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
