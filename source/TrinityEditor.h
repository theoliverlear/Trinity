#pragma once

#include <JuceHeader.h>

class TrinityAudioProcessor;

class TrinityAudioProcessorEditor : public AudioProcessorEditor,
                                     Timer
{
public:
    explicit TrinityAudioProcessorEditor(TrinityAudioProcessor& processorRef);
    ~TrinityAudioProcessorEditor() override = default;

    void paint(Graphics& graphics) override;
    void resized() override {}

private:
    TrinityAudioProcessor& processor;
    void timerCallback() override;
    float displayTotal { 0.0f };
    float displayLow { 0.0f };
    float displayMid { 0.0f };
    float displayHigh { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TrinityAudioProcessorEditor)
};
