#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

#include "shared/BridgeAnalysis.h"

struct QQDeBreathARASourceInfo
{
    juce::String name;
    juce::String persistentId;
    juce::String sourceFingerprint;
    double sampleRate = 0.0;
    int channelCount = 0;
    juce::int64 numSamples = 0;
    double durationSeconds = 0.0;
    juce::File exportedWav;
    juce::String exportStatus = "No source loaded.";
};

class QQDeBreathARAAudioProcessor final : public juce::AudioProcessor,
                                          private juce::AudioProcessorARAExtension
{
public:
    QQDeBreathARAAudioProcessor();
    ~QQDeBreathARAAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "QQDeBreath_ARA"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }

    double getTailLengthSeconds() const override;

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorARAExtension* getARAClientExtensions() override { return this; }
    void setAnalysisResult(const QQDeBreathBridgeAnalysisResult& result);
    QQDeBreathBridgeAnalysisResult getAnalysisResult() const;
    void clearAnalysisResult();

private:
    juce::AudioBuffer<float> araDryBuffer;
    mutable juce::CriticalSection analysisLock;
    QQDeBreathBridgeAnalysisResult analysisResult;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(QQDeBreathARAAudioProcessor)
};

class QQDeBreathARADocumentController final : public juce::ARADocumentControllerSpecialisation
{
public:
    using juce::ARADocumentControllerSpecialisation::ARADocumentControllerSpecialisation;

protected:
    juce::ARAPlaybackRenderer* doCreatePlaybackRenderer() override;
    bool doRestoreObjectsFromStream(juce::ARAInputStream& input, const juce::ARARestoreObjectsFilter* filter) noexcept override;
    bool doStoreObjectsToStream(juce::ARAOutputStream& output, const juce::ARAStoreObjectsFilter* filter) noexcept override;
};
