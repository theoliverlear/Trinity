#include "TrinityEditor.h"
#include "TrinityProcessor.h"
#include <vector>
#include <cmath>

namespace
{
    Colour getColourFromDb(float db)
    {
        using namespace juce;

        constexpr float greenToYellowStartDb = -20.0f;
        constexpr float yellowToRedStartDb = -10.0f;
        constexpr float maxDb = 0.0f;

        if (db <= greenToYellowStartDb)
        {
            return Colours::green;
        }

        if (db <= yellowToRedStartDb)
        {
            const float ratio = (db - greenToYellowStartDb)
                            / (yellowToRedStartDb - greenToYellowStartDb);
            const uint8 red = static_cast<uint8> (ratio * 255.0f);
            const uint8 green = 255u;
            return Colour::fromRGB (red, green, 0u);
        }

        if (db <= maxDb)
        {
            const float ratio = (db - yellowToRedStartDb)
                            / (maxDb - yellowToRedStartDb);
            const uint8 red = 255u;
            const uint8 green = static_cast<uint8> ((1.0f - ratio) * 255.0f); // 255 -> 0
            return Colour::fromRGB (red, green, 0u);
        }
        return Colours::red;
    }
}

void TrinityAudioProcessorEditor::initSpectrumAnalyzerControls()
{
    // Add spectrum analyzer
    this->addAndMakeVisible(this->spectrumAnalyzer);

    // Controls: Test Signal + type, Debug CSV
    this->addAndMakeVisible(this->btnTestEnabled);
    this->addAndMakeVisible(this->cbxTestType);
    this->addAndMakeVisible(this->btnDebugCsv);

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
    this->cbxTestType.addItem("Sine 17 kHz",  TrinityAudioProcessor::kSine17k);
    this->cbxTestType.addItem("Sine 19 kHz",  TrinityAudioProcessor::kSine19k);
    this->cbxTestType.addItem("White noise",  TrinityAudioProcessor::kWhiteNoise);
    this->cbxTestType.addItem("Pink-ish noise", TrinityAudioProcessor::kPinkNoise);
    this->cbxTestType.addItem("Log sweep",    TrinityAudioProcessor::kLogSweep);
    this->cbxTestType.setSelectedId(TrinityAudioProcessor::kSine17k, juce::dontSendNotification);
}

TrinityAudioProcessorEditor::TrinityAudioProcessorEditor(
    TrinityAudioProcessor& processorRef)
    : AudioProcessorEditor(&processorRef),
      processor(processorRef)
{
    // Increase default editor size for better readability
    this->setSize(420, 560);
    this->startTimerHz(30);
    this->meters = {
        MeterInfo { &this->displayTotal, "Total" },
        MeterInfo { &this->displayLow,   "Low"   },
        MeterInfo { &this->displayMid,   "Mid"   },
        MeterInfo { &this->displayHigh,  "High"  },
    };

    initSpectrumAnalyzerControls();

    initSpectrumAnalyzerButtons();

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
        }
        else
        {
            this->debugStream.reset();
        }
    };

    // Initialize and wire diagnostics controls
    this->btnUiSmooth.setToggleState (true, dontSendNotification);
    this->btnUiPeaks .setToggleState (true, dontSendNotification);
    this->spectrumAnalyzer.setSmoothingEnabled (true);
    this->spectrumAnalyzer.setPeakHoldEnabled (true);
    this->btnUiSmooth.onClick = [this]{ this->spectrumAnalyzer.setSmoothingEnabled (this->btnUiSmooth.getToggleState()); };
    this->btnUiPeaks .onClick = [this]{ this->spectrumAnalyzer.setPeakHoldEnabled  (this->btnUiPeaks.getToggleState()); };

    this->btnFreqSmooth.setButtonText ("Freq Smooth");
    this->btnBandSmooth.setButtonText ("Band Smooth");
    this->btnFreqSmooth.setToggleState (true, juce::dontSendNotification);
    this->btnBandSmooth.setToggleState (true, juce::dontSendNotification);
    this->btnFreqSmooth.onClick = [this]{ this->processor.setFreqSmoothingEnabled (this->btnFreqSmooth.getToggleState()); };
    this->btnBandSmooth.onClick = [this]{ this->processor.setBandSmoothingEnabled (this->btnBandSmooth.getToggleState()); };

    this->sldGuardPercent.setRange (0.0, 0.2, 0.001);
    this->sldGuardPercent.setTextValueSuffix (" guard");
    this->sldGuardPercent.setSliderStyle (juce::Slider::LinearHorizontal);
    this->sldGuardPercent.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 60, 18);
    this->sldGuardPercent.setValue (0.06, juce::dontSendNotification);
    this->sldGuardPercent.onValueChange = [this]{ this->processor.setGuardPercent ((float) this->sldGuardPercent.getValue()); };

    this->sldTaperPercent.setRange (0.0, 0.2, 0.001);
    this->sldTaperPercent.setTextValueSuffix (" taper");
    this->sldTaperPercent.setSliderStyle (juce::Slider::LinearHorizontal);
    this->sldTaperPercent.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 60, 18);
    this->sldTaperPercent.setValue (0.02, juce::dontSendNotification);
    this->sldTaperPercent.onValueChange = [this]{ this->processor.setTaperPercent ((float) this->sldTaperPercent.getValue()); };

    this->sldSpecSmoothing.setRange (0.0, 1.0, 0.001);
    this->sldSpecSmoothing.setTextValueSuffix (" specSmooth");
    this->sldSpecSmoothing.setSliderStyle (juce::Slider::LinearHorizontal);
    this->sldSpecSmoothing.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 60, 18);
    this->sldSpecSmoothing.setValue (0.2, juce::dontSendNotification);
    this->sldSpecSmoothing.onValueChange = [this]{ this->processor.setSpecSmoothing ((float) this->sldSpecSmoothing.getValue()); };

    // Provide frequency range so the analyzer can draw Hz ticks.
    // Use processor's computed post-guard max frequency so ticks align with displayed data.
    this->spectrumAnalyzer.setFrequencyRange(20.0f, static_cast<float>(this->processor.getDisplayMaxHz()));

    // Ensure layout is applied now and on any future resizes
    this->resized();
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
        std::vector<float> mags;
        this->processor.copySpectrum(mags); // values expected in [0..1]
        if (!mags.empty())
        {
            this->spectrumAnalyzer.setMagnitudes(mags);
        }
    }

    // Keep analyzer frequency ticks aligned to processor's current display maximum
    this->spectrumAnalyzer.setFrequencyRange(20.0f, static_cast<float>(this->processor.getDisplayMaxHz()));

    // Optional debug CSV snapshot (throttled)
    if (this->btnDebugCsv.getToggleState())
    {
        if (++this->debugFrameCounter >= 15) // ~0.5s at 30 Hz
        {
            this->debugFrameCounter = 0;
            this->writeDebugCsvSnapshot();
        }
    }

    this->repaint();
}

void TrinityAudioProcessorEditor::resized()
{
    // Layout: spectrum top ~60%, then two controls rows, bottom meters
    auto bounds = this->getLocalBounds();
    auto topArea = bounds.removeFromTop((int) std::round(bounds.getHeight() * 0.60f));
    this->spectrumAnalyzer.setBounds(topArea);

    const int controlsHeight = 72;
    auto controlsArea = bounds.removeFromTop(controlsHeight);
    auto row1 = controlsArea.removeFromTop(controlsHeight / 2);
    auto row2 = controlsArea;

    const int pad = 6;
    const int h1 = row1.getHeight() - pad * 2;
    int x1 = row1.getX() + pad;
    this->btnTestEnabled.setBounds(x1, row1.getY() + pad, 110, h1); x1 += 110 + pad;
    this->cbxTestType.setBounds (x1, row1.getY() + pad, 150, h1);    x1 += 150 + pad;
    this->btnDebugCsv.setBounds (x1, row1.getY() + pad, 100, h1);

    const int h2 = row2.getHeight() - pad * 2;
    int x2 = row2.getX() + pad;
    this->btnUiSmooth.setBounds (x2, row2.getY() + pad, 96, h2); x2 += 96 + pad;
    this->btnUiPeaks .setBounds (x2, row2.getY() + pad, 90, h2); x2 += 90 + pad;
    this->btnFreqSmooth.setBounds (x2, row2.getY() + pad, 110, h2); x2 += 110 + pad;
    this->btnBandSmooth.setBounds (x2, row2.getY() + pad, 110, h2); x2 += 110 + pad;
    const int sliderW = 160;
    this->sldGuardPercent.setBounds (x2, row2.getY() + pad, sliderW, h2); x2 += sliderW + pad;
    this->sldTaperPercent.setBounds (x2, row2.getY() + pad, sliderW, h2); x2 += sliderW + pad;
    this->sldSpecSmoothing.setBounds (x2, row2.getY() + pad, sliderW, h2);
}

void TrinityAudioProcessorEditor::paint(Graphics& graphics)
{
    using namespace juce;
    graphics.fillAll(Colours::black);
    constexpr float minDb = -60.0f;

    // Layout areas: top ~60% reserved for spectrum component, then two controls rows (72px), bottom for meters
    auto layoutBounds = this->getLocalBounds();
    auto topAreaPaint = layoutBounds.removeFromTop((int) std::round(layoutBounds.getHeight() * 0.60f));
    juce::ignoreUnused(topAreaPaint);
    auto controlsPaint = layoutBounds.removeFromTop(72); juce::ignoreUnused(controlsPaint);
    auto bottomHalf = layoutBounds; // remaining area

    const int numMeters = this->meters.size();
    // Enlarge meters for clarity
    const int meterWidth = 56;
    const int meterGap = 12;
    const int totalWidth = numMeters * meterWidth + (numMeters - 1) * meterGap;
    const int startX = bottomHalf.getX() + (bottomHalf.getWidth() - totalWidth) / 2;
    const int bottomY = bottomHalf.getBottom();

    graphics.setFont(16.0f);

    for (int i = 0; i < numMeters; ++i)
    {
        const auto& meter = this->meters[static_cast<std::size_t>(i)];
        const float levelLinear = meter.levelPtr != nullptr ? *meter.levelPtr : 0.0f;
        const float db = Decibels::gainToDecibels(levelLinear, minDb);
        float normalised = jmap(db, minDb, 0.0f, 0.0f, 1.0f);
        normalised = jlimit(0.0f, 1.0f, normalised);
        const float meterHeight = normalised * static_cast<float>(bottomHalf.getHeight() - 40); // leave space for labels
        const int meterPositionX = startX + i * (meterWidth + meterGap);
        const int meterPositionY = bottomY - static_cast<int>(meterHeight);

        // Use dB for colour choice
        graphics.setColour(getColourFromDb(db));

        graphics.fillRect(meterPositionX, meterPositionY, meterWidth, static_cast<int>(meterHeight));

        graphics.setColour(Colours::white);
        // Labels at the top of the bottom area
        Rectangle labelArea(meterPositionX, bottomHalf.getY(), meterWidth, 20);
        graphics.drawFittedText(meter.label, labelArea, Justification::centred, 1);

        if (levelLinear > 0.0f)
        {
            Rectangle dbArea(meterPositionX, bottomHalf.getY() + 20, meterWidth, 20);
            graphics.drawFittedText(String(db, 1) + " dB",
                                    dbArea,
                                    Justification::centred,
                                    1);
        }
    }
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

void TrinityAudioProcessorEditor::writeDebugCsvSnapshot()
{
    ensureDebugStreamOpen();
    if (!this->debugStream || !this->debugStream->openedOk())
    {
        return;
    }

    // New extended debug data
    std::vector<float> tailPre, tailPostSmooth, tailPostTaper;
    std::vector<float> bandsPre, bandsFinal;
    int hiGuard = 0;
    int allowedEndBin = 0;
    double allowedEndHz = 0.0;
    double sr = 0.0; int n = 0;
    double dispMax = 0.0;
    this->processor.copyDebugData (tailPre, tailPostSmooth, tailPostTaper,
                             bandsPre, bandsFinal,
                             hiGuard, allowedEndBin, allowedEndHz,
                             sr, n, dispMax);

    if (!this->debugHeaderWritten)
    {
        String header = "csvVersion=2\n";
        header += "frame,hiGuard,allowedEndBin,allowedEndHz,sampleRate,fftSize,displayMaxHz";
        for (size_t i = 0; i < tailPre.size(); ++i)
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
        for (size_t i = 0; i < bandsPre.size(); ++i)
        {
            header += ",bandPre" + String(static_cast<int>(i));
        }
        for (size_t i = 0; i < bandsFinal.size(); ++i)
        {
            header += ",bandFinal" + String(static_cast<int>(i));
        }
        header += "\n";
        // writeString is portable across JUCE versions
        this->debugStream->writeString(header);
        this->debugHeaderWritten = true;
    }

    static long long csvFrameIndex = 0;
    String line;
    line << (++csvFrameIndex) << "," << hiGuard << "," << allowedEndBin << "," << allowedEndHz
         << "," << sr << "," << n << "," << dispMax;
    for (float value : tailPre)        line << "," << value;
    for (float value : tailPostSmooth) line << "," << value;
    for (float value : tailPostTaper)  line << "," << value;
    for (float value : bandsPre)       line << "," << value;
    for (float value : bandsFinal)     line << "," << value;
    line << "\n";
    debugStream->writeString(line);
    debugStream->flush();
}
