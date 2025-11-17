#pragma once

#include <JuceHeader.h>

// Utility for mapping dB values to UI colours consistently across the app.
struct LevelColourScale
{
    static constexpr float greenToYellowStartDb = -20.0f;
    static constexpr float yellowToRedStartDb = -10.0f;
    static constexpr float maxDb = 0.0f;

    static Colour colourForDb(float decibels) noexcept
    {
        using namespace juce;
        if (decibels <= greenToYellowStartDb)
        {
            return Colours::green;
        }
        if (decibels <= yellowToRedStartDb)
        {
            const float ratio = (decibels - greenToYellowStartDb) / (yellowToRedStartDb - greenToYellowStartDb);
            float normalizedRatio = jlimit(0.0f, 1.0f, ratio);
            const uint8 red = static_cast<uint8>(normalizedRatio * 255.0f);
            const uint8 green = 255u;
            return Colour::fromRGB(red, green, 0u);
        }
        if (decibels <= maxDb)
        {
            const float ratio = (decibels - yellowToRedStartDb) / (maxDb - yellowToRedStartDb);
            const uint8 red = 255u;
            float normalizedRatio = jlimit(0.0f, 1.0f, ratio);
            const uint8 green = static_cast<uint8>((1.0f - normalizedRatio) * 255.0f);
            return Colour::fromRGB(red, green, 0u);
        }
        return Colours::red;
    }
};
