#pragma once

#include <JuceHeader.h>

struct BandTints
{
    Colour low { Colour::fromRGBA(0, 120, 255, 18) };  // soft blue, ~7%
    Colour mid { Colour::fromRGBA(255, 255, 255, 12) }; // light neutral, ~5%
    Colour high { Colour::fromRGBA(180, 80, 255, 18) };  // soft violet, ~7%
};
