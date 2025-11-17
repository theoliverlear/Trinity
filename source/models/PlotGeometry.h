#pragma once

#include <JuceHeader.h>

// Simple geometry helper extracted from common patterns like
// leftX/rightX/topY/bottomY/height computed from a Rectangle<float>.
struct PlotGeometry
{
    float leftX { 0.0f };
    float rightX { 0.0f };
    float topY { 0.0f };
    float bottomY { 0.0f };
    float width { 0.0f };
    float height { 0.0f };

    static PlotGeometry fromRectangle(const Rectangle<float>& rectangle) noexcept
    {
        PlotGeometry plotGeometry;
        plotGeometry.leftX = rectangle.getX();
        plotGeometry.rightX = rectangle.getRight();
        plotGeometry.topY = rectangle.getY();
        plotGeometry.bottomY = rectangle.getBottom();
        plotGeometry.width = rectangle.getWidth();
        plotGeometry.height = rectangle.getHeight();
        return plotGeometry;
    }
};
