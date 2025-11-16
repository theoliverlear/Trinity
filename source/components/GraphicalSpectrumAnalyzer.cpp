#include "./GraphicalSpectrumAnalyzer.h"

namespace
{
    // Visual tuning constants
    constexpr float kVisualGain   = 0.90f;  // trim overall amplitude a touch
    constexpr float kVisualGamma  = 0.88f;  // perceptual curve ( <1 expands lows, >1 compresses )

    // Grid styling
    constexpr float kGridLineAlpha        = 0.06f;
    constexpr int   kHorizontalDivisions  = 4;     // 25% steps
    constexpr float kLabelTextAlpha       = 0.5f;
    constexpr float kLabelFontSize        = 10.0f;
    constexpr float kLabelYInset          = 14.0f;
    constexpr int   kLabelWidth           = 36;
    constexpr int   kLabelHeight          = 12;

    // Spectrum styling
    constexpr float kGlowAlpha            = 0.10f;
    constexpr float kGlowStrokeWidth      = 5.0f;
    constexpr float kOutlineAlpha         = 0.32f;
    constexpr float kOutlineStrokeWidth   = 1.5f;

    // Peaks styling
    constexpr float kPeakAlpha            = 0.5f;
    constexpr int   kPeakStep             = 2;     // draw every 2 bins
    constexpr float kPeakMarkerWidth      = 2.0f;
    constexpr float kPeakMarkerHalfWidth  = kPeakMarkerWidth * 0.5f;
    constexpr float kPeakMarkerYOffset    = 2.0f;
    constexpr float kPeakMarkerHeight     = 6.0f;

    // Vignette styling
    constexpr float kTopFadeAlpha         = 0.10f;
    constexpr float kSideFadeAlpha        = 0.18f;
    constexpr float kSideBarsAlpha        = 0.12f;
    constexpr float kTopVignetteHeightPct = 0.15f;
    constexpr float kSideVignetteWidth    = 8.0f;

    // Major frequency ticks to label
    constexpr float kMajorFreqs[] = {
        20.f, 30.f, 40.f, 50.f, 60.f, 80.f,
        100.f, 200.f, 300.f, 400.f, 500.f, 800.f,
        1000.f, 2000.f, 3000.f, 4000.f, 5000.f, 8000.f,
        10000.f, 16000.f, 20000.f
    };
}

void GraphicalSpectrumAnalyzer::setFrequencyRange(float minHz, float maxHz)
{
    if (maxHz <= 0.0f) return;
    if (minHz <= 0.0f) minHz = 1.0f;
    if (maxHz <= minHz) maxHz = minHz * 2.0f;
    this->freqMinHz = minHz;
    this->freqMaxHz = maxHz;
    this->repaint();
}

void GraphicalSpectrumAnalyzer::setMagnitudes(const float* values, int numValues)
{
    if (values == nullptr || numValues <= 0)
        return;

    this->magnitudes.assign (values, values + numValues);
    this->applySmoothingAndPeaks();
    this->repaint();
}

void GraphicalSpectrumAnalyzer::setMagnitudes (const std::vector<float>& values)
{
    this->magnitudes = values;
    this->applySmoothingAndPeaks();
    this->repaint();
}

void GraphicalSpectrumAnalyzer::paint (juce::Graphics& graphics)
{
    using namespace juce;

    // Background gradient (dark slate to near-black)
    auto bounds = this->getLocalBounds().toFloat();
    ColourGradient bgGrad { Colour::fromRGB (10, 14, 18), bounds.getTopLeft(),
                            Colour::fromRGB (3, 5, 8),   bounds.getBottomLeft(), false };
    bgGrad.addColour (0.5, Colour::fromRGB (6, 9, 13));
    graphics.setGradientFill (bgGrad);
    graphics.fillAll();

    if (this->magnitudes.empty())
        return;

    const float left    = bounds.getX();
    const float right   = bounds.getRight();
    const float bottom  = bounds.getBottom();
    const float height  = bounds.getHeight();
    const int   numBins   = static_cast<int> (std::max<std::size_t> (1, this->smoothed.size()));
    // Grid: horizontal bands + frequency ticks and labels
    this->drawGrid (graphics, bounds);

    // Build smoothed spectrum path with perceptual x mapping
    Path spectrumPath;
    spectrumPath.startNewSubPath(left, bottom);

    auto getX = [left, right, numBins](int binIndex)
    {
        if (numBins <= 1) return left;
        float proportion = (float) binIndex / (float) (numBins - 1);
        // Data arriving here are already log-spaced bands; use linear x to avoid double-warping
        return left + proportion * (right - left);
    };

    // Optionally precompute midpoints for a smoother curve using quadratic segments
    const int step = 1;
    for (int binIndex = 0; binIndex < numBins; binIndex += step)
    {
        const float norm = jlimit (0.0f, 1.0f, this->smoothed[(size_t) binIndex]);
        // apply headroom and gamma before mapping to Y
        float vis = jlimit (0.0f, 1.0f, norm * kVisualGain);
        vis = std::pow (vis, kVisualGamma);
        const float x    = getX (binIndex);
        const float y    = bottom - vis * height;
        spectrumPath.lineTo (x, y);
    }

    spectrumPath.lineTo(right, bottom);
    spectrumPath.closeSubPath();

    // Soft glow behind (slightly thinner to avoid over-brightness)
    graphics.setColour (Colour::fromRGB (0, 255, 255).withAlpha(kGlowAlpha));
    graphics.strokePath (spectrumPath, PathStrokeType (kGlowStrokeWidth, PathStrokeType::curved, PathStrokeType::rounded));

    // Gradient fill under curve (use points, not scalar coords)
    ColourGradient specGrad { Colour::fromRGB (0, 200, 255).withAlpha (0.80f), bounds.getTopLeft(),
                              Colour::fromRGB (20, 120, 255).withAlpha (0.60f), bounds.getBottomLeft(), false };
    specGrad.addColour(0.6, Colour::fromRGB (120, 80, 255).withAlpha (0.50f));
    graphics.setGradientFill (specGrad);
    graphics.fillPath (spectrumPath);

    // Crisp outline
    graphics.setColour(Colours::white.withAlpha (kOutlineAlpha));
    graphics.strokePath(spectrumPath, PathStrokeType (kOutlineStrokeWidth, PathStrokeType::curved, PathStrokeType::rounded));

    // Peak-hold markers
    if (this->peakHoldEnabled && ! this->peaks.empty())
    {
        graphics.setColour(Colours::yellow.withAlpha (kPeakAlpha));
        for (int binIndex = 0; binIndex < numBins; binIndex += kPeakStep) // every 2 bins for clarity
        {
            const float x = getX (binIndex);
            const float y = bottom - jlimit (0.0f, 1.0f, this->peaks[(size_t) binIndex]) * height;
            graphics.fillRect (Rectangle<float> (x - kPeakMarkerHalfWidth, y - kPeakMarkerYOffset, kPeakMarkerWidth, kPeakMarkerHeight));
        }
    }

    // Vignette overlay for a polished look
    this->drawVignetteOverlay(graphics, bounds);
}

// ===== Helpers =====
float GraphicalSpectrumAnalyzer::mapLogFrequencyToX (float hz, float xLeft, float xRight) const noexcept
{
    const float fMin = jmax (1.0f, this->freqMinHz);
    const float fMax = jmax (fMin * 1.01f, this->freqMaxHz);
    const float clampedFrequency = jlimit (fMin, fMax, hz);
    const float normalisedPosition = (std::log (clampedFrequency) - std::log (fMin)) / (std::log (fMax) - std::log (fMin));
    return xLeft + normalisedPosition * (xRight - xLeft);
}

String GraphicalSpectrumAnalyzer::formatFrequencyLabel (float frequency) noexcept
{
    if (frequency >= 1000.0f)
    {
        const float kFrequency = frequency / 1000.0f;
        return String (kFrequency, frequency >= 10000.0f ? 0 : 1) + "k";
    }
    return String (static_cast<int> (frequency));
}

void GraphicalSpectrumAnalyzer::drawGrid (Graphics& graphics, Rectangle<float> bounds) const
{
    using namespace juce;
    const float left   = bounds.getX();
    const float right  = bounds.getRight();
    const float top    = bounds.getY();
    const float bottom = bounds.getBottom();
    const float height = bounds.getHeight();

    // Horizontal bands
    graphics.setColour(Colours::white.withAlpha(kGridLineAlpha));
    for (int i = 1; i < kHorizontalDivisions; ++i)
    {
        const float y = bottom - (height * static_cast<float>(i) / static_cast<float>(kHorizontalDivisions));
        graphics.drawLine (left, y, right, y, 1.0f);
    }

    // Vertical frequency ticks
    graphics.setColour (Colours::white.withAlpha (kGridLineAlpha));
    for (float majorFrequencyHz : kMajorFreqs)
    {
        if (majorFrequencyHz < this->freqMinHz || majorFrequencyHz > this->freqMaxHz) continue;
        const float x = mapLogFrequencyToX(majorFrequencyHz, left, right);
        graphics.drawLine (x, top, x, bottom, 1.0f);
    }

    // Labels
    graphics.setColour (Colours::white.withAlpha (kLabelTextAlpha));
   #if JUCE_MODULE_AVAILABLE_juce_graphics
    #if JUCE_MAJOR_VERSION >= 7
        graphics.setFont (juce::Font (juce::FontOptions (kLabelFontSize)));
    #else
        graphics.setFont (juce::Font (kLabelFontSize));
    #endif
   #else
        graphics.setFont (juce::Font (kLabelFontSize));
   #endif
    const int labelY = static_cast<int> (bottom - kLabelYInset);
    for (float frequency : kMajorFreqs)
    {
        if (frequency < this->freqMinHz || frequency > this->freqMaxHz) continue;
        const float x = mapLogFrequencyToX (frequency, left, right);
        const String text = formatFrequencyLabel (frequency);
        const int labelX = static_cast<int> (x) - (kLabelWidth / 2);
        graphics.drawFittedText (text, { labelX, labelY, kLabelWidth, kLabelHeight }, juce::Justification::centred, 1);
    }
}

void GraphicalSpectrumAnalyzer::drawVignetteOverlay(Graphics& graphics, Rectangle<float> bounds) const
{
    using namespace juce;
    auto overlay = bounds;
    Colour topFade  = Colours::black.withAlpha (kTopFadeAlpha);
    Colour sideFade = Colours::black.withAlpha (kSideFadeAlpha);
    graphics.setGradientFill ({ topFade, overlay.getTopLeft(), sideFade, overlay.getBottomLeft(), false });
    graphics.fillRect (overlay.withHeight (overlay.getHeight() * kTopVignetteHeightPct));
    graphics.setColour (Colours::black.withAlpha (kSideBarsAlpha));
    graphics.fillRect (Rectangle(overlay.getX(), overlay.getY(), kSideVignetteWidth, overlay.getHeight()));
    graphics.fillRect (Rectangle(overlay.getRight() - kSideVignetteWidth, overlay.getY(), kSideVignetteWidth, overlay.getHeight()));
}

void GraphicalSpectrumAnalyzer::resized()
{
    // Nothing to lay out internally for now.
}

void GraphicalSpectrumAnalyzer::applySmoothingAndPeaks()
{
    const size_t n = this->magnitudes.size();
    if (n == 0)
        return;

    if (this->smoothed.size() != n)
    {
        this->smoothed.assign(n, 0.0f);
    }
    if (this->peaks.size() != n)
    {
        this->peaks.assign(n, 0.0f);
    }

    if (!this->smoothingEnabled)
    {
        // Bypass smoothing completely
        for (size_t i = 0; i < n; ++i)
        {
            this->smoothed[i] = jlimit(0.0f, 1.0f, this->magnitudes[i]);
        }
    }
    else
    {
        for (size_t i = 0; i < n; ++i)
        {
            float target = jlimit(0.0f, 1.0f, this->magnitudes[i]);
            float cur = this->smoothed[i];
            float coeff = (target > cur ? this->attackCoeff : this->releaseCoeff);
            this->smoothed[i] = cur * (1.0f - coeff) + target * coeff;
        }
    }

    if (!this->peakHoldEnabled)
    {
        std::fill(this->peaks.begin(), this->peaks.end(), 0.0f);
    }
    else
    {
        for (size_t i = 0; i < n; ++i)
        {
            this->peaks[i] = std::max(this->peaks[i] * this->peakHoldDecay, this->smoothed[i]);
        }
    }
}
