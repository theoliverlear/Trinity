#pragma once

#include <JuceHeader.h>
#include <array>
#include "AudioMeter.h"
#include "../models/MeterInfo.h"
#include "../models/MeterLayoutMetrics.h"

// Container for four AudioMeter components, laid out side-by-side and centred.
// The first meter shows dB ticks/labels in a left gutter so the group looks polished.
class AudioSpectrumMeters : public Component
{
public:
    AudioSpectrumMeters();
    void initDbRanges();
    void initTickDisplays();
    ~AudioSpectrumMeters() override = default;

    void setMeters(const std::array<MeterInfo, 4>& infos);

    void advanceFrame();

    void resized() override;

    void paint(Graphics& graphics) override;

private:
    std::array<AudioMeter, 4> meters;

    MeterLayoutMetrics metrics; // groups widths/gaps and total width helpers

    int computeTotalGroupWidth() const noexcept;

    void layoutMetersWithin(Rectangle<int> bounds);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioSpectrumMeters)
};
