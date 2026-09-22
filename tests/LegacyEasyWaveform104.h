#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

#include "shared/BridgeAnalysis.h"

class LegacyEasyWaveform104 final : public juce::Component,
                                       private juce::ScrollBar::Listener
{
public:
    struct DisplayProcessingParams
    {
        bool enableFade = true;
        bool normalizeBreath = false;
        bool normalizeSibilance = false;
        double fadeInMs = 10.0;
        double fadeOutMs = 10.0;
        double breathTargetDb = -6.0;
        double sibilanceTargetDb = -12.0;
        double breathGainDb = 0.0;
        double sibilanceGainDb = 0.0;
        QQDeBreathEqState breathEqState;
        QQDeBreathEqState sibilanceEqState;
    };

    LegacyEasyWaveform104();

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseMove(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;
    bool keyPressed(const juce::KeyPress& key) override;

    bool loadAudioFile(const juce::File& file, juce::String& status);
    void setAudioBuffer(const juce::AudioBuffer<float>& audio, double sourceSampleRate, double timelineDurationSeconds = 0.0, bool preserveView = false);
    void clearAudio();
    void setAnalysisResult(const QQDeBreathBridgeAnalysisResult& result);
    void clearRegions();
    const juce::Array<QQDeBreathBridgeRegion>& getRegions() const { return regions; }
    juce::Array<double> buildRegionPeakCache(const juce::Array<QQDeBreathBridgeRegion>& regionsToMeasure) const;
    int getSelectedRegionIndex() const { return selectedRegion; }
    QQDeBreathBridgeRegion getRegion(int index) const;
    void setRegionProcessing(int index, double gainDb, const QQDeBreathEqState& eqState, bool notifyChange = true, bool rebuildDisplay = true);
    void setCreationType(const juce::String& type);
    void setSelectedRegionType(const juce::String& type);
    juce::String getCreationType() const { return creationType; }
    bool canUndo() const;
    bool canRedo() const;
    void undo();
    void redo();
    double getDurationSeconds() const;
    double getTimelineDurationSeconds() const;
    void setTimelineDurationSeconds(double seconds);
    void setPlayheadSeconds(double seconds);
    void setWaveformDisplayGain(double gain);
    void setMonitorState(bool voiceEnabled, bool breathEnabled, bool noiseEnabled, bool sibilanceEnabled, bool othersEnabled);
    void setProcessingParams(const DisplayProcessingParams& params);
    void refreshProcessedDisplay();
    void setFollowPlayhead(bool shouldFollow);
    void setRecordingOverlay(bool shouldShow, const juce::String& text = "Recording...");

    std::function<void(const juce::Array<QQDeBreathBridgeRegion>&)> onRegionsChanged;
    std::function<void(int)> onSelectedRegionChanged;
    std::function<void(double)> onSeekRequested;
    std::function<void(int)> onRegionDoubleClicked;

private:
    friend class QQDeBreathWaveformDisplayProbe;
    enum class DragMode
    {
        none,
        create,
        move,
        resizeStart,
        resizeEnd
    };

    struct DisplayRegion
    {
        int sourceIndex = -1;
        juce::String type;
        juce::int64 startSample = 0;
        juce::int64 endSample = 0;
        double normGain = 1.0;
        bool adjacentBefore = false;
        bool adjacentAfter = false;
    };

    int timeToX(double seconds) const;
    double xToTime(int x) const;
    int hitTestRegion(int x) const;
    int hitTestRegionStartEdge(int x) const;
    int hitTestRegionEndEdge(int x) const;
    void updateMouseCursor(int x);
    void selectRegion(int index);
    void pushUndoState();
    void notifyRegionsChanged(bool rebuildDisplay = true);
    int insertRegionReplacingOverlaps(QQDeBreathBridgeRegion region);
    void fitView();
    void setView(double start, double end);
    juce::Rectangle<int> getWaveformArea() const;
    void updateScrollBar();
    void scrollBarMoved(juce::ScrollBar* scrollBarThatHasMoved, double newRangeStart) override;
    juce::Colour regionColour(const juce::String& type) const;
    juce::int64 regionStartSample(const QQDeBreathBridgeRegion& region) const;
    juce::int64 regionEndSample(const QQDeBreathBridgeRegion& region) const;
    bool hasAdjacentRegionBefore(int regionIndex, juce::int64 startSample, int fadeSamples) const;
    bool hasAdjacentRegionAfter(int regionIndex, juce::int64 endSample, int fadeSamples) const;
    double regionWeightAtSample(int regionIndex, juce::int64 sampleIndex, int fadeInSamples, int fadeOutSamples) const;
    double breathNormGainForRegion(int regionIndex) const;
    double displayRegionWeightAtSample(const DisplayRegion& region, juce::int64 sampleIndex, int fadeInSamples, int fadeOutSamples) const;
    double renderedDisplaySample(juce::int64 sampleIndex, bool activeComponents) const;
    void drawFadeGuides(juce::Graphics& g, juce::Rectangle<int> area) const;
    void rebuildBreathNormGainCache();
    void rebuildProcessedBreathDisplay();
    juce::String buildProcessedBreathDisplayKey() const;
    static double dbToGain(double db);

    juce::AudioFormatManager formatManager;
    juce::ScrollBar horizontalScrollBar { false };
    juce::AudioBuffer<float> monoWaveform;
    double sampleRate = 0.0;
    double timelineDuration = 0.0;
    juce::Array<QQDeBreathBridgeRegion> regions;
    juce::Array<juce::Array<QQDeBreathBridgeRegion>> undoStack;
    juce::Array<juce::Array<QQDeBreathBridgeRegion>> redoStack;
    juce::Array<double> breathNormGainCache;
    juce::Array<DisplayRegion> displayRegions;
    juce::AudioBuffer<float> processedBreathDisplay;
    juce::AudioBuffer<float> processedSibilanceDisplay;
    juce::AudioBuffer<float> processedOthersDisplay;
    juce::String processedBreathDisplayKey;
    int selectedRegion = -1;
    double viewStart = 0.0;
    double viewEnd = 1.0;
    double playhead = 0.0;
    double waveformDisplayGain = 1.0;
    bool monitorVoice = true;
    bool monitorBreath = true;
    bool monitorNoize = true;
    bool monitorSibilance = true;
    bool monitorOthers = true;
    bool updatingScrollBar = false;
    bool followPlayhead = false;
    bool recordingOverlay = false;
    juce::String recordingOverlayText;
    DisplayProcessingParams processingParams;
    DragMode dragMode = DragMode::none;
    double createStart = 0.0;
    double createEnd = 0.0;
    juce::String creationType { "Breath" };
    int resizeRegion = -1;
    int moveRegion = -1;
    double movePointerStartTime = 0.0;
    double moveOriginalStartTime = 0.0;
    double moveOriginalEndTime = 0.0;
    bool moveUndoPushed = false;
    bool deferredRegionDisplayRebuild = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LegacyEasyWaveform104)
};
