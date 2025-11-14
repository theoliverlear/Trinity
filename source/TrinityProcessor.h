#pragma once

#include <JuceHeader.h>
#include <atomic>

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

private:
    std::atomic<float> rmsLevel { 0.0f };

    std::atomic<float> totalLevel { 0.0f };
    std::atomic<float> lowLevel { 0.0f };
    std::atomic<float> midLevel { 0.0f };
    std::atomic<float> highLevel { 0.0f };

    dsp::LinkwitzRileyFilter<float> lowMidCrossover[2];
    dsp::LinkwitzRileyFilter<float> midHighCrossover[2];

    dsp::ProcessSpec processSpec {};
};

AudioProcessor* JUCE_CALLTYPE createPluginFilter();
