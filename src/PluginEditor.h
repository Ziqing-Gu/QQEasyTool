#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include "PluginProcessor.h"
#include "shared/BreathEqComponent.h"
#include "shared/WaveformEditorComponent.h"

class QQDeBreathAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                             private juce::AudioProcessorEditorARAExtension,
                                             private juce::Timer
{
public:
    explicit QQDeBreathAudioProcessorEditor(QQDeBreathAudioProcessor& processor);
    ~QQDeBreathAudioProcessorEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;
    void parentHierarchyChanged() override;
    void visibilityChanged() override;
    void setScaleFactor(float newScaleFactor) override;

private:
    enum class SourceMode
    {
        none,
        recorded,
        ara
    };

    enum class GlobalEqMode
    {
        breath,
        sibilance
    };

    class DragValueSlider final : public juce::Slider
    {
    public:
        void configure(juce::String suffixIn,
                       double defaultValueIn,
                       double coarseStepIn,
                       double fineStepIn,
                       int decimalPlacesIn)
        {
            suffix = std::move(suffixIn);
            defaultValue = defaultValueIn;
            coarseStep = coarseStepIn;
            fineStep = fineStepIn;
            decimalPlaces = decimalPlacesIn;
        }

        void paint(juce::Graphics& g) override
        {
            auto bounds = getLocalBounds().toFloat().reduced(1.0f);
            g.setColour(isEnabled() ? juce::Colour(0xff0f172a) : juce::Colour(0xff111827));
            g.fillRoundedRectangle(bounds, 2.0f);
            g.setColour(isMouseOverOrDragging() ? juce::Colour(0xff60a5fa) : juce::Colour(0xff334155));
            g.drawRoundedRectangle(bounds, 2.0f, 1.0f);
            g.setColour(isEnabled() ? juce::Colour(0xffe2e8f0) : juce::Colour(0xff64748b));
            g.setFont(juce::Font(13.0f, juce::Font::plain));
            g.drawText(juce::String(getValue(), decimalPlaces) + suffix,
                       getLocalBounds().reduced(4, 0),
                       juce::Justification::centred);
        }

        void mouseDown(const juce::MouseEvent& event) override
        {
            if (event.mods.isAltDown() && event.mods.isLeftButtonDown())
            {
                setValue(defaultValue, juce::sendNotificationSync);
                return;
            }

            dragStartY = event.position.y;
            dragStartValue = getValue();
        }

        void mouseDoubleClick(const juce::MouseEvent& event) override
        {
            juce::ignoreUnused(event);
            showInlineEditor();
        }

        void mouseDrag(const juce::MouseEvent& event) override
        {
            const auto step = event.mods.isShiftDown() ? fineStep : coarseStep;
            const auto pixelsPerStep = event.mods.isShiftDown() ? 18.0 : 14.0;
            const auto delta = static_cast<double>(dragStartY - event.position.y) / pixelsPerStep;
            setSnappedValue(dragStartValue + delta * step, juce::sendNotificationAsync);
        }

        void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override
        {
            const auto step = event.mods.isShiftDown() ? fineStep : coarseStep;
            const auto direction = wheel.deltaY >= 0.0f ? 1.0 : -1.0;
            setSnappedValue(getValue() + direction * step, juce::sendNotificationAsync);
        }

        void resized() override
        {
            if (inlineEditor != nullptr)
                inlineEditor->setBounds(getLocalBounds().reduced(1));
        }

    private:
        juce::String getDisplayText() const
        {
            return juce::String(getValue(), decimalPlaces) + suffix;
        }

        double parseValueText(juce::String text) const
        {
            text = text.trim()
                       .replace("dB", "", true)
                       .replace("ms", "", true)
                       .replace("x", "", true)
                       .replace(",", ".");
            return text.getDoubleValue();
        }

        void showInlineEditor()
        {
            inlineEditor = std::make_unique<juce::TextEditor>();
            inlineEditor->setJustification(juce::Justification::centred);
            inlineEditor->setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff020617));
            inlineEditor->setColour(juce::TextEditor::textColourId, juce::Colour(0xfff8fafc));
            inlineEditor->setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff60a5fa));
            inlineEditor->setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(0xff60a5fa));
            inlineEditor->setInputRestrictions(0, "0123456789.,-+ dBmsxDBMSX");
            inlineEditor->setText(getDisplayText(), juce::dontSendNotification);
            inlineEditor->selectAll();
            inlineEditor->onReturnKey = [this] { commitInlineEditor(false); };
            inlineEditor->onEscapeKey = [this] { commitInlineEditor(true); };
            inlineEditor->onFocusLost = [this] { commitInlineEditor(false); };
            addAndMakeVisible(*inlineEditor);
            resized();
            inlineEditor->grabKeyboardFocus();
        }

        void commitInlineEditor(bool discard)
        {
            if (inlineEditor == nullptr)
                return;

            const auto text = inlineEditor->getText();
            inlineEditor.reset();

            if (! discard)
                setSnappedValue(parseValueText(text), juce::sendNotificationSync);

            repaint();
        }

        void setSnappedValue(double value, juce::NotificationType notification)
        {
            const auto interval = getInterval();
            if (interval > 0.0)
                value = std::round(value / interval) * interval;

            setValue(juce::jlimit(getMinimum(), getMaximum(), value), notification);
        }

        juce::String suffix;
        std::unique_ptr<juce::TextEditor> inlineEditor;
        double defaultValue = 0.0;
        double coarseStep = 1.0;
        double fineStep = 0.1;
        double dragStartY = 0.0;
        double dragStartValue = 0.0;
        int decimalPlaces = 1;
    };

    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    void timerCallback() override;
    void scheduleInitialHostLayoutSync();
    void performInitialHostLayoutSync();
    void updateRecordingInfo();
    bool isAraContext() const;
    void updateContextUi();
    QQDeBreathARADocumentController* getAraDocumentController() const;
    void persistAraState();
    void persistAraMappedSourceStates(const QQDeBreathBridgeAnalysisResult& result);
    void updateAraRuntimeState();
    void updateAraRuntimeMappedSourceStates(const QQDeBreathBridgeAnalysisResult& result);
    void restoreAraStateIfNeeded();
    QQDeBreathARAPlaybackParams buildAraPlaybackParams() const;
    void syncAraPlaybackParams();
    double getHostTimeSeconds(bool* hostIsPlaying = nullptr) const;
    void updatePlayheadFromHost(const QQDeBreathAudioProcessor::RecordedBufferInfo& info);
    void reloadAraSource();
    juce::ARAAudioSource* findCurrentAudioSource() const;
    juce::Array<juce::ARAPlaybackRegion*> findSelectedAraPlaybackRegions() const;
    void populateAraPlaybackMappings(juce::ARAAudioSource& source, QQDeBreathARASourceInfo& info) const;
    bool exportSelectedAraPlaybackRegionsToWav(QQDeBreathARASourceInfo& info);
    bool exportAraSourceToWav(juce::ARAAudioSource& source, QQDeBreathARASourceInfo& info);
    bool cacheAraSourceForPlayback(juce::ARAAudioSource& source, juce::String& status);
    double getAraLocalPlayheadSeconds(double hostTimeSeconds) const;
    juce::String getActiveSourceKey(const QQDeBreathAudioProcessor::RecordedBufferInfo& info) const;
    bool hasAnalyzableSource(const QQDeBreathAudioProcessor::RecordedBufferInfo& info) const;
    void refreshRecordedWaveformIfNeeded(const QQDeBreathAudioProcessor::RecordedBufferInfo& info);
    void showMainPage();
    void showGlobalEqPage();
    void showBreathDetailPage();
    void setMainPageComponentsVisible(bool visible);
    void setBreathEqEnabled(bool enabled);
    void setGlobalEqMode(GlobalEqMode mode);
    juce::String getActiveGlobalTypeName() const;
    QQDeBreathEqState getActiveGlobalEqState() const;
    void setActiveGlobalEqState(const QQDeBreathEqState& state);
    DragValueSlider& getActiveGlobalGainSlider();
    void applyBreathEqStateFromUi(const QQDeBreathEqState& state);
    void applyBreathEqPreviewToWaveform();
    void rollbackBreathEqPreviewIfNeeded();
    void saveCurrentGlobalDefaults();
    void applySavedGlobalDefaultsIfFreshInstance();
    void applySavedGlobalDefaults();
    void applyGlobalDefaultsFromValue(const juce::var& value);
    void requestDeferredWaveformRefresh(bool persistAfterRefresh);
    void requestDeferredSpectrumRefresh();
    void requestDeferredAraRuntimeUpdate();
    void refreshBreathEqUi();
    void refreshBreathEqSpectrum();
    void refreshBreathEqSpectrumSource();
    void updateBreathEqDynamicSpectrum();
    void setBreathEqLoopEnabled(bool enabled);
    bool findNextBreathLoopRange(double& startSeconds, double& endSeconds) const;
    double getCurrentLocalPlayheadSeconds(const QQDeBreathAudioProcessor::RecordedBufferInfo& info) const;
    void waveformSelectionChanged(int index);
    bool selectedRegionSupportsProcessing() const;
    void refreshBreathDetailControlsVisibility();
    void refreshBreathDetailUi();
    void applyBreathDetailFromUi();
    void applyBreathDetailPreviewToWaveform();
    void rollbackBreathDetailPreviewIfNeeded();
    void updateBreathDetailPreviewForListening(double gainDb, const QQDeBreathEqState& eqState);
    void updateAnalysisInfo();
    void exportRecording();
    void startAnalysis();
    void cancelAnalysis();
    void clearAnalysis();
    void handleAnalysisFinished(const QQDeBreathBridgeAnalysisResult& result);
    void waveformRegionsChanged(const juce::Array<QQDeBreathBridgeRegion>& regions);
    void waveformSeekRequested(double localSeconds);
    void exportToDirectory(const juce::File& directory);
    bool renderCurrentStemsToDirectory(const juce::File& directory, juce::String& status);
    static juce::String buildAraSourceFingerprint(const juce::ARAAudioSource& source);
    static juce::String makeAraSourceName(const juce::ARAAudioSource& source);
    static juce::File getAraExportDirectory();

    class AnalysisThread;

    QQDeBreathAudioProcessor& audioProcessor;

    juce::Label titleLabel;
    juce::Label phaseLabel;
    juce::Label passthroughLabel;
    juce::Label parameterHintLabel;
    juce::TextButton loadAraButton { "Load" };
    juce::TextButton recordButton { "Record" };
    juce::TextButton stopButton { "Stop" };
    juce::TextButton clearButton { "Clear Recording" };
    juce::TextButton undoButton { "Undo" };
    juce::TextButton redoButton { "Redo" };
    juce::TextButton exportButton { "Export Stems" };
    juce::TextButton breathDetailButton { "Region Adjust" };
    juce::ToggleButton breathDetailBypassButton { "ON" };
    juce::Label breathDetailTopGainLabel;
    juce::ToggleButton breathEqMainEnableButton;
    juce::ToggleButton sibilanceEqMainEnableButton;
    juce::TextButton breathEqPageButton { "Global EQ" };
    juce::TextButton setDefaultButton { "Set as Default" };
    juce::TextButton analyzeButton { "Analyze" };
    juce::TextButton cancelAnalyzeButton { "Cancel" };
    juce::TextButton clearAnalysisButton { "Clear Analysis" };
    juce::Label sampleRateLabel;
    juce::Label channelsLabel;
    juce::Label samplesLabel;
    juce::Label durationLabel;
    juce::Label exportPathLabel;
    juce::Label analysisStatusLabel;
    juce::Label breathCountLabel;
    juce::Label noizeCountLabel;
    juce::Label sibilanceCountLabel;
    juce::Label othersCountLabel;
    juce::Label analysisErrorLabel;
    juce::Label sourceMismatchLabel;
    juce::Label statusLabel;
    juce::Label workflowLabel;
    juce::ToggleButton monitorVoiceButton { "Voice" };
    juce::ToggleButton monitorBreathButton { "Breath" };
    juce::ToggleButton monitorNoizeButton { "Noise" };
    juce::ToggleButton monitorSibilanceButton { "Sibilance" };
    juce::ToggleButton monitorOthersButton { "Others" };
    juce::ToggleButton followButton { "Follow" };
    juce::ToggleButton fadeButton { "Fade" };
    juce::ToggleButton breathNormButton { "Breath Norm" };
    juce::ToggleButton sibilanceNormButton { "Sibilance Norm" };
    juce::Label fadeInLabel;
    juce::Label fadeOutLabel;
    juce::Label breathTargetLabel;
    juce::Label sibilanceTargetLabel;
    juce::Label breathGainLabel;
    juce::Label sibilanceGainLabel;
    juce::Label waveformSizeLabel;
    DragValueSlider fadeInSlider;
    DragValueSlider fadeOutSlider;
    DragValueSlider breathTargetSlider;
    DragValueSlider sibilanceTargetSlider;
    DragValueSlider breathGainSlider;
    DragValueSlider sibilanceGainSlider;
    DragValueSlider waveformSizeSlider;
    QQDeBreathWaveformEditor waveformEditor;
    juce::TextButton closeBreathEqButton { "X" };
    juce::ToggleButton breathEqEnableButton { "ON" };
    juce::TextButton eqScopeGlobalButton { "Global EQ" };
    juce::TextButton eqScopeRegionButton { "Region EQ" };
    juce::ToggleButton breathEqHighPassButton { "HP" };
    juce::ToggleButton breathEqLowPassButton { "LP" };
    juce::ToggleButton breathEqLoopButton { "Loop" };
    juce::ToggleButton breathEqAutoApplyButton { "Auto Apply" };
    juce::TextButton breathEqApplyButton { "Apply" };
    juce::TextButton breathEqClearButton { "Clear" };
    QQDeBreathBreathEqComponent breathEqEditor;
    juce::TextButton closeBreathDetailButton { "X" };
    juce::Label breathDetailTitleLabel;
    juce::ToggleButton breathDetailEqPowerButton { "EQ ON" };
    juce::ToggleButton breathDetailEqEnableButton { "ON" };
    juce::Label breathDetailGainLabel;
    DragValueSlider breathDetailGainSlider;
    juce::ToggleButton breathDetailAutoApplyButton { "Auto Apply" };
    juce::TextButton breathDetailApplyButton { "Apply" };
    juce::TextButton breathDetailClearButton { "Clear" };
    QQDeBreathBreathEqComponent breathDetailEqEditor;
    SourceMode sourceMode = SourceMode::none;
    QQDeBreathARASourceInfo araSourceInfo;
    bool araPlaybackCacheReady = false;
    bool araUiMode = false;
    juce::File lastExportedFile;
    juce::int64 lastDisplayedRecordedSamples = 0;
    uint32_t lastWaveformRefreshMs = 0;
    uint32_t lastAraRestoreAttemptMs = 0;
    bool showingLiveRecordingPreview = false;
    bool showingBreathEqPage = false;
    GlobalEqMode globalEqMode = GlobalEqMode::breath;
    bool showingBreathDetailPage = false;
    bool updatingBreathDetailUi = false;
    bool breathEqPreviewDirty = false;
    bool breathDetailPreviewDirty = false;
    bool pendingWaveformRefresh = false;
    bool pendingWaveformRefreshShouldPersist = false;
    bool pendingSpectrumRefresh = false;
    bool pendingAraRuntimeUpdate = false;
    bool initialHostLayoutSyncPending = false;
    bool attemptedGlobalDefaultsLoad = false;
    uint32_t pendingWaveformRefreshMs = 0;
    uint32_t pendingSpectrumRefreshMs = 0;
    uint32_t pendingAraRuntimeUpdateMs = 0;
    int initialHostLayoutSyncAttempts = 0;
    int selectedRegionIndex = -1;
    int breathDetailSnapshotIndex = -1;
    double breathDetailSnapshotGainDb = 0.0;
    QQDeBreathEqState breathEqPageSnapshot;
    QQDeBreathEqState breathDetailSnapshotEq;
    uint32_t lastBreathEqDynamicSpectrumMs = 0;
    uint32_t lastBreathDetailPersistMs = 0;
    bool pendingBreathDetailPersist = false;
    juce::String breathEqSpectrumSourceKey;
    double breathEqSpectrumSampleRate = 0.0;
    juce::AudioBuffer<float> breathEqSpectrumSourceBuffer;
    std::map<std::pair<juce::int64, juce::int64>, double> breathEqSpectrumPeakMemo;
    std::vector<float> globalPreSpectrumPeak;
    std::vector<float> globalPostSpectrumPeak;
    std::vector<float> detailPreSpectrumPeak;
    std::vector<float> detailPostSpectrumPeak;
    std::unique_ptr<AnalysisThread> analysisThread;
    std::unique_ptr<juce::FileChooser> fileChooser;
    std::unique_ptr<ButtonAttachment> monitorVoiceAttachment;
    std::unique_ptr<ButtonAttachment> monitorBreathAttachment;
    std::unique_ptr<ButtonAttachment> monitorNoizeAttachment;
    std::unique_ptr<ButtonAttachment> monitorSibilanceAttachment;
    std::unique_ptr<ButtonAttachment> monitorOthersAttachment;
    std::unique_ptr<ButtonAttachment> followAttachment;
    std::unique_ptr<ButtonAttachment> fadeAttachment;
    std::unique_ptr<ButtonAttachment> breathNormAttachment;
    std::unique_ptr<ButtonAttachment> sibilanceNormAttachment;
    std::unique_ptr<SliderAttachment> fadeInAttachment;
    std::unique_ptr<SliderAttachment> fadeOutAttachment;
    std::unique_ptr<SliderAttachment> breathTargetAttachment;
    std::unique_ptr<SliderAttachment> sibilanceTargetAttachment;
    std::unique_ptr<SliderAttachment> breathGainAttachment;
    std::unique_ptr<SliderAttachment> sibilanceGainAttachment;
    std::unique_ptr<SliderAttachment> waveformSizeAttachment;
    uint32_t analysisStartMs = 0;


    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(QQDeBreathAudioProcessorEditor)
};
