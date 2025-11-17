#pragma once

struct FrequencyRange
{
    float minHz { 20.0f };
    float maxHz { 20000.0f };

    void set(float newMinHz, float newMaxHz) noexcept
    {
        if (newMaxHz <= 0.0f)
        {
            return;
        }
        if (newMinHz <= 0.0f)
        {
            newMinHz = 1.0f;
        }
        if (newMaxHz <= newMinHz)
        {
            newMaxHz = newMinHz * 2.0f;
        }
        this->minHz = newMinHz;
        this->maxHz = newMaxHz;
    }

    float clampedMin() const noexcept
    {
        bool isNegativeMin = this->minHz <= 0.0f;
        return isNegativeMin ? 1.0f : this->minHz;
    }
    float clampedMax() const noexcept
    {
        return this->maxHz <= clampedMin() ? clampedMin() * 2.0f : this->maxHz;
    }
};
