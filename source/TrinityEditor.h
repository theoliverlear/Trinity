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
    float displayLevel = 0.0f;
    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TrinityAudioProcessorEditor)
};
