#pragma once

#include <algorithm>
#include <cmath>

// Encapsulates segmented‑log frequency layout used by the spectrum analyzer.
// Segments (Hz): [1..100], [100..1k], [1k..10k], [10k..20k]
// Cumulative width at segment ends: 10%, 40%, 80%, 100%.
struct SegmentedFrequencyLayout
{
    // Segment end frequencies
    float endHz_100 { 100.0f };
    float endHz_1k { 1000.0f };
    float endHz_10k { 10000.0f };
    float referenceMaxHz { 20000.0f }; // design reference cap

    // Cumulative layout fractions at these segment ends
    float cumulativeAt100Hz { 0.10f };
    float cumulativeAt1kHz { 0.40f };
    float cumulativeAt10kHz { 0.80f };
    float cumulativeAtMaxHz { 1.00f };

    // Map a frequency (Hz) to the cumulative fraction [0..1] using log spacing within segments.
    float cumulativeFraction(float frequencyHz) const noexcept
    {
        const float hz = clamp(frequencyHz, 1.0f, this->referenceMaxHz);

        auto logNormalised = [](float inputHz, float startHz, float endHz) -> float
        {
            const float startFrequency = std::max(1.0f, startHz <= 0.0f ? 1.0f : startHz);
            const float endFrequency = std::max(1.0f, endHz);
            const float inputFrequency = std::max(1.0f, inputHz);
            const float logDifference = std::max(1e-6f, std::log(endFrequency) - std::log(startFrequency));
            const float logRatio = (std::log(inputFrequency) - std::log(startFrequency)) / logDifference;
            return clamp(logRatio, 0.0f, 1.0f);
        };

        if (hz <= this->endHz_100)
        {
            return 0.0f + logNormalised(hz, 1.0f, this->endHz_100) * (this->cumulativeAt100Hz - 0.0f);
        }
        if (hz <= this->endHz_1k)
        {
            return this->cumulativeAt100Hz + logNormalised(hz, this->endHz_100, this->endHz_1k)
                   * (this->cumulativeAt1kHz - this->cumulativeAt100Hz);
        }
        if (hz <= this->endHz_10k)
        {
            return this->cumulativeAt1kHz + logNormalised(hz, this->endHz_1k, this->endHz_10k)
                   * (this->cumulativeAt10kHz - this->cumulativeAt1kHz);
        }
        return this->cumulativeAt10kHz + logNormalised(hz, this->endHz_10k, this->referenceMaxHz)
               * (this->cumulativeAtMaxHz - this->cumulativeAt10kHz);
    }

private:
    static float clamp(float value, float low, float high) noexcept
    {
        return std::max(low, std::min(value, high));
    }
};
