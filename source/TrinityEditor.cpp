#include "TrinityEditor.h"
#include "TrinityProcessor.h"

namespace
{
    Colour getColour(float normalizedVolume)
    {
        using namespace juce;
        normalizedVolume = jlimit(0.0f, 1.0f, normalizedVolume);
        if (normalizedVolume < 0.5f)
        {
            const float ratio = normalizedVolume / 0.5f;
            return Colour::fromRGB(static_cast<uint8>(ratio * 255.0f), 255u,
                                   0u);
        }

        const float ratio = (normalizedVolume - 0.5f) / 0.5f;
        return Colour::fromRGB(
            255u, static_cast<uint8>((1.0f - ratio) * 255.0f), 0u);
    }
}

TrinityAudioProcessorEditor::TrinityAudioProcessorEditor(
    TrinityAudioProcessor& processorRef)
    : AudioProcessorEditor(&processorRef),
      processor(processorRef)
{
    setSize(300, 450);
    startTimerHz(30);
}

void TrinityAudioProcessorEditor::timerCallback()
{
    constexpr float smoothing = 0.10f;

    auto smooth = [smoothing](float current, float target)
    {
        return current * (1.0f - smoothing) + target * smoothing;
    };

    this->displayTotal = smooth(this->displayTotal, this->processor.getTotalLevel());
    this->displayLow = smooth(this->displayLow, this->processor.getLowLevel());
    this->displayMid = smooth(this->displayMid, this->processor.getMidLevel());
    this->displayHigh = smooth(this->displayHigh, this->processor.getHighLevel());
    repaint();
}

void TrinityAudioProcessorEditor::paint(Graphics& graphics)
{
    using namespace juce;
    graphics.fillAll(Colours::black);
    constexpr float minDb = -60.0f;
    struct MeterInfo
    {
        float levelLinear;
        const char* label;
    };

    MeterInfo meters[] =
    {
        {this->displayTotal, "Total"},
        {this->displayLow, "Low"},
        {this->displayMid, "Mid"},
        {this->displayHigh, "High"},
    };

    const int numMeters = (int) std::size(meters);
    const int meterWidth = 40;
    const int meterGap = 10;
    const int totalWidth = numMeters * meterWidth + (numMeters - 1) *
        meterGap;
    const int startX = (getWidth() - totalWidth) / 2;
    const int bottomY = getHeight();

    graphics.setFont(14.0f);

    for (int i = 0; i < numMeters; ++i)
    {
        const auto& meter = meters[i];
        const float db = Decibels::gainToDecibels(meter.levelLinear, minDb);
        float normalised = jmap(db, minDb, 0.0f, 0.0f, 1.0f);
        normalised = jlimit(0.0f, 1.0f, normalised);
        const float meterHeight = normalised * (float) getHeight();
        const int x = startX + i * (meterWidth + meterGap);
        const int y = bottomY - (int) meterHeight;

        graphics.setColour(getColour(normalised));
        graphics.fillRect(x, y, meterWidth, (int) meterHeight);

        graphics.setColour(Colours::white);

        Rectangle<int> labelArea(x, 0, meterWidth, 20);
        graphics.drawFittedText(meter.label, labelArea, Justification::centred, 1);

        if (meter.levelLinear > 0.0f)
        {
            Rectangle<int> dbArea(x, 20, meterWidth, 20);
            graphics.drawFittedText(String(db, 1) + " dB",
                             dbArea,
                             Justification::centred,
                             1);
        }
    }
}
