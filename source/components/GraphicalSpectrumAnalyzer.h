#pragma once

#include <JuceHeader.h>
#include <vector>
#include <cmath>
#include <algorithm>
#include "../models/FrequencyRange.h"
#include "../models/UiDynamicsSettings.h"
#include "../models/SpectrumAnalyzerStyle.h"
#include "../models/SegmentedFrequencyLayout.h"

class GraphicalSpectrumAnalyzer : public Component
{
public:
    GraphicalSpectrumAnalyzer() = default;
    ~GraphicalSpectrumAnalyzer() override = default;

    // Enable/disable internal UI smoothing (attack/release). Default: enabled.
    void setSmoothingEnabled(bool enabled)
    {
        this->uiSettings.smoothingEnabled = enabled;
        this->repaint();
    }
    bool isSmoothingEnabled() const noexcept
    {
        return this->uiSettings.smoothingEnabled;
    }

    // Enable/disable peak-hold markers. Default: enabled.
    void setPeakHoldEnabled(bool enabled)
    {
        this->uiSettings.peakHoldEnabled = enabled;
        this->repaint();
    }
    bool isPeakHoldEnabled() const noexcept
    {
        return this->uiSettings.peakHoldEnabled;
    }

    // Set the display frequency range used for drawing tick marks
    void setFrequencyRange(float minHz, float maxHz);

    /** Provide a new block of magnitudes to display.
        Values should be normalised 0.0f .. 1.0f (0 = silence, 1 = full scale).
        You can call this from your editor's timerCallback.
    */
    void setMagnitudes(const float* values, int numValues);

    void setMagnitudes(const std::vector<float>& values);

    void paint(Graphics& graphics) override;

    void resized() override;

private:
    // incoming magnitudes [0..1]
    std::vector<float> magnitudes;
    // smoothed magnitudes for stable display
    std::vector<float> smoothed;
    // peak-hold values per bin
    std::vector<float> peaks;

    // Style and dynamics consolidated
    UiDynamicsSettings uiSettings {};

    // Frequency range for labels/ticks
    FrequencyRange frequencyRange {};

    // Extracted styles/config
    VisualTuning visualTuning {};
    GridStyleConfig gridStyle {};
    SpectrumRenderStyle spectrumStyle {};
    PeakMarkerStyle peakStyle {};
    VignetteStyle vignetteStyle {};
    SegmentedFrequencyLayout segmentedLayout {};
    AnalyzerBackgroundStyle backgroundStyle {};
    SpectrumFillGradientStyle fillStyle {};

    void applySmoothingAndPeaks();

    // Helpers (extracted for readability)
    // Map frequency to x (log scale within current display range)
    float mapLogFrequencyToX(float hz, float xLeft, float xRight) const noexcept;

    // Custom segmented mapping requested by design:
    // 0–100 Hz = 10% width, 100–1k = 30%, 1k–10k = 40%, 10k–20k = 20%.
    // Within each segment, use logarithmic spacing between the segment bounds
    // to preserve perceptual distribution while enforcing the exact width ratios.
    float mapSegmentedFrequencyToX(float hz, float xLeft, float xRight) const noexcept;

    // Frequency label formatting (e.g., 1k, 10k, 250)
    static String formatFrequencyLabel(float frequency) noexcept;

    // Draw grid (horizontal bands + frequency ticks and labels)
    void drawGrid(Graphics& graphics, Rectangle<float> bounds) const;

    // Vignette overlay
    void drawVignetteOverlay(Graphics& graphics, Rectangle<float> bounds) const;

    // New helpers to improve readability and maintainability
    // Draw subtle background rectangles for Low/Mid/High bands
    void drawBandBackgrounds(Graphics& graphics, Rectangle<float> bounds) const;

    // Build the spectrum line (open) and fill (closed) paths given current smoothed magnitudes
    void buildSpectrumPaths(const Rectangle<float>& bounds,
                            Path& outLinePath,
                            Path& outFillPath) const;

    // Draw peak-hold markers
    void drawPeakMarkers(Graphics& graphics, Rectangle<float> bounds) const;

    // Map a band/bin index to x position across [xLeft, xRight] with linear spacing
    static float computeBinXPosition(int binIndex, int binCount, float xLeft, float xRight) noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GraphicalSpectrumAnalyzer)
};
