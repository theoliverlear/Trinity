#pragma once

#include <JuceHeader.h>

// Encapsulates colours and sizing for the AudioMeter component UI.
struct MeterVisualStyle
{
    // Background colours
    Colour backgroundTop { Colour::fromRGB(18, 22, 28) };
    Colour backgroundBottom { Colour::fromRGB(6, 8, 12) };
    Colour backgroundMidTint { Colour::fromRGB(10, 13, 18) };

    // Fill gradient colours (low -> mid -> high)
    Colour fillLow { Colour::fromRGB(40, 220, 120) };
    Colour fillMid { Colour::fromRGB(240, 200, 40) };
    Colour fillHigh { Colour::fromRGB(255, 70, 60) };

    // Geometry constants
    float cornerRadius { 8.0f };
    float innerPadding { 4.0f };
    float ledSize { 10.0f };

    ColourGradient buildBackgroundGradient(const Rectangle<float>& bounds) const
    {
        ColourGradient gradient(this->backgroundTop, bounds.getTopLeft(),this->backgroundBottom, bounds.getBottomLeft(), false);
        gradient.addColour(0.5, this->backgroundMidTint);
        return gradient;
    }

    ColourGradient buildFillGradient(const Rectangle<float>& innerRect) const
    {
        ColourGradient gradient (this->fillLow, innerRect.getBottomLeft(),
                                       this->fillHigh, innerRect.getTopLeft(), false);
        gradient.addColour(0.65, this->fillMid);
        return gradient;
    }
};
