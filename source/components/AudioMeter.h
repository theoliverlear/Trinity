#pragma once

#include <JuceHeader.h>
#include "../models/MeterVisualStyle.h"
#include "../models/MeterDbScaleSpec.h"
class AudioMeter : public Component
{
public:
    AudioMeter() = default;
    ~AudioMeter() override = default;

    void setLevelPointer (float* ptr)
    {
        this->levelPtr = ptr;
    }

    void setLabel (const String& text)
    {
        this->label = text;
        this->repaint();
    }

    void setShowTicks (bool show)
    {
        this->showTicks = show;
        this->repaint();
    }

    void setDbRange (float minDbValue, float maxDbValue)
    {
        this->minDb = minDbValue;
        this->maxDb = maxDbValue;
        this->repaint();
    }

    // Reserve pixels on the left for tick marks and labels (first meter typically).
    void setLeftGutterWidth (float px)
    {
        this->leftGutterWidth = jmax(0.0f, px);
        this->repaint();
    }

    // Advance one UI frame: update peak-hold / clip LED from current level.
    void advanceFrame()
    {
        const float currentLevel = jlimit (0.0f, 1.0f, this->levelPtr ? *this->levelPtr : 0.0f);
        const float db = Decibels::gainToDecibels(currentLevel, this->minDb);
        float norm = jmap(db, this->minDb, this->maxDb, 0.0f, 1.0f);
        norm = jlimit(0.0f, 1.0f, norm);

        this->peakHoldLevel *= this->peakHoldDecay;
        if (norm > this->peakHoldLevel)
        {
            this->peakHoldLevel = norm;
        }

        if (db >= this->maxDb - 0.1f)
        {
            this->clipHoldFrames = this->clipHoldDurationFrames;
        }
        else if (this->clipHoldFrames > 0)
        {
            --this->clipHoldFrames;
        }

        this->repaint();
    }

    void paint (Graphics& graphics) override;

private:
    // ===== Helper methods to keep paint() focused on orchestration =====
    static Rectangle<float> computeColumnBounds(const Rectangle<float>& componentArea,
                                                      float leftGutterWidth,
                                                      bool showTicks) noexcept
    {
        const float tickGutter = showTicks ? leftGutterWidth : 0.0f;
        return Rectangle(componentArea.getX() + tickGutter,
                                      componentArea.getY(),
                                      componentArea.getWidth() - tickGutter,
                                      componentArea.getHeight());
    }

    static Rectangle<float> computeInnerRect(const Rectangle<float>& columnBounds,
                                                   float innerPadding) noexcept
    {
        const float innerLeft = columnBounds.getX() + innerPadding;
        const float innerRight = columnBounds.getRight() - innerPadding;
        const float innerTop = columnBounds.getY() + innerPadding + 32.0f; // space for LED/label
        const float innerBottom = columnBounds.getBottom() - innerPadding - 8.0f;
        const float innerHeight = jmax(1.0f, innerBottom - innerTop);
        return Rectangle(innerLeft, innerTop, innerRight - innerLeft, innerHeight);
    }

    static void drawBackground(Graphics& graphics,
                               const Rectangle<float>& columnBounds,
                               const MeterVisualStyle& style,
                               float cornerRadius)
    {
        Path backgroundPath; backgroundPath.addRoundedRectangle(columnBounds, cornerRadius);
        auto backgroundGradient = style.buildBackgroundGradient(columnBounds);
        graphics.setGradientFill(backgroundGradient);
        graphics.fillPath(backgroundPath);
        graphics.setColour(Colours::black.withAlpha(0.35f));
        graphics.strokePath(backgroundPath, PathStrokeType(1.0f));
    }

    static float computeLevelNormalized(float currentLevel, float minDb, float maxDb) noexcept
    {
        const float db = Decibels::gainToDecibels(currentLevel, minDb);
        float normalizedValue = jmap(db, minDb, maxDb, 0.0f, 1.0f);
        const float norm = jlimit(0.0f, 1.0f, normalizedValue);
        return norm;
    }

    static void drawFilledBar(Graphics& graphics,
                              const Rectangle<float>& innerRect,
                              float levelNormalised,
                              float cornerRadius,
                              const MeterVisualStyle& style)
    {
        const float innerBottom = innerRect.getBottom();
        const float innerHeight = innerRect.getHeight();
        const float filledHeight = innerHeight * levelNormalised;
        const float filledTop = innerBottom - filledHeight;
        Rectangle filledRect(innerRect.getX(), filledTop, innerRect.getWidth(), filledHeight);
        if (filledRect.getHeight() > 0.5f)
        {
            auto fillGradient = style.buildFillGradient(innerRect);
            graphics.setGradientFill(fillGradient);
            Path fillPath; fillPath.addRoundedRectangle(filledRect, cornerRadius * 0.6f);
            graphics.fillPath(fillPath);

            const float glossHeight = jmin(10.0f, filledRect.getHeight() * 0.25f);
            if (glossHeight > 1.0f)
            {
                Rectangle gloss(filledRect.getX() + 2.0f, filledRect.getY() + 2.0f,
                                             filledRect.getWidth() - 4.0f, glossHeight);
                graphics.setColour(Colours::white.withAlpha(0.08f));
                graphics.fillRoundedRectangle(gloss, cornerRadius * 0.4f);
            }
        }
    }

    static void drawPeakHoldMarker(Graphics& graphics,
                                   const Rectangle<float>& innerRect,
                                   float peakHoldLevel)
    {
        const float innerBottom = innerRect.getBottom();
        const float innerLeft = innerRect.getX();
        const float innerRight = innerRect.getRight();
        const float innerHeight = innerRect.getHeight();
        const float markerY = innerBottom - peakHoldLevel * innerHeight;
        const float markerLeft = innerLeft + 2.0f;
        const float markerRight = innerRight - 2.0f;
        graphics.setColour(Colours::yellow.withAlpha(0.7f));
        graphics.drawLine(markerLeft, markerY, markerRight, markerY, 2.0f);
    }

    static void drawClipLed(Graphics& graphics,
                            const Rectangle<float>& columnBounds,
                            bool clipOn,
                            float ledSize)
    {
        const float ledX = columnBounds.getCentreX() - ledSize * 0.5f;
        const float ledY = columnBounds.getY() + 6.0f;
        Rectangle ledRect(ledX, ledY, ledSize, ledSize);
        graphics.setColour(Colours::red.withAlpha(clipOn ? 0.9f : 0.25f));
        graphics.fillEllipse(ledRect);
        graphics.setColour(Colours::black.withAlpha(0.6f));
        graphics.drawEllipse(ledRect, 1.0f);
    }

    static void drawDbTicksAndLabels(Graphics& graphics,
                                     const Rectangle<float>& componentArea,
                                     const Rectangle<float>& innerRect,
                                     float minDb,
                                     float maxDb,
                                     float leftGutterWidth)
    {
        const float innerBottom = innerRect.getBottom();
        const float innerHeight = innerRect.getHeight();
        const float innerTickRight = componentArea.getX() + leftGutterWidth - 4.0f;
        const float tickLeftShort = innerTickRight - 6.0f;
        const float tickLeftLong = innerTickRight - 12.0f;

        graphics.setColour(Colours::white.withAlpha(0.15f));
        const float* tickValuesDb = MeterDbScaleSpec::ticks();
        const int tickCount = MeterDbScaleSpec::tickCount();
        for (int tickIndex = 0; tickIndex < tickCount; ++tickIndex)
        {
            const float tickDbValue = tickValuesDb[tickIndex];
            float tickedValue = jmap(tickDbValue, minDb, maxDb, 0.0f, 1.0f);
            const float norm = jlimit(0.0f, 1.0f, tickedValue);
            const float y = innerBottom - norm * innerHeight;
            const bool drawLabel = MeterDbScaleSpec::isLabeledTick(tickDbValue);
            const float x0 = drawLabel ? tickLeftLong : tickLeftShort;
            graphics.drawLine(x0, y, innerTickRight, y, tickDbValue == 0.0f ? 1.8f : 1.0f);
            if (drawLabel)
            {
                String tickLabel = (tickDbValue >= 0.0f ? "0" : String(static_cast<int>(tickDbValue)));
                tickLabel << " dB";
                graphics.setColour(Colours::white.withAlpha(0.45f));
                graphics.drawFittedText(tickLabel,
                                 Rectangle(static_cast<int>(componentArea.getX()), static_cast<int>(y - 8),
                                                      static_cast<int>(tickLeftLong - componentArea.getX() - 6.0f), 16),
                                 Justification::centredRight, 1);
                graphics.setColour(Colours::white.withAlpha(0.15f));
            }
        }
    }

    static void drawTopLabels(Graphics& graphics,
                              const Rectangle<float>& columnBounds,
                              const String& label,
                              float currentLevel,
                              float minDb)
    {
        graphics.setColour(Colours::white);
        Rectangle labelArea(static_cast<int>(columnBounds.getX()), static_cast<int>(columnBounds.getY()), static_cast<int>(columnBounds.getWidth()), 20);
        graphics.drawFittedText(label, labelArea, Justification::centred, 1);

        if (currentLevel > 0.0f)
        {
            const float decibels = Decibels::gainToDecibels(currentLevel, minDb);
            graphics.setColour(Colours::white.withAlpha(0.8f));
            Rectangle dbArea(static_cast<int>(columnBounds.getX()), static_cast<int>(columnBounds.getY()) + 20, static_cast<int>(columnBounds.getWidth()), 20);
            graphics.drawFittedText(String(decibels, 1) + " dB", dbArea, Justification::centred, 1);
        }
    }

    float* levelPtr { nullptr };
    String label { "" };

    // Visual/state
    float peakHoldLevel { 0.0f };
    int clipHoldFrames { 0 };

    // Tuning
    float peakHoldDecay { 0.96f };            // per-frame
    int   clipHoldDurationFrames { 18 };      // ~0.6s @ 30 fps
    float minDb { -120.0f };
    float maxDb { 0.0f };
    bool  showTicks { true };
    float leftGutterWidth { 72.0f }; // space for ticks+labels when enabled

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioMeter)
};
