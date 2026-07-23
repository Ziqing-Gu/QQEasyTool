#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

#include "ARAProcessor.h"
#include "shared/WaveformEditorComponent.h"

class QQDeBreathARAAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                                private juce::AudioProcessorEditorARAExtension,
                                                private juce::Timer
{
public:
    explicit QQDeBreathARAAudioProcessorEditor(QQDeBreathARAAudioProcessor& processor);
    ~QQDeBreathARAAudioProcessorEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;
    void reloadSource();
    void updatePlayheadFromHost();
    void startAnalysis();
    void cancelAnalysis();
    void clearAnalysis();
    void updateAnalysisLabels();
    void handleAnalysisFinished(const QQDeBreathBridgeAnalysisResult& result);
    void waveformRegionsChanged(const juce::Array<QQDeBreathBridgeRegion>& regions);
    void waveformSeekRequested(double localSeconds);
    juce::ARAAudioSource* findCurrentAudioSource() const;
    bool exportSourceToWav(juce::ARAAudioSource& source, QQDeBreathARASourceInfo& info);
    void updateLabels();

    static juce::String buildSourceFingerprint(const juce::ARAAudioSource& source);
    static juce::String makeSourceName(const juce::ARAAudioSource& source);
    static juce::File getExportDirectory();
    static juce::File getAnalysisDirectory(const juce::String& sourceKey);

    class AnalysisThread;

    QQDeBreathARAAudioProcessor& audioProcessor;
    QQDeBreathARASourceInfo sourceInfo;

    juce::Label titleLabel;
    juce::Label phaseLabel;
    juce::TextButton reloadButton { "Reload Source" };
    juce::TextButton analyzeButton { "Analyze" };
    juce::TextButton cancelAnalyzeButton { "Cancel" };
    juce::TextButton clearAnalysisButton { "Clear Analysis" };
    juce::Label sourceNameLabel;
    juce::Label sourceIdLabel;
    juce::Label sampleRateLabel;
    juce::Label channelsLabel;
    juce::Label samplesLabel;
    juce::Label durationLabel;
    juce::Label exportPathLabel;
    juce::Label analysisStatusLabel;
    juce::Label breathCountLabel;
    juce::Label noizeCountLabel;
    juce::Label analysisErrorLabel;
    juce::Label sourceMismatchLabel;
    juce::Label statusLabel;
    juce::ToggleButton monitorVoiceButton { "Voice" };
    juce::ToggleButton monitorBreathButton { "Breath" };
    juce::ToggleButton monitorNoizeButton { "Noize" };
    QQDeBreathWaveformEditor waveformEditor;
    std::unique_ptr<AnalysisThread> analysisThread;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(QQDeBreathARAAudioProcessorEditor)
};
