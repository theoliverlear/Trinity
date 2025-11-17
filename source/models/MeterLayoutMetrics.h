#pragma once

struct MeterLayoutMetrics
{
    float meterContentWidth { 72.0f }; // width of each meter column (excludes first-meter gutter)
    float meterGap { 16.0f };          // horizontal gap between meters
    float leftGutterWidth { 64.0f };   // gutter reserved on first meter for ticks/labels

    int meterColumnWidth() const noexcept
    {
        return static_cast<int>(this->meterContentWidth);
    }
    int firstMeterGutter() const noexcept
    {
        return static_cast<int>(this->leftGutterWidth);
    }
    int gapBetweenMeters() const noexcept
    {
        return static_cast<int>(this->meterGap);
    }

    int computeTotalGroupWidth() const noexcept
    {
        return this->firstMeterGutter() + this->meterColumnWidth() * 4 + this->gapBetweenMeters() * 3;
    }
};
