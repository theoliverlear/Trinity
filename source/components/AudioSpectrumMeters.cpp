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
    this->meters[0].setDbRange(-120.0f, 0.0f);
    this->meters[1].setDbRange(-120.0f, 0.0f);
    this->meters[2].setDbRange(-120.0f, 0.0f);
    this->meters[3].setDbRange(-120.0f, 0.0f);
}

void AudioSpectrumMeters::initTickDisplays()
{
    this->meters[0].setShowTicks(true);
    this->meters[1].setShowTicks(false);
    this->meters[2].setShowTicks(false);
    this->meters[3].setShowTicks(false);
}

void AudioSpectrumMeters::setMeters(const std::array<MeterInfo, 4>& meterInfos)
{
    // Connect external level pointers and labels
    for (int meterIndex = 0; meterIndex < 4; ++meterIndex)
    {
        this->meters[static_cast<size_t>(meterIndex)].setLevelPointer(meterInfos[(size_t) meterIndex].levelPtr);
        this->meters[static_cast<size_t>(meterIndex)].setLabel(meterInfos[(size_t) meterIndex].label != nullptr
                                                   ? String(meterInfos[(size_t) meterIndex].label)
                                                   : String());
    }
    // Configure dB range and tick visibility
    this->initDbRanges();
    this->initTickDisplays();
    this->meters[0].setLeftGutterWidth(this->metrics.leftGutterWidth);
    for (int meterIndex = 1; meterIndex < 4; ++meterIndex)
    {
        this->meters[static_cast<size_t>(meterIndex)].setLeftGutterWidth (0.0f);
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

    // First meter gets gutter + content width
    this->meters[0].setBounds (xPosition, bounds.getY(), firstMeterGutter + meterColumnWidth, meterHeight);
    xPosition += firstMeterGutter + meterColumnWidth + gapBetweenMeters;

    // Remaining meters share the same content width
    this->meters[1].setBounds (xPosition, bounds.getY(), meterColumnWidth, meterHeight);
    xPosition += meterColumnWidth + gapBetweenMeters;
    this->meters[2].setBounds (xPosition, bounds.getY(), meterColumnWidth, meterHeight);
    xPosition += meterColumnWidth + gapBetweenMeters;
    this->meters[3].setBounds (xPosition, bounds.getY(), meterColumnWidth, meterHeight);
}
