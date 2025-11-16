#pragma once

#include <JuceHeader.h>
#include <vector>
#include <cmath>
#include <algorithm>

class GraphicalSpectrumAnalyzer : public Component
{
public:
    GraphicalSpectrumAnalyzer() = default;
    ~GraphicalSpectrumAnalyzer() override = default;

    // Enable/disable internal UI smoothing (attack/release). Default: enabled.
    void setSmoothingEnabled(bool enabled)
    {
        this->smoothingEnabled = enabled;
        this->repaint();
    }
    bool isSmoothingEnabled() const noexcept
    {
        return this->smoothingEnabled;
    }

    // Enable/disable peak-hold markers. Default: enabled.
    void setPeakHoldEnabled(bool enabled)
    {
        this->peakHoldEnabled = enabled;
        this->repaint();
    }
    bool isPeakHoldEnabled() const noexcept
    {
        return this->peakHoldEnabled;
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

    // Style and dynamics
    float attackCoeff { 0.35f }; // faster rise
    float releaseCoeff { 0.08f }; // slower fall
    float peakHoldDecay { 0.97f }; // per update decay

    bool smoothingEnabled { true };
    bool peakHoldEnabled { true };

    // Frequency range for labels/ticks
    float freqMinHz { 20.0f };
    float freqMaxHz { 20000.0f };

    void applySmoothingAndPeaks();

    // Helpers (extracted for readability)
    // Map frequency to x (log scale within current [freqMinHz, freqMaxHz] range)
    float mapLogFrequencyToX(float hz, float xLeft, float xRight) const noexcept;

    // Frequency label formatting (e.g., 1k, 10k, 250)
    static String formatFrequencyLabel(float frequency) noexcept;

    // Draw grid (horizontal bands + frequency ticks and labels)
    void drawGrid(Graphics& graphics, Rectangle<float> bounds) const;

    // Vignette overlay
    void drawVignetteOverlay (Graphics& graphics, Rectangle<float> bounds) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GraphicalSpectrumAnalyzer)
};
