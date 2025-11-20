#include "TrinityEditor.h"
#include "TrinityProcessor.h"
#include <vector>
#include <cmath>
#include "services/LevelColourScale.h"

void TrinityAudioProcessorEditor::initSpectrumAnalyzerControls()
{
    // Add spectrum analyzer
    this->addAndMakeVisible(this->spectrumAnalyzer);

    // Controls: Test Signal + type, Debug CSV
    this->addAndMakeVisible(this->btnTestEnabled);
    this->addAndMakeVisible(this->cbxTestType);
    this->addAndMakeVisible(this->btnDebugCsv);

    // Solo buttons
    this->addAndMakeVisible(this->btnSoloLow);
    this->addAndMakeVisible(this->btnSoloMid);
    this->addAndMakeVisible(this->btnSoloHigh);

    // Diagnostics toggles and sliders
    this->addAndMakeVisible(this->btnUiSmooth);
    this->addAndMakeVisible(this->btnUiPeaks);
    this->addAndMakeVisible(this->btnFreqSmooth);
    this->addAndMakeVisible(this->btnBandSmooth);
    this->addAndMakeVisible(this->sldGuardPercent);
    this->addAndMakeVisible(this->sldTaperPercent);
    this->addAndMakeVisible(this->sldSpecSmoothing);
}

void TrinityAudioProcessorEditor::initSpectrumAnalyzerButtons()
{
    this->cbxTestType.addItem("Sine 17 kHz",  kSine17k);
    this->cbxTestType.addItem("Sine 19 kHz",  kSine19k);
    this->cbxTestType.addItem("White noise",  kWhiteNoise);
    this->cbxTestType.addItem("Pink-ish noise", kPinkNoise);
    this->cbxTestType.addItem("Log sweep",    kLogSweep);
    this->cbxTestType.setSelectedId(kSine17k, dontSendNotification);
}

TrinityAudioProcessorEditor::TrinityAudioProcessorEditor(
    TrinityAudioProcessor& processorRef)
    : AudioProcessorEditor(&processorRef),
      processor(processorRef)
{
    // Increase default editor size for better readability and less cramped UI
    this->setSize(700, 800);
    this->startTimerHz(30);
    this->meters = {
        MeterInfo { &this->displayTotal, "Total" },
        MeterInfo { &this->displayLow,   "Low"   },
        MeterInfo { &this->displayMid,   "Mid"   },
        MeterInfo { &this->displayHigh,  "High"  },
    };

    this->initSpectrumAnalyzerControls();

    this->initSpectrumAnalyzerButtons();

    // Add meters component and connect its four child meters to our level pointers
    this->addAndMakeVisible(this->audioMeters);
    this->audioMeters.setMeters(this->meters);

    this->btnTestEnabled.onClick = [this]
    {
        this->processor.setTestEnabled (this->btnTestEnabled.getToggleState());
        this->processor.setTestType (this->cbxTestType.getSelectedId());
    };
    this->cbxTestType.onChange = [this]
    {
        this->processor.setTestType (this->cbxTestType.getSelectedId());
    };

    this->btnDebugCsv.onClick = [this]
    {
        if (this->btnDebugCsv.getToggleState())
        {
            this->debugHeaderWritten = false;
            this->ensureDebugStreamOpen();
            this->processor.setDebugCaptureEnabled (true);
        }
        else
        {
            this->debugStream.reset();
            this->processor.setDebugCaptureEnabled (false);
        }
    };

    // Solo button wiring (mutually exclusive)
    this->btnSoloLow.onClick = [this]
    {
        const bool shouldSolo = this->btnSoloLow.getToggleState();
        this->btnSoloMid.setToggleState(false, dontSendNotification);
        this->btnSoloHigh.setToggleState(false, dontSendNotification);
        this->processor.setSoloMode(shouldSolo ? SoloMode::Low
                                               : SoloMode::None);
    };

    this->btnSoloMid.onClick = [this]
    {
        const bool shouldSolo = this->btnSoloMid.getToggleState();
        this->btnSoloLow.setToggleState(false, dontSendNotification);
        this->btnSoloHigh.setToggleState(false, dontSendNotification);
        this->processor.setSoloMode(shouldSolo ? SoloMode::Mid
                                               : SoloMode::None);
    };

    this->btnSoloHigh.onClick = [this]
    {
        const bool shouldSolo = this->btnSoloHigh.getToggleState();
        this->btnSoloLow.setToggleState(false, dontSendNotification);
        this->btnSoloMid.setToggleState(false, dontSendNotification);
        this->processor.setSoloMode(shouldSolo ? SoloMode::High
                                               : SoloMode::None);
    };

    // Initial state
    this->btnSoloLow.setToggleState(false, dontSendNotification);
    this->btnSoloMid.setToggleState(false, dontSendNotification);
    this->btnSoloHigh.setToggleState(false, dontSendNotification);
    this->processor.setSoloMode(SoloMode::None);

    // Initialize and wire diagnostics controls
    this->btnUiSmooth.setToggleState(true, dontSendNotification);
    this->btnUiPeaks .setToggleState(true, dontSendNotification);
    this->spectrumAnalyzer.setSmoothingEnabled(true);
    this->spectrumAnalyzer.setPeakHoldEnabled(true);
    this->btnUiSmooth.onClick = [this]
    {
        this->spectrumAnalyzer.setSmoothingEnabled(this->btnUiSmooth.getToggleState());
    };
    this->btnUiPeaks .onClick = [this]
    {
        this->spectrumAnalyzer.setPeakHoldEnabled(this->btnUiPeaks.getToggleState());
    };

    this->btnFreqSmooth.setButtonText("Freq Smooth");
    this->btnBandSmooth.setButtonText("Band Smooth");
    this->btnFreqSmooth.setToggleState(true, dontSendNotification);
    this->btnBandSmooth.setToggleState(true, dontSendNotification);
    this->btnFreqSmooth.onClick = [this]
    {
        this->processor.setFreqSmoothingEnabled(this->btnFreqSmooth.getToggleState());
    };
    this->btnBandSmooth.onClick = [this]
    {
        this->processor.setBandSmoothingEnabled(this->btnBandSmooth.getToggleState());
    };

    this->sldGuardPercent.setRange(0.0, 0.2, 0.001);
    this->sldGuardPercent.setTextValueSuffix(" guard");
    this->sldGuardPercent.setSliderStyle(Slider::LinearHorizontal);
    this->sldGuardPercent.setTextBoxStyle(Slider::TextBoxLeft, false, 60, 18);
    this->sldGuardPercent.setValue (0.06, dontSendNotification);
    this->sldGuardPercent.onValueChange = [this]
    {
        this->processor.setGuardPercent (static_cast<float>(this->sldGuardPercent.getValue()));
    };

    this->sldTaperPercent.setRange(0.0, 0.2, 0.001);
    this->sldTaperPercent.setTextValueSuffix(" taper");
    this->sldTaperPercent.setSliderStyle(Slider::LinearHorizontal);
    this->sldTaperPercent.setTextBoxStyle(Slider::TextBoxLeft, false, 60, 18);
    this->sldTaperPercent.setValue(0.02, dontSendNotification);
    this->sldTaperPercent.onValueChange = [this]
    {
        this->processor.setTaperPercent ((float) this->sldTaperPercent.getValue());
    };

    this->sldSpecSmoothing.setRange (0.0, 1.0, 0.001);
    this->sldSpecSmoothing.setTextValueSuffix (" specSmooth");
    this->sldSpecSmoothing.setSliderStyle (Slider::LinearHorizontal);
    this->sldSpecSmoothing.setTextBoxStyle (Slider::TextBoxLeft, false, 60, 18);
    this->sldSpecSmoothing.setValue (0.2, dontSendNotification);
    this->sldSpecSmoothing.onValueChange = [this]{ this->processor.setSpecSmoothing ((float) this->sldSpecSmoothing.getValue()); };

    // Provide frequency range so the analyzer can draw Hz ticks.
    // Use processor's computed post-guard max frequency so ticks align with displayed data.
    this->spectrumAnalyzer.setFrequencyRange(20.0f, static_cast<float>(this->processor.getDisplayMaxHz()));

    // Ensure layout is applied now and on any future resizes
    this->TrinityAudioProcessorEditor::resized();
}

void TrinityAudioProcessorEditor::updateDisplayAndSmoothLevels()
{
    constexpr float smoothing = 0.10f;

    auto smooth = [smoothing](float current, float target)
    {
        return current * (1.0f - smoothing) + target * smoothing;
    };
    this->displayTotal = smooth(this->displayTotal, this->processor.getTotalLevel());
    this->displayLow = smooth(this->displayLow, this->processor.getLowLevel());
    this->displayMid = smooth(this->displayMid, this->processor.getMidLevel());
    this->displayHigh = smooth(this->displayHigh, this->processor.getHighLevel());
}

void TrinityAudioProcessorEditor::timerCallback()
{
    this->updateDisplayAndSmoothLevels();
    {
        std::vector<float> spectrumMagnitudes;
        this->processor.copySpectrum(spectrumMagnitudes); // values in [0..1]
        if (!spectrumMagnitudes.empty())
        {
            this->spectrumAnalyzer.setMagnitudes(spectrumMagnitudes);
        }
    }

    // Keep analyzer ticks aligned to processor's current display maximum
    this->spectrumAnalyzer.setFrequencyRange(20.0f, static_cast<float>(this->processor.getDisplayMaxHz()));

    // Throttled debug CSV snapshot
    if (this->btnDebugCsv.getToggleState())
    {
        if (++this->debugFrameCounter >= 15) // ~0.5s at 30 Hz
        {
            this->debugFrameCounter = 0;
            this->writeDebugCsvSnapshot();
        }
    }

    // Advance meters UI state (peak-hold/clip)
    this->audioMeters.advanceFrame();
    this->repaint(); // repaint editor backdrop; child meters repaint internally
}

void TrinityAudioProcessorEditor::resized()
{
    auto bounds = this->getLocalBounds();
    auto topArea = bounds.removeFromTop((int) std::round(bounds.getHeight() * 0.40f));
    this->spectrumAnalyzer.setBounds(topArea);

    const int controlsHeight = 132;
    auto controlsArea = bounds.removeFromTop(controlsHeight);
    auto row1 = controlsArea.removeFromTop(controlsHeight / 3);
    auto row2 = controlsArea.removeFromTop(controlsHeight / 3);
    auto row3 = controlsArea; // dedicated solo buttons row

    const int pad = 8;
    const int h1 = row1.getHeight() - pad * 2;
    const int wTest = 130;
    const int wType = 200;
    const int wCsv  = 110;
    const int gap1 = pad * 2; // larger gaps for the top row
    const int totalTopWidth = wTest + wType + wCsv + gap1 * 2;
    const int x1Start = row1.getX() + (row1.getWidth() - totalTopWidth) / 2;
    const int y1 = row1.getY() + (row1.getHeight() - h1) / 2; // vertically center within the row

    int x1 = x1Start;
    this->btnTestEnabled.setBounds(x1, y1, wTest, h1); x1 += wTest + gap1;
    this->cbxTestType.setBounds(x1, y1, wType, h1);    x1 += wType + gap1;
    this->btnDebugCsv.setBounds(x1, y1, wCsv,  h1);

    // Row 2: diagnostics toggles and sliders
    const int h2 = row2.getHeight() - pad * 2;
    int x2 = row2.getX() + pad;
    this->btnUiSmooth.setBounds(x2, row2.getY() + pad, 120, h2); x2 += 120 + pad;
    this->btnUiPeaks.setBounds(x2, row2.getY() + pad, 112, h2); x2 += 112 + pad;
    this->btnFreqSmooth.setBounds(x2, row2.getY() + pad, 140, h2); x2 += 140 + pad;
    this->btnBandSmooth.setBounds(x2, row2.getY() + pad, 140, h2); x2 += 140 + pad;
    const int sliderW = 220;
    this->sldGuardPercent.setBounds(x2, row2.getY() + pad, sliderW, h2); x2 += sliderW + pad;
    this->sldTaperPercent.setBounds(x2, row2.getY() + pad, sliderW, h2); x2 += sliderW + pad;
    this->sldSpecSmoothing.setBounds(x2, row2.getY() + pad, sliderW, h2);

    // Row 3: Solo buttons only (own dedicated row), centered horizontally
    const int h3 = row3.getHeight() - pad * 2;
    const int soloW = 120;
    const int gaps = 2; // between three buttons
    const int totalSoloWidth = soloW * 3 + pad * gaps;
    const int x3Start = row3.getX() + (row3.getWidth() - totalSoloWidth) / 2;
    const int y3 = row3.getY() + (row3.getHeight() - h3) / 2; // vertical centering within row

    int x3 = x3Start;
    this->btnSoloLow.setBounds(x3, y3, soloW, h3); x3 += soloW + pad;
    this->btnSoloMid.setBounds(x3, y3, soloW, h3); x3 += soloW + pad;
    this->btnSoloHigh.setBounds(x3, y3, soloW, h3);

    // Remaining area is for the meters component
    this->audioMeters.setBounds (bounds);
}

void TrinityAudioProcessorEditor::paint(Graphics& graphics)
{
    using namespace juce;
    graphics.fillAll(Colours::black);
}

void TrinityAudioProcessorEditor::ensureDebugStreamOpen()
{
    if (this->debugStream && this->debugStream->openedOk())
    {
        return;
    }

    File docs = File::getSpecialLocation(File::userDocumentsDirectory);
    this->debugFile = docs.getChildFile("Trinity_Debug.csv");
    // createOutputStream returns std::unique_ptr<FileOutputStream>; assign/move it directly
    this->debugStream = this->debugFile.createOutputStream();
    if (this->debugStream && this->debugStream->openedOk())
    {
        this->debugHeaderWritten = false;
    }
}

String TrinityAudioProcessorEditor::getInitHeader(std::vector<float> tailPreSmooth, std::vector<float> tailPostSmooth, std::vector<float> tailPostTaper, std::vector<float> bandsPreSmooth, std::vector<float> bandsFinal)
{
    String header = "csvVersion=2\n";
    header += "frame,hiGuard,allowedEndBin,allowedEndHz,sampleRate,fftSize,displayMaxHz";
    for (size_t i = 0; i < tailPreSmooth.size(); ++i)
    {
        header += ",preSmoothTail" + String(static_cast<int>(i));
    }
    for (size_t i = 0; i < tailPostSmooth.size(); ++i)
    {
        header += ",postSmoothTail" + String(static_cast<int>(i));
    }
    for (size_t i = 0; i < tailPostTaper.size(); ++i)
    {
        header += ",postTaperTail" + String(static_cast<int>(i));
    }
    for (size_t i = 0; i < bandsPreSmooth.size(); ++i)
    {
        header += ",bandPre" + String(static_cast<int>(i));
    }
    for (size_t i = 0; i < bandsFinal.size(); ++i)
    {
        header += ",bandFinal" + String(static_cast<int>(i));
    }
    header += "\n";
    return header;
}

bool TrinityAudioProcessorEditor::isValidDebugStream()
{
    return this->debugStream && this->debugStream->openedOk();
}

void TrinityAudioProcessorEditor::writeDebugCsvSnapshot()
{
    this->ensureDebugStreamOpen();
    if (!this->isValidDebugStream())
    {
        return;
    }

    // New extended debug data
    std::vector<float> tailPreSmooth;
    std::vector<float> tailPostSmooth;
    std::vector<float> tailPostTaper;
    std::vector<float> bandsPreSmooth;
    std::vector<float> bandsFinal;
    int hiGuardBinsLocal = 0;
    int allowedEndBinLocal = 0;
    double allowedEndHzLocal = 0.0;
    double sampleRateHzLocal = 0.0; 
    int fftSizeLocal = 0;
    double displayMaxHzLocal = 0.0;
    this->processor.copyDebugData(tailPreSmooth, tailPostSmooth, tailPostTaper,
                                  bandsPreSmooth, bandsFinal,
                                  hiGuardBinsLocal, allowedEndBinLocal, allowedEndHzLocal,
                                  sampleRateHzLocal, fftSizeLocal, displayMaxHzLocal);

    if (!this->debugHeaderWritten)
    {
        String header = getInitHeader(tailPreSmooth,
                                   tailPostSmooth,
                                   tailPostTaper,
                                   bandsPreSmooth,
                                   bandsFinal);
        // writeString is portable across JUCE versions
        this->debugStream->writeString(header);
        this->debugHeaderWritten = true;
    }

    static long long csvFrameIndex = 0;
    String line;
    line = line << ++csvFrameIndex << "," << hiGuardBinsLocal << "," <<
        allowedEndBinLocal << "," << allowedEndHzLocal
        << "," << sampleRateHzLocal << "," << fftSizeLocal << "," <<
        displayMaxHzLocal;
    for (float value : tailPreSmooth)
    {
        line << "," << value;
    }
    for (float value : tailPostSmooth)
    {
        line << "," << value;
    }
    for (float value : tailPostTaper)
    {
        line << "," << value;
    }
    for (float value : bandsPreSmooth)
    {
        line << "," << value;
    }
    for (float value : bandsFinal)
    {
        line << "," << value;
    }
    line << "\n";
    this->debugStream->writeString(line);
    this->debugStream->flush();
}
