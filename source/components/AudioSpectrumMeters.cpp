#include "AudioSpectrumMeters.h"

using namespace juce;

AudioSpectrumMeters::AudioSpectrumMeters()
{
    for (auto& meter : this->meters)
    {
        this->addAndMakeVisible(meter);
    }
}

void AudioSpectrumMeters::initDbRanges()
{
    for (int i = 0; i < meters.size(); ++i)
    {
        this->meters[i].setDbRange(-120.0f, 0.0f);
    }
}

void AudioSpectrumMeters::initTickDisplays()
{
    this->meters[0].setShowTicks(true);
    for (int i = 1; i < meters.size(); ++i)
    {
        this->meters[i].setShowTicks(false);
    }
}

void AudioSpectrumMeters::setMeters(const std::array<MeterInfo, 4>& meterInfos)
{
    // Connect external level pointers and labels
    for (int meterIndex = 0; meterIndex < 4; ++meterIndex)
    {
        size_t meter_index = static_cast<size_t>(meterIndex);
        this->meters[meter_index].setLevelPointer(meterInfos[meterIndex].levelPtr);
        this->meters[meter_index].setLabel(meterInfos[meterIndex].label != nullptr
                                                   ? String(meterInfos[meterIndex].label)
                                                   : String());
    }
    // Configure dB range and tick visibility
    this->initDbRanges();
    this->initTickDisplays();
    this->meters[0].setLeftGutterWidth(this->metrics.leftGutterWidth);
    for (int meterIndex = 1; meterIndex < 4; ++meterIndex)
    {
        size_t meter_index = static_cast<size_t>(meterIndex);
        this->meters[meter_index].setLeftGutterWidth(0.0f);
    }

    this->resized();
    this->repaint();
}

void AudioSpectrumMeters::advanceFrame()
{
    for (auto& meter : this->meters)
    {
        meter.advanceFrame();
    }
}

void AudioSpectrumMeters::resized()
{
    this->layoutMetersWithin(this->getLocalBounds());
}

void AudioSpectrumMeters::paint(Graphics&)
{
    // Transparent; children draw themselves.
}

int AudioSpectrumMeters::computeTotalGroupWidth() const noexcept
{
    return this->metrics.computeTotalGroupWidth();
}

void AudioSpectrumMeters::layoutMetersWithin(Rectangle<int> bounds)
{
    const int meterColumnWidth = this->metrics.meterColumnWidth();
    const int firstMeterGutter = this->metrics.firstMeterGutter();
    const int gapBetweenMeters = this->metrics.gapBetweenMeters();

    const int totalGroupWidth = this->computeTotalGroupWidth();
    int xPosition = bounds.getX() + (bounds.getWidth() - totalGroupWidth) / 2;
    const int meterHeight = bounds.getHeight();

    for (int i = 0; i < meters.size(); ++i)
    {
        int initialGutter = i == 0 ? firstMeterGutter : 0;
        this->meters[i].setBounds(xPosition, bounds.getY(), meterColumnWidth + initialGutter, meterHeight);
        xPosition += meterColumnWidth + initialGutter + gapBetweenMeters;
    }
}
