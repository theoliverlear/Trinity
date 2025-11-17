#pragma once

#include <JuceHeader.h>
#include <array>

#include "models/MeterInfo.h"
#include "components/GraphicalSpectrumAnalyzer.h"
#include "components/AudioSpectrumMeters.h"

class TrinityAudioProcessor;

class TrinityAudioProcessorEditor : public AudioProcessorEditor,
                                     Timer
{
public:
    void initSpectrumAnalyzerControls();
    void initSpectrumAnalyzerButtons();
    explicit TrinityAudioProcessorEditor(TrinityAudioProcessor& processorRef);
    void updateDisplayAndSmoothLevels();
    ~TrinityAudioProcessorEditor() override = default;

    void paint(Graphics& graphics) override;
    void resized() override;

private:
    TrinityAudioProcessor& processor;
    void timerCallback() override;
    float displayTotal { 0.0f };
    float displayLow { 0.0f };
    float displayMid { 0.0f };
    float displayHigh { 0.0f };
    std::array<MeterInfo, 4> meters;
    GraphicalSpectrumAnalyzer spectrumAnalyzer;
    AudioSpectrumMeters audioMeters;

    // Controls for built-in test signal and debug CSV
    ToggleButton btnTestEnabled { "Test Signal" };
    ComboBox    cbxTestType;
    ToggleButton btnDebugCsv { "Debug CSV" };

    // Solo buttons for bands
    ToggleButton btnSoloLow  { "Solo Low" };
    ToggleButton btnSoloMid  { "Solo Mid" };
    ToggleButton btnSoloHigh { "Solo High" };

    // A/B diagnostics controls (Standalone convenience)
    ToggleButton btnUiSmooth { "UI Smooth" };
    ToggleButton btnUiPeaks  { "UI Peaks" };
    ToggleButton btnFreqSmooth { "Freq Smooth" };
    ToggleButton btnBandSmooth { "Band Smooth" };
    Slider sldGuardPercent;       // 0..0.2
    Slider sldTaperPercent;       // 0..0.2
    Slider sldSpecSmoothing;      // 0..1

    // Debug CSV state
    int debugFrameCounter { 0 };
    bool debugHeaderWritten { false };
    std::unique_ptr<FileOutputStream> debugStream;
    File debugFile;

    void writeDebugCsvSnapshot();
    void ensureDebugStreamOpen();
    static String getInitHeader(std::vector<float> tailPreSmooth,
                             std::vector<float> tailPostSmooth,
                             std::vector<float> tailPostTaper,
                             std::vector<float> bandsPreSmooth,
                             std::vector<float> bandsFinal);
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TrinityAudioProcessorEditor)
};
