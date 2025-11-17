#include <gtest/gtest.h>
#include "../source/TrinityProcessor.h"
#include "../source/services/UiMagnitudeProcessor.h"
#include "../source/services/SpectrumProcessing.h"

TEST(TrinityBasic, CanConstructProcessor) {
    TrinityAudioProcessor processor;
    EXPECT_STREQ(processor.getName().toRawUTF8(), "Trinity");
    EXPECT_FALSE(processor.acceptsMidi());
    EXPECT_FALSE(processor.producesMidi());
}

TEST(UiMagnitudeProcessorTest, SmoothingAndPeaksBasic) {
    std::vector<float> magnitudes { 0.0f, 0.5f, 1.0f };
    std::vector<float> smoothed;
    std::vector<float> peaks;
    UiDynamicsSettings settings;
    settings.smoothingEnabled = true;
    settings.attackCoeff = 1.0f;
    settings.releaseCoeff = 1.0f;
    settings.peakHoldEnabled = true;
    settings.peakHoldDecay = 1.0f;

    UiMagnitudeProcessor::process(magnitudes, smoothed, peaks, settings);
    ASSERT_EQ(smoothed.size(), magnitudes.size());
    ASSERT_EQ(peaks.size(), magnitudes.size());
    for (size_t vectorIndex = 0; vectorIndex < magnitudes.size(); ++vectorIndex) {
        EXPECT_FLOAT_EQ(smoothed[vectorIndex], magnitudes[vectorIndex]);
        EXPECT_FLOAT_EQ(peaks[vectorIndex], magnitudes[vectorIndex]);
    }
}

TEST(SpectrumProcessingTest, AllowedEndAndZeroing) {
    const double sampleRate = 48000.0;
    const int fftSize = 2048;
    const int hiGuardBins = 8;
    const int allowedEnd = SpectrumProcessing::computeAllowedEndBin(sampleRate, fftSize, hiGuardBins);
    // Expected: cap by 20k dominates for these values: floor(20000/(48000/2048))-1 = 852
    EXPECT_EQ(allowedEnd, 852);

    std::vector<float> buffer(10, 1.0f);
    SpectrumProcessing::zeroStrictlyAbove(buffer, 7);
    EXPECT_FLOAT_EQ(buffer[7], 1.0f);
    EXPECT_FLOAT_EQ(buffer[8], 0.0f);
    EXPECT_FLOAT_EQ(buffer[9], 0.0f);
}

TEST(SpectrumProcessingTest, TriangularSmoothingAndTaper) {
    std::vector<float> sourceBins { 0,1,2,3,4,5,6,7,8,9 };
    std::vector<float> destinationBins;
    SpectrumProcessing::frequencySmoothTriangularIfEnabled(sourceBins, destinationBins, /*allowedEnd*/9, /*enabled*/true);
    ASSERT_EQ(destinationBins.size(), sourceBins.size());

    EXPECT_NEAR(destinationBins[3], 3.0f, 1e-6f);
    EXPECT_NEAR(destinationBins[4], 4.0f, 1e-6f);

    std::vector<float> taperBuffer(10, 1.0f);
    SpectrumProcessing::applyCosineTaper(taperBuffer, /*allowedEnd*/9, /*taperPercent*/0.2f);
    EXPECT_FLOAT_EQ(taperBuffer[8], 1.0f);
    EXPECT_NEAR(taperBuffer[9], 0.0f, 1e-6f);
}
