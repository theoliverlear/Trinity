#pragma once

#include <vector>
#include <algorithm>
#include "../models/UiDynamicsSettings.h"

// Service: applies UI magnitude smoothing and peak-hold behaviour.
// Keeps the heavy logic reusable and consistent across components.
struct UiMagnitudeProcessor
{
    static void process(const std::vector<float>& magnitudes,
                        std::vector<float>& smoothed,
                        std::vector<float>& peaks,
                        const UiDynamicsSettings& settings) noexcept
    {
        const size_t sampleCount = magnitudes.size();
        if (smoothed.size() != sampleCount)
        {
            smoothed.assign(sampleCount, 0.0f);
        }
        if (peaks.size() != sampleCount)
        {
            peaks.assign(sampleCount, 0.0f);
        }

        if (!settings.smoothingEnabled)
        {
            for (size_t index = 0; index < sampleCount; ++index)
            {
                const float inputMagnitude = magnitudes[index];
                const float clamped = inputMagnitude < 0.0f ? 0.0f : (inputMagnitude > 1.0f ? 1.0f : inputMagnitude);
                smoothed[index] = clamped;
            }
        }
        else
        {
            for (size_t index = 0; index < sampleCount; ++index)
            {
                const float inputMagnitude = magnitudes[index];
                const float targetValue = inputMagnitude < 0.0f ? 0.0f : (inputMagnitude > 1.0f ? 1.0f : inputMagnitude);
                const float currentValue = smoothed[index];
                const float smoothingCoefficient = targetValue > currentValue ? settings.attackCoeff
                                                                             : settings.releaseCoeff;
                smoothed[index] = currentValue * (1.0f - smoothingCoefficient) + targetValue * smoothingCoefficient;
            }
        }

        if (!settings.peakHoldEnabled)
        {
            std::fill(peaks.begin(), peaks.end(), 0.0f);
        }
        else
        {
            for (size_t index = 0; index < sampleCount; ++index)
            {
                const float decayedPeakValue = peaks[index] * settings.peakHoldDecay;
                peaks[index] = decayedPeakValue > smoothed[index] ? decayedPeakValue : smoothed[index];
            }
        }
    }
};
