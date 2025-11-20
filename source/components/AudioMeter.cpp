#include "AudioMeter.h"
#include "../models/MeterVisualStyle.h"
#include "../models/MeterDbScaleSpec.h"

using namespace juce;

namespace
{
    static const MeterVisualStyle& defaultMeterStyle()
    {
        static MeterVisualStyle style;
        return style;
    }
}

void AudioMeter::paint(Graphics& graphics)
{
    const auto componentArea = getLocalBounds().toFloat();
    graphics.fillAll(Colours::transparentBlack);
    const MeterVisualStyle& style = defaultMeterStyle();
    const float cornerRadius = style.cornerRadius;
    const float innerPadding = style.innerPadding;
    const Rectangle<float> columnBounds = computeColumnBounds(componentArea, this->leftGutterWidth, this->showTicks);
    drawBackground(graphics, columnBounds, style, cornerRadius);
    const Rectangle<float> innerRect = computeInnerRect(columnBounds, innerPadding);
    const float currentLevel = jlimit(0.0f, 1.0f, this->levelPtr ? *this->levelPtr : 0.0f);
    const float levelNormalised = computeLevelNormalized(currentLevel, this->minDb, this->maxDb);
    drawFilledBar(graphics, innerRect, levelNormalised, cornerRadius, style);
    drawPeakHoldMarker(graphics, innerRect, this->peakHoldLevel);
    drawClipLed(graphics, columnBounds, this->clipHoldFrames > 0, style.ledSize);
    if (this->showTicks)
    {
        drawDbTicksAndLabels(graphics, componentArea, innerRect,
            this->minDb, this->maxDb, this->leftGutterWidth);
    }
    drawTopLabels(graphics, columnBounds, this->label, currentLevel, this->minDb);
}
