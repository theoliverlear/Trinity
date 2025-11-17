#pragma once

#include <vector>
#include <algorithm>
#include <cmath>
#include <JuceHeader.h>

struct SpectrumProcessing
{
    static int computeAllowedEndBin(double sampleRate,
                                    int fftSize,
                                    int hiGuardBins) noexcept
    {
        const int numBins = fftSize / 2;
        const double binHz = sampleRate / static_cast<double>(fftSize);
        const auto clampInt = [numBins](int v){ return std::max(0, std::min(numBins - 1, v)); };
        const int allowedEndByGuard = clampInt(numBins - hiGuardBins - 1);
        const int capBy20k = clampInt(static_cast<int>(std::floor(20000.0 / binHz)) - 1);
        const int allowedEnd = clampInt(std::min(allowedEndByGuard, capBy20k));
        return allowedEnd;
    }
    static void zeroStrictlyAbove(std::vector<float>& buffer, int allowedEnd) noexcept
    {
        const int numBins = static_cast<int>(buffer.size());
        const int startIndex = std::min(std::max(allowedEnd + 1, 0), std::max(0, numBins));
        for (int binIndex = startIndex; binIndex < numBins; ++binIndex)
        {
            buffer[static_cast<size_t>(binIndex)] = 0.0f;
        }
    }
    static void frequencySmoothTriangularIfEnabled(const std::vector<float>& src,
                                                   std::vector<float>& dst,
                                                   int allowedEnd,
                                                   bool enabled) noexcept
    {
        // Use descriptive aliases for clarity without changing the public API
        const auto& sourceBins = src;
        auto& destinationBins = dst;

        const int numBins = static_cast<int>(sourceBins.size());
        if (static_cast<int>(destinationBins.size()) != numBins)
        {
            destinationBins.assign(static_cast<size_t>(numBins), 0.0f);
        }
        if (!enabled || numBins < 5)
        {
            std::copy(sourceBins.begin(), sourceBins.end(), destinationBins.begin());
            return;
        }
        const int lastValid = std::max(2, std::min(allowedEnd, numBins - 1));
        for (int binIndex = 0; binIndex < numBins; ++binIndex)
        {
            if (binIndex < 2 || binIndex >= lastValid - 2)
            {
                destinationBins[static_cast<size_t>(binIndex)] = sourceBins[static_cast<size_t>(binIndex)];
                continue;
            }
            int radius = 2;
            if (binIndex + radius >= lastValid)
            {
                radius = std::max(0, lastValid - 1 - binIndex);
            }
            if (radius < 2)
            {
                const float acc = sourceBins[static_cast<size_t>(binIndex - 1)] * 1.0f
                                 + sourceBins[static_cast<size_t>(binIndex)] * 2.0f
                                 + sourceBins[static_cast<size_t>(binIndex + 1)] * 1.0f;
                const float norm = 1.0f + 2.0f + 1.0f;
                destinationBins[static_cast<size_t>(binIndex)] = acc / norm;
            }
            else
            {
                const float weightFar = 1.0f, weightNear = 2.0f, weightCenter = 3.0f;
                const float acc = weightFar * sourceBins[static_cast<size_t>(binIndex - 2)]
                               + weightNear * sourceBins[static_cast<size_t>(binIndex - 1)]
                               + weightCenter * sourceBins[static_cast<size_t>(binIndex)]
                               + weightNear * sourceBins[static_cast<size_t>(binIndex + 1)]
                               + weightFar * sourceBins[static_cast<size_t>(binIndex + 2)];
                const float norm = weightFar + weightNear + weightCenter + weightNear + weightFar; // 9
                destinationBins[static_cast<size_t>(binIndex)] = acc / norm;
            }
        }
    }

    static void applyCosineTaper(std::vector<float>& buffer,
                                 int allowedEnd,
                                 float taperPercent) noexcept
    {
        const int numBins = static_cast<int>(buffer.size());
        const float clampedPercent = std::max(0.0f, std::min(0.2f, taperPercent));
        double taperBinsReal = static_cast<double>(numBins) * static_cast<double>(clampedPercent);
        const int taperBins = std::max(1, static_cast<int>(std::floor(taperBinsReal)));
        const int taperStart = std::max(0, std::min(allowedEnd, allowedEnd - taperBins + 1));
        for (int bin = taperStart; bin <= allowedEnd && bin < numBins; ++bin)
        {
            const float taperPosition = taperBins <= 1 ? 1.0f : static_cast<float>(bin - taperStart) / static_cast<float>(taperBins - 1);
            const float weight = 0.5f * (1.0f + std::cos(3.14159265358979323846f * taperPosition));
            const float clampedWeight = weight < 0.0f ? 0.0f : (weight > 1.0f ? 1.0f : weight);
            buffer[static_cast<size_t>(bin)] *= clampedWeight;
        }
    }

    static void aggregateBandsFractional(const std::vector<float>& srcPower,
                                         int allowedEnd,
                                         double binHz,
                                         const std::vector<double>& bandF0Hz,
                                         const std::vector<double>& bandF1Hz,
                                         float minDb,
                                         float maxDb,
                                         std::vector<float>& outBands,
                                         std::vector<float>& outBandsPreSmooth) noexcept
    {
        const int numBins = static_cast<int>(srcPower.size());
        const int numBands = static_cast<int>(bandF0Hz.size());
        if (static_cast<int>(outBands.size()) != numBands)
        {
            outBands.assign(static_cast<size_t>(numBands), 0.0f);
        }
        else
        {
            std::fill(outBands.begin(), outBands.end(), 0.0f);
        }
        if (static_cast<int>(outBandsPreSmooth.size()) != numBands)
        {
            outBandsPreSmooth.assign(static_cast<size_t>(numBands), 0.0f);
        }
        else
        {
            std::fill(outBandsPreSmooth.begin(), outBandsPreSmooth.end(), 0.0f);
        }

        const int allowedEndForBands = std::max(1, std::min(numBins - 2, allowedEnd));
        const double allowedEndHzLocal = static_cast<double>(allowedEnd + 1) * binHz;

        for (int bandIndex = 0; bandIndex < numBands; ++bandIndex)
        {
            double bandStartHz = bandF0Hz.size() == static_cast<size_t>(numBands) ? bandF0Hz[static_cast<size_t>(bandIndex)] : 20.0;
            double bandEndHz = bandF1Hz.size() == static_cast<size_t>(numBands) ? bandF1Hz[static_cast<size_t>(bandIndex)] : allowedEndHzLocal;
            if (bandEndHz <= bandStartHz)
            {
                bandEndHz = bandStartHz + binHz;
            }

            if (bandEndHz <= bandStartHz + 1e-12 || bandStartHz >= allowedEndHzLocal)
            {
                outBands[static_cast<size_t>(bandIndex)] = 0.0f;
                outBandsPreSmooth[static_cast<size_t>(bandIndex)] = 0.0f;
                continue;
            }

            int binStartIndex = std::max(1, std::min(allowedEndForBands, static_cast<int>(std::floor(bandStartHz / binHz))));
            int binEndIndex = std::max(1, std::min(allowedEndForBands, static_cast<int>(std::floor(bandEndHz / binHz))));
            if (binEndIndex < binStartIndex)
            {
                binEndIndex = binStartIndex;
            }

            double sumPower = 0.0;
            double weightSum = 0.0;
            for (int binIndex = binStartIndex; binIndex <= binEndIndex; ++binIndex)
            {
                const double binStartHzEdge = static_cast<double>(binIndex) * binHz;
                const double binEndHzEdge = static_cast<double>(binIndex + 1) * binHz;
                const double overlapStart = std::max(bandStartHz, binStartHzEdge);
                const double overlapEnd = std::min(bandEndHz, binEndHzEdge);
                const double overlapWidth = std::max(0.0, overlapEnd - overlapStart);
                if (overlapWidth > 0.0)
                {
                    sumPower  += static_cast<double>(srcPower[static_cast<size_t>(binIndex)]) * overlapWidth;
                    weightSum += overlapWidth;
                }
            }

            if (weightSum <= 0.0)
            {
                outBands[static_cast<size_t>(bandIndex)] = 0.0f;
                outBandsPreSmooth[static_cast<size_t>(bandIndex)] = 0.0f;
                continue;
            }

            const double meanPower = sumPower / weightSum;
            const float meanMag = static_cast<float>(std::sqrt(std::max(0.0, meanPower))) + 1e-20f;
            float db = Decibels::gainToDecibels(meanMag, minDb);
            db = db < minDb ? minDb : (db > maxDb ? maxDb : db);
            float normalised = jmap(db, minDb, maxDb, 0.0f, 1.0f);
            normalised = std::min(normalised * 0.92f, 0.98f);
            const float normClamped = normalised < 0.0f ? 0.0f : (normalised > 1.0f ? 1.0f : normalised);
            outBands[static_cast<size_t>(bandIndex)] = normClamped;
            outBandsPreSmooth[static_cast<size_t>(bandIndex)] = normClamped;
        }
    }

    // Band-domain smoothing: median3 for top 15% bands, 3-point weighted average elsewhere.
    static void smoothBandsInPlace(std::vector<float>& bands,
                                   bool enabled) noexcept
    {
        if (!enabled) { return; }
        const int bandCount = static_cast<int>(bands.size());
        if (bandCount < 3) { return; }

        auto median3 = [] (float bandLow, float bandMid, float bandHigh) -> float
        {
            float minValue = bandLow; if (bandMid < minValue) minValue = bandMid; if (bandHigh < minValue) minValue = bandHigh;
            float maxValue = bandLow; if (bandMid > maxValue) maxValue = bandMid; if (bandHigh > maxValue) maxValue = bandHigh;
            return (bandLow + bandMid + bandHigh) - minValue - maxValue;
        };

        std::vector tempBands(static_cast<size_t>(bandCount), 0.0f);
        const int topRegionStartIndex = static_cast<int>(std::floor(bandCount * 0.85));
        tempBands[0] = bands[0];
        for (int bandIndex = 1; bandIndex < bandCount - 1; ++bandIndex)
        {
            if (bandIndex >= topRegionStartIndex)
            {
                tempBands[static_cast<size_t>(bandIndex)] = median3(bands[static_cast<size_t>(bandIndex - 1)], bands[static_cast<size_t>(bandIndex)], bands[static_cast<size_t>(bandIndex + 1)]);
            }
            else
            {
                tempBands[static_cast<size_t>(bandIndex)] = (bands[static_cast<size_t>(bandIndex - 1)] + 2.0f * bands[static_cast<size_t>(bandIndex)] + bands[static_cast<size_t>(bandIndex + 1)]) * 0.25f;
            }
        }
        tempBands[static_cast<size_t>(bandCount - 1)] = (bands[static_cast<size_t>(bandCount - 2)] + bands[static_cast<size_t>(bandCount - 1)]) * 0.5f;
        // Copy back
        std::copy(tempBands.begin(), tempBands.end(), bands.begin());
    }
};
