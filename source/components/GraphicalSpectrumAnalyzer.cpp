#include "./GraphicalSpectrumAnalyzer.h"
#include "../models/PlotGeometry.h"
#include "../models/BandFrequencies.h"
#include "../models/BandTints.h"
#include "../services/UiMagnitudeProcessor.h"

void GraphicalSpectrumAnalyzer::setFrequencyRange(float minHz, float maxHz)
{
    this->frequencyRange.set(minHz, maxHz);
    this->repaint();
}

void GraphicalSpectrumAnalyzer::setMagnitudes(const float* values, int numValues)
{
    if (values == nullptr || numValues <= 0)
    {
        return;
    }

    this->magnitudes.assign(values, values + numValues);
    this->applySmoothingAndPeaks();
    this->repaint();
}

void GraphicalSpectrumAnalyzer::setMagnitudes(const std::vector<float>& values)
{
    this->magnitudes = values;
    this->applySmoothingAndPeaks();
    this->repaint();
}

void GraphicalSpectrumAnalyzer::paint(Graphics& graphics)
{
    using namespace juce;

    // Background gradient
    auto bounds = this->getLocalBounds().toFloat();
    graphics.setGradientFill(this->backgroundStyle.buildBackgroundGradient(bounds));
    graphics.fillAll();

    // Create inner plotting bounds to leave left/right margins for better spacing
    const Rectangle<float> plotBounds = bounds.reduced(this->vignetteStyle.sideVignetteWidth, 0.0f);

    // Subtle band backgrounds for Low / Mid / High ranges within the plot area
    this->drawBandBackgrounds(graphics, plotBounds);

    if (this->magnitudes.empty())
    {
        return;
    }

    // Grid: horizontal bands + frequency ticks and labels within the plot area
    this->drawGrid(graphics, plotBounds);

    // Build spectrum paths via helper
    Path linePath;   // open path for glow/outline strokes
    Path fillPath;   // closed path for gradient fill only
    this->buildSpectrumPaths(plotBounds, linePath, fillPath);

    // Soft glow behind along the open curve
    graphics.setColour(Colour::fromRGB(0, 255, 255).withAlpha(this->spectrumStyle.glowAlpha));
    graphics.strokePath(linePath, PathStrokeType(this->spectrumStyle.glowStrokeWidth, PathStrokeType::curved, PathStrokeType::rounded));

    // Gradient fill under curve
    graphics.setGradientFill(this->fillStyle.buildFillGradient(bounds));
    graphics.fillPath(fillPath);

    // Crisp outline along the open curve only
    graphics.setColour(Colours::white.withAlpha(this->spectrumStyle.outlineAlpha));
    graphics.strokePath(linePath, PathStrokeType(this->spectrumStyle.outlineStrokeWidth, PathStrokeType::curved, PathStrokeType::rounded));

    // Peak-hold markers within the plot area
    this->drawPeakMarkers(graphics, plotBounds);

    // Vignette overlay for a polished look
    this->drawVignetteOverlay(graphics, bounds);
}

// ===== Helpers =====
float GraphicalSpectrumAnalyzer::mapLogFrequencyToX(float hz, float xLeft, float xRight) const noexcept
{
    const float fMin = jmax(1.0f, this->frequencyRange.clampedMin());
    const float fMax = jmax(fMin * 1.01f, this->frequencyRange.clampedMax());
    const float clampedFrequency = jlimit(fMin, fMax, hz);
    const float normalisedPosition = (std::log(clampedFrequency) - std::log(fMin)) / (std::log(fMax) - std::log(fMin));
    return xLeft + normalisedPosition * (xRight - xLeft);
}

float GraphicalSpectrumAnalyzer::mapSegmentedFrequencyToX(float hz, float xLeft, float xRight) const noexcept
{
    using namespace juce;
    const float width = xRight - xLeft;
    const float minHz = jmax(1.0f, this->frequencyRange.clampedMin());
    const float maxHz = jmax(minHz * 1.01f, this->frequencyRange.clampedMax());

    const float rightEdgeFraction = jmax(1e-6f, this->segmentedLayout.cumulativeFraction(maxHz));
    const float currentFraction = this->segmentedLayout.cumulativeFraction(hz);
    const float normalized = jlimit(0.0f, 1.0f, currentFraction / rightEdgeFraction);
    return xLeft + normalized * width;
}

String GraphicalSpectrumAnalyzer::formatFrequencyLabel(float frequency) noexcept
{
    if (frequency >= 1000.0f)
    {
        const float kFrequency = frequency / 1000.0f;
        return String(kFrequency, frequency >= 10000.0f ? 0 : 1) + "k";
    }
    return String(static_cast<int>(frequency));
}

void GraphicalSpectrumAnalyzer::drawGrid(Graphics& graphics, Rectangle<float> bounds) const
{
    using namespace juce;
    const float leftX = bounds.getX();
    const float rightX = bounds.getRight();
    const float topY = bounds.getY();
    const float bottomY = bounds.getBottom();
    const float plotHeight = bounds.getHeight();

    // Horizontal bands
    graphics.setColour(Colours::white.withAlpha(this->gridStyle.gridLineAlpha));
    for (int divisionIndex = 1; divisionIndex < this->gridStyle.horizontalDivisions; ++divisionIndex)
    {
        const float y = bottomY - plotHeight * static_cast<float>(divisionIndex) / static_cast<float>(this->gridStyle.horizontalDivisions);
        graphics.drawLine(leftX, y, rightX, y, 1.0f);
    }

    // Vertical frequency ticks using logarithmic 1-9 per-decade pattern
    // Major ticks at 1, 2, 5; minor ticks at the rest
    const float minFrequencyHz = std::max(1.0f, this->frequencyRange.clampedMin());
    const float maxFrequencyHz = std::max(minFrequencyHz * 1.01f, this->frequencyRange.clampedMax());
    const int minDecade = static_cast<int>(std::floor(std::log10(minFrequencyHz)));
    const int maxDecade = static_cast<int>(std::ceil(std::log10(maxFrequencyHz)));

    auto drawTick = [&](float frequencyHz, bool isMajorTick)
    {
        const float x = this->mapSegmentedFrequencyToX(frequencyHz, leftX, rightX);
        const float width = isMajorTick ? this->gridStyle.majorTickWidth : this->gridStyle.minorTickWidth;
        const float alpha = isMajorTick ? this->gridStyle.majorTickAlpha : this->gridStyle.minorTickAlpha;
        graphics.setColour(Colours::white.withAlpha(alpha));
        graphics.drawLine(x, topY, x, bottomY, width);
    };

    for (int decadeIndex = minDecade; decadeIndex <= maxDecade; ++decadeIndex)
    {
        const float decadeBase = std::pow(10.0f, static_cast<float>(decadeIndex));
        for (int mantissa = 1; mantissa <= 9; ++mantissa)
        {
            const float frequencyHz = decadeBase * static_cast<float>(mantissa);
            if (frequencyHz < minFrequencyHz || frequencyHz > maxFrequencyHz)
            {
                continue;
            }
            const bool isMajor = mantissa == 1 || mantissa == 2 || mantissa == 5;
            drawTick(frequencyHz, isMajor);
        }
    }

    // Labels for majors only (2/5/10 pattern yields 20,50,100,200,500,1k,2k,5k,10k,20k...)
    graphics.setColour(Colours::white.withAlpha(this->gridStyle.labelTextAlpha));
#if JUCE_MODULE_AVAILABLE_juce_graphics
#if JUCE_MAJOR_VERSION >= 7
        graphics.setFont(Font(FontOptions(this->gridStyle.labelFontSize)));
#else
        graphics.setFont(Font(this->gridStyle.labelFontSize));
#endif
#else
        graphics.setFont(Font(this->gridStyle.labelFontSize));
#endif
    const int labelY = static_cast<int>(bottomY - this->gridStyle.labelYInset);
    int lastRight = static_cast<int>(std::floor(leftX)) - 100000; // sentinel far to the left
    const int minGap = this->gridStyle.labelMinGap; // pixels
    for (int decadeIndex = minDecade; decadeIndex <= maxDecade; ++decadeIndex)
    {
        const float decadeBase = std::pow(10.0f, static_cast<float>(decadeIndex));
        for (int mantissa : {1, 2, 5})
        {
            const float frequencyHz = decadeBase * static_cast<float>(mantissa);
            if (frequencyHz < minFrequencyHz || frequencyHz > maxFrequencyHz)
            {
                continue;
            }
            const float x = this->mapSegmentedFrequencyToX(frequencyHz, leftX, rightX);
            const String text = formatFrequencyLabel(frequencyHz);
            int labelX = static_cast<int>(x) - (this->gridStyle.labelWidth / 2);
            // Clamp to bounds horizontally
            if (labelX < static_cast<int>(std::floor(leftX)))
            {
                labelX = static_cast<int>(std::floor(leftX));
            }
            if (labelX + this->gridStyle.labelWidth > static_cast<int>(std::ceil(rightX)))
            {
                labelX = static_cast<int>(std::ceil(rightX)) - this->gridStyle.labelWidth;
            }

            const Rectangle labelRect { labelX, labelY, this->gridStyle.labelWidth, this->gridStyle.labelHeight };
            if (labelRect.getX() > lastRight + minGap)
            {
                graphics.drawFittedText(text, labelRect, Justification::centred, 1);
                lastRight = labelRect.getRight();
            }
        }
    }
}

void GraphicalSpectrumAnalyzer::drawVignetteOverlay(Graphics& graphics, Rectangle<float> bounds) const
{
    using namespace juce;
    Rectangle<float> overlay = bounds;
    Colour topFade = Colours::black.withAlpha(this->vignetteStyle.topFadeAlpha);
    Colour sideFade = Colours::black.withAlpha(this->vignetteStyle.sideFadeAlpha);
    graphics.setGradientFill({ topFade, overlay.getTopLeft(), sideFade, overlay.getBottomLeft(), false });
    graphics.fillRect(overlay.withHeight(overlay.getHeight() * this->vignetteStyle.topVignetteHeightPct));
    graphics.setColour(Colours::black.withAlpha(this->vignetteStyle.sideBarsAlpha));
    graphics.fillRect(Rectangle(overlay.getX(), overlay.getY(), this->vignetteStyle.sideVignetteWidth, overlay.getHeight()));
    graphics.fillRect(Rectangle(overlay.getRight() - this->vignetteStyle.sideVignetteWidth, overlay.getY(), this->vignetteStyle.sideVignetteWidth, overlay.getHeight()));
}

void GraphicalSpectrumAnalyzer::resized()
{
    // Nothing to lay out internally for now.
}

void GraphicalSpectrumAnalyzer::applySmoothingAndPeaks()
{
    if (this->magnitudes.empty())
    {
        return;
    }
    // Delegate to service for consistency and reuse.
    UiMagnitudeProcessor::process(this->magnitudes, this->smoothed, this->peaks, this->uiSettings);
}

// ===================== Extracted helpers (implementations) =====================

void GraphicalSpectrumAnalyzer::drawBandBackgrounds(Graphics& graphics, Rectangle<float> bounds) const
{
    using namespace juce;
    const PlotGeometry plot = PlotGeometry::fromRectangle(bounds);

    const float minFrequencyHz = std::max(1.0f, this->frequencyRange.clampedMin());
    const float maxFrequencyHz = std::max(minFrequencyHz * 1.01f, this->frequencyRange.clampedMax());

    const float xMin = this->mapSegmentedFrequencyToX(minFrequencyHz, plot.leftX, plot.rightX);
    const float lowBandEndHz = static_cast<float>(BandFrequencies::LowBandEndHz);
    const float midBandEndHz = static_cast<float>(BandFrequencies::MidBandEndHz);
    const float xLow = this->mapSegmentedFrequencyToX(std::min(lowBandEndHz, maxFrequencyHz), plot.leftX, plot.rightX);
    const float xMid = this->mapSegmentedFrequencyToX(std::min(midBandEndHz, maxFrequencyHz), plot.leftX, plot.rightX);
    const float xMax = this->mapSegmentedFrequencyToX(maxFrequencyHz, plot.leftX, plot.rightX);

    BandTints tints;
    graphics.setColour(tints.low);
    graphics.fillRect(Rectangle(std::min (xMin, xLow), plot.topY, std::abs (xLow - xMin), plot.height));

    if (xMid > xLow)
    {
        graphics.setColour(tints.mid);
        graphics.fillRect(Rectangle(xLow, plot.topY, xMid - xLow, plot.height));
    }

    if (xMax > xMid)
    {
        graphics.setColour(tints.high);
        graphics.fillRect(Rectangle(xMid, plot.topY, xMax - xMid, plot.height));
    }
}

void GraphicalSpectrumAnalyzer::buildSpectrumPaths(const Rectangle<float>& bounds,
                                                    Path& outLinePath,
                                                    Path& outFillPath) const
{
    const float leftX = bounds.getX();
    const float rightX = bounds.getRight();
    const float bottomY = bounds.getBottom();
    const float plotHeight = bounds.getHeight();

    const int binCount = static_cast<int>(std::max<std::size_t>(1, this->smoothed.size()));
    if (binCount <= 0)
    {
        return;
    }

    bool isFirstPoint = true;
    float lastDataX = leftX;

    for (int binIndex = 0; binIndex < binCount; ++binIndex)
    {
        const float magnitudeNormalized = jlimit(0.0f, 1.0f, this->smoothed[static_cast<size_t>(binIndex)]);
        float visualValue = jlimit(0.0f, 1.0f, magnitudeNormalized * this->visualTuning.visualGain);
        visualValue = std::pow(visualValue, this->visualTuning.visualGamma);

        const float x = computeBinXPosition (binIndex, binCount, leftX, rightX);
        const float y = bottomY - visualValue * plotHeight;

        if (isFirstPoint)
        {
            isFirstPoint = false;
            outLinePath.startNewSubPath(x, y);
            outFillPath.startNewSubPath(leftX, bottomY);
            outFillPath.lineTo(x, y);
        }
        else
        {
            outLinePath.lineTo(x, y);
            outFillPath.lineTo(x, y);
        }

        lastDataX = x;
    }

    // Close only the fill path back to the baseline directly under the last data point
    outFillPath.lineTo(lastDataX, bottomY);
    outFillPath.closeSubPath();
}

void GraphicalSpectrumAnalyzer::drawPeakMarkers(Graphics& graphics, Rectangle<float> bounds) const
{
    if (!this->uiSettings.peakHoldEnabled || this->peaks.empty())
    {
        return;
    }

    const float leftX = bounds.getX();
    const float rightX = bounds.getRight();
    const float bottomY = bounds.getBottom();
    const float plotHeight = bounds.getHeight();

    const int binCount = static_cast<int>(this->peaks.size());

    graphics.setColour(Colours::yellow.withAlpha(this->peakStyle.alpha));
    for (int binIndex = 0; binIndex < binCount; binIndex += this->peakStyle.step)
    {
        const float x = computeBinXPosition (binIndex, binCount, leftX, rightX);
        size_t bin_index = static_cast<size_t>(binIndex);
        const float y = bottomY - jlimit(0.0f, 1.0f, this->peaks[bin_index]) * plotHeight;
        Rectangle<float> rectangle(x - this->peakStyle.markerHalfWidth(), y - this->peakStyle.markerYOffset, this->peakStyle.markerWidth, this->peakStyle.markerHeight);
        graphics.fillRect(rectangle);
    }
}

float GraphicalSpectrumAnalyzer::computeBinXPosition(int binIndex, int binCount, float xLeft, float xRight) noexcept
{
    if (binCount <= 1)
    {
        return xLeft;
    }
    float bin_index = static_cast<float>(binIndex);
    float bin_count = static_cast<float>(binCount - 1);
    const float proportion = bin_index / bin_count;
    return xLeft + proportion * (xRight - xLeft);
}
