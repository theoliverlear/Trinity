#pragma once

#include <JuceHeader.h>

// Style/configuration structs for GraphicalSpectrumAnalyzer extracted from
// scattered local constants. Keeping them here improves reuse and readability.
struct VisualTuning
{
    float visualGain { 0.90f };   // trim overall amplitude a touch
    float visualGamma { 0.88f };  // perceptual curve (<1 expands lows, >1 compresses)
};

struct GridStyleConfig
{
    // Horizontal bands
    float gridLineAlpha { 0.06f };
    int horizontalDivisions { 4 }; // 25% steps

    // Labels
    float labelTextAlpha { 0.5f };
    float labelFontSize { 14.0f };
    float labelYInset { 18.0f };
    int labelWidth { 44 };
    int labelHeight { 16 };
    int labelMinGap { 4 }; // minimum pixel gap between labels to avoid overlap

    // Ticks
    float minorTickAlpha { 0.05f };
    float majorTickAlpha { 0.10f };
    float minorTickWidth { 1.0f };
    float majorTickWidth { 1.5f };
};

struct SpectrumRenderStyle
{
    float glowAlpha { 0.10f };
    float glowStrokeWidth { 6.0f };
    float outlineAlpha { 0.32f };
    float outlineStrokeWidth { 2.0f };
};

struct PeakMarkerStyle
{
    float alpha { 0.5f };
    int step { 2 };               // draw every 2 bins
    float markerWidth { 3.0f };
    float markerYOffset { 2.0f };
    float markerHeight { 6.0f };

    float markerHalfWidth() const noexcept
    {
        return this->markerWidth * 0.5f;
    }
};

// Background gradient for the analyzer canvas.
struct AnalyzerBackgroundStyle
{
    Colour topLeft { Colour::fromRGB(10, 14, 18) };   // dark slate
    Colour bottomLeft { Colour::fromRGB(3, 5, 8) };   // near-black
    Colour midTint { Colour::fromRGB(6, 9, 13) };     // subtle mid

    ColourGradient buildBackgroundGradient(const Rectangle<float>& bounds) const
    {
        ColourGradient grad { this->topLeft, bounds.getTopLeft(),
                                    this->bottomLeft, bounds.getBottomLeft(), false };
        grad.addColour(0.5, this->midTint);
        return grad;
    }
};

// Fill gradient under the spectrum curve.
struct SpectrumFillGradientStyle
{
    Colour top { Colour::fromRGB(0, 200, 255).withAlpha(0.80f) };
    Colour bottom { Colour::fromRGB(20, 120, 255).withAlpha(0.60f) };
    Colour mid { Colour::fromRGB(120, 80, 255).withAlpha(0.50f) };

    ColourGradient buildFillGradient(const Rectangle<float>& bounds) const
    {
        ColourGradient gradient { this->top, bounds.getTopLeft(),
                                    this->bottom, bounds.getBottomLeft(), false };
        gradient.addColour(0.6, this->mid);
        return gradient;
    }
};

struct VignetteStyle
{
    float topFadeAlpha { 0.10f };
    float sideFadeAlpha { 0.18f };
    float sideBarsAlpha { 0.12f };
    float topVignetteHeightPct { 0.15f };
    float sideVignetteWidth { 8.0f };
};
