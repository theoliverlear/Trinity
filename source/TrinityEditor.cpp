#include "TrinityEditor.h"
#include "TrinityProcessor.h"

TrinityAudioProcessorEditor::TrinityAudioProcessorEditor(TrinityAudioProcessor& processorRef)
    : AudioProcessorEditor(&processorRef),
      processor(processorRef)
{
    this->setSize(300, 450);
    startTimerHz(30);
}

void TrinityAudioProcessorEditor::timerCallback()
{
    const float targetLevel = this->processor.getRMSLevel();
    constexpr float smoothing = 0.10f;
    this->displayLevel = this->displayLevel * (1.0f - smoothing) + targetLevel * smoothing;
    repaint();
}

Colour getColour(float value)
{
    if (value < 0.5f)
    {
        const float ratio = value / 0.5f;
        return Colour::fromRGB(static_cast<uint8>(ratio * 255), 255, 0);
    }
    const float ratio = (value - 0.5f) / 0.5f;
    return Colour::fromRGB(255, static_cast<uint8>((1.0f - ratio) * 255), 0);
}

void TrinityAudioProcessorEditor::paint(Graphics& graphics)
{
    graphics.fillAll(Colours::black);
    constexpr float minDb = -60.0f;
    const float dbLevel = Decibels::gainToDecibels(this->displayLevel, minDb);
    float normalized = jmap(dbLevel, minDb, 0.0f, 0.0f, 1.0f);
    normalized = jlimit(0.0f, 1.0f, normalized);
    const float meterHeight = normalized * static_cast<float>(getHeight());
    Colour meterColour = getColour(normalized);

    const int meterWidth = 40;
    const int meterX = this->getWidth() / 2 - meterWidth / 2;
    const int meterY = this->getHeight() - static_cast<int>(meterHeight);

    graphics.setColour(meterColour);
    graphics.fillRect(meterX, meterY, meterWidth, static_cast<int>(meterHeight));

    graphics.setColour(Colours::white);
    graphics.setFont(16.0f);

    graphics.drawFittedText("Level",
                     getLocalBounds().reduced(4),
                     Justification::centredTop,
                     1);

    if (this->displayLevel > 0.0f)
    {
        Rectangle meterLabelArea(meterX, 0, meterWidth, getHeight() / 3);
        graphics.drawFittedText(String(dbLevel, 1) + " dB",
                         meterLabelArea,
                         Justification::centredBottom,
                         1);
    }
}
