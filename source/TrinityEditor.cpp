#include "TrinityEditor.h"
#include "TrinityProcessor.h"

TrinityAudioProcessorEditor::TrinityAudioProcessorEditor(TrinityAudioProcessor& processorRef)
    : AudioProcessorEditor(&processorRef),
      processor(processorRef)
{
    setSize(200, 300);
    startTimerHz(30);
}

void TrinityAudioProcessorEditor::timerCallback()
{
    float targetLevel = this->processor.getRMSLevel();
    constexpr float smoothing = 0.10f;
    this->displayLevel = this->displayLevel * (1.0f - smoothing) + targetLevel * smoothing;
    repaint();
}

void TrinityAudioProcessorEditor::paint(Graphics& graphics)
{
    graphics.fillAll(Colours::black);
    float meterHeight = this->displayLevel * static_cast<float>(getHeight());
    graphics.setColour(Colours::limegreen);
    int meterWidth = 40;
    int meterX = getWidth() / 2 - meterWidth / 2;
    int meterY = getHeight() - static_cast<int> (meterHeight);

    graphics.fillRect(meterX, meterY, meterWidth, static_cast<int> (meterHeight));

    graphics.setColour(Colours::white);
    graphics.setFont(16.0f);
    graphics.drawFittedText("Level",
                             getLocalBounds().reduced(4),
                             Justification::centredTop,
                             1);
}
