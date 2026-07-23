#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

#include <cstdint>
#include <memory>
#include <vector>

#include "Parameters.h"
#include "shared/BridgeAnalysis.h"
#include "shared/BreathEq.h"

struct QQDeBreathARASourceInfo
{
    struct PlaybackMapping
    {
        juce::String sourceName;
        juce::String persistentId;
        juce::String sourceFingerprint;
        double sourceSampleRate = 0.0;
        int sourceChannelCount = 0;
        juce::int64 sourceNumSamples = 0;
        double compositeStartSeconds = 0.0;
        double compositeEndSeconds = 0.0;
        double sourceStartSeconds = 0.0;
        double playbackStartSeconds = 0.0;
        double playbackEndSeconds = 0.0;
    };

    juce::String name;
    juce::String persistentId;
    juce::String sourceFingerprint;
    bool isComposite = false;
    double compositePlaybackStartSeconds = 0.0;
    double compositePlaybackEndSeconds = 0.0;
    double sampleRate = 0.0;
    int channelCount = 0;
    juce::int64 numSamples = 0;
    double durationSeconds = 0.0;
    juce::File exportedWav;
    juce::String exportStatus = "No ARA source loaded.";
    juce::Array<PlaybackMapping> playbackMappings;
};

struct QQDeBreathARAPersistentState
{
    QQDeBreathARASourceInfo sourceInfo;
    QQDeBreathBridgeAnalysisResult analysisResult;
    juce::Array<double> regionPeakCache;
};

using QQDeBreathARASourceAudioBuffer = std::shared_ptr<const juce::AudioBuffer<float>>;

struct QQDeBreathARAPlaybackParams
{
    bool monitorVoice = true;
    bool monitorBreath = true;
    bool monitorNoize = true;
    bool monitorSibilance = true;
    bool monitorOthers = true;
    bool followPlayhead = false;
    bool enableFade = true;
    bool normalizeBreath = false;
    bool normalizeSibilance = false;
    double fadeInMs = 10.0;
    double fadeOutMs = 10.0;
    double breathTargetDb = -6.0;
    double sibilanceTargetDb = -12.0;
    double breathGainDb = 0.0;
    double sibilanceGainDb = 0.0;
    double waveformDisplayGain = 1.0;
    QQDeBreathEqState breathEqState;
    QQDeBreathEqState sibilanceEqState;

    // Live region processing snapshots travel with the same controller channel
    // as the global controls. Persistent ARA state remains the source of truth;
    // this cache makes Gain/EQ edits audible immediately in Cubase renderers.
    std::shared_ptr<const juce::Array<QQDeBreathARAPersistentState>> runtimeRegionStates;
};

class QQDeBreathAudioProcessor final : public juce::AudioProcessor,
                                       private juce::AudioProcessorARAExtension
{
public:
    struct RecordedBufferInfo
    {
        bool isRecordArmed = false;
        bool isRecording = false;
        bool hasRecording = false;
        double sampleRate = 0.0;
        int channelCount = 0;
        juce::int64 numSamples = 0;
        double durationSeconds = 0.0;
        double recordingStartTimelineSeconds = -1.0;
        juce::String sourceFingerprint;
        juce::String status;
    };

    QQDeBreathAudioProcessor();
    ~QQDeBreathAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "QQEasyTool"; }

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
    bool hasRestoredStateInformation() const noexcept { return restoredStateInformation.load(std::memory_order_acquire); }
    bool claimInitialGlobalDefaultsApplication() noexcept
    {
        auto expected = false;
        return globalDefaultsApplicationClaimed.compare_exchange_strong(expected, true, std::memory_order_acq_rel);
    }

    void startRecording();
    void stopRecording();
    void clearRecording();
    RecordedBufferInfo getRecordedBufferInfo() const;
    bool copyRecordedBuffer(juce::AudioBuffer<float>& dest, double& sampleRate) const;
    bool tryCopyRecordedBuffer(juce::AudioBuffer<float>& dest, double& sampleRate) const;
    bool exportRecordedBufferToTempWav(juce::File& exportedFile, juce::String& status) const;
    bool exportRecordedBufferToWav(const juce::File& outputFile, juce::String& status) const;
    void setAnalysisResult(const QQDeBreathBridgeAnalysisResult& result);
    void setAnalysisRegionPeakCache(const juce::Array<double>& peakCache);
    void updateAnalysisRegionsPreservingCaches(const juce::Array<QQDeBreathBridgeRegion>& regions);
    QQDeBreathBridgeAnalysisResult getAnalysisResult() const;
    void clearAnalysisResult();
    void setAraSourceInfo(const QQDeBreathARASourceInfo& info);
    QQDeBreathARASourceInfo getAraSourceInfo() const;
    void clearAraSourceInfo();
    QQDeBreathEqState getBreathEqState() const;
    void setBreathEqState(const QQDeBreathEqState& state);
    void applyBreathEqToBuffer(juce::AudioBuffer<float>& buffer, double sampleRate) const;
    QQDeBreathEqState getSibilanceEqState() const;
    void setSibilanceEqState(const QQDeBreathEqState& state);
    void applySibilanceEqToBuffer(juce::AudioBuffer<float>& buffer, double sampleRate) const;
    void setInternalPreviewPosition(double localSeconds, double hostTimeSeconds);
    void setInternalPreviewLoopRange(double startSeconds, double endSeconds, double hostTimeSeconds);
    void clearInternalPreviewLoop();
    void clearInternalPreviewPosition();
    double getInternalPreviewPosition(double hostTimeSeconds) const;
    juce::AudioProcessorARAExtension* getARAClientExtensions() override { return this; }
    bool isBoundToAraHost() const noexcept { return isBoundToARA(); }
    bool hasAraPlaybackRendererRole() const noexcept { return isPlaybackRenderer(); }
#if defined(QQEASYTOOL_ENABLE_ROUTE_TESTS)
    bool renderAraInputBlockForTesting(juce::AudioBuffer<float>& buffer, double hostTimeSeconds)
    {
        return renderAraInputBlock(buffer, hostTimeSeconds);
    }
#endif


    juce::AudioProcessorValueTreeState parameters;

private:
    void appendToRecordedBuffer(const juce::AudioBuffer<float>& buffer, double hostTimeSeconds);
    bool renderPreviewBlock(juce::AudioBuffer<float>& buffer, double hostTimeSeconds);
    bool renderAraInputBlock(juce::AudioBuffer<float>& buffer, double hostTimeSeconds);
    double getInternalPreviewPositionUnlocked(double hostTimeSeconds) const;
    juce::Array<double> buildRegionPeakCacheForResult(const QQDeBreathBridgeAnalysisResult& result) const;
    bool exportRecordedBufferToWavUnchecked(const juce::File& outputFile, juce::String& status) const;
    static juce::File getRecordingExportDirectory();

    mutable juce::CriticalSection recordedBufferLock;
    juce::AudioBuffer<float> recordedBuffer;
    juce::int64 recordedLengthSamples = 0;
    double recordedSampleRate = 0.0;
    double recordingStartTimelineSeconds = -1.0;
    std::atomic<bool> recordArmed { false };
    std::atomic<bool> recording { false };
    std::atomic<bool> restoredStateInformation { false };
    std::atomic<bool> globalDefaultsApplicationClaimed { false };
    std::atomic<int> droppedRecordBlocks { 0 };
    std::atomic<bool> internalPreviewActive { false };
    std::atomic<bool> internalPreviewLoopEnabled { false };
    std::atomic<double> internalPreviewAnchorLocalSeconds { 0.0 };
    std::atomic<double> internalPreviewAnchorHostSeconds { 0.0 };
    std::atomic<double> internalPreviewLoopStartSeconds { 0.0 };
    std::atomic<double> internalPreviewLoopEndSeconds { 0.0 };

    juce::AudioBuffer<float> previewBreathBuffer;
    juce::AudioBuffer<float> previewSibilanceBuffer;
    juce::AudioBuffer<float> previewOthersBuffer;
    juce::AudioBuffer<float> previewNoiseBuffer;
    juce::AudioBuffer<float> previewDryBuffer;
    juce::AudioBuffer<float> previewRegionBuffer;
    juce::AudioBuffer<float> previewWeightBuffer;
    QQDeBreathEqProcessor vst3BreathEqProcessor;
    QQDeBreathEqProcessor vst3SibilanceEqProcessor;
    std::vector<QQDeBreathEqProcessor> vst3RegionEqProcessors;
    std::vector<int> vst3RegionEqTailBlocks;
    juce::int64 previewExpectedNextSample = -1;

    mutable juce::CriticalSection analysisLock;
    QQDeBreathBridgeAnalysisResult analysisResult;
    juce::Array<double> analysisRegionPeakCache;
    mutable juce::CriticalSection breathEqLock;
    QQDeBreathEqState breathEqState;
    mutable juce::CriticalSection sibilanceEqLock;
    QQDeBreathEqState sibilanceEqState;
    mutable juce::CriticalSection araSourceInfoLock;
    QQDeBreathARASourceInfo araSourceInfo;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(QQDeBreathAudioProcessor)
};

class QQDeBreathARADocumentController final : public juce::ARADocumentControllerSpecialisation
{
public:
    using juce::ARADocumentControllerSpecialisation::ARADocumentControllerSpecialisation;

    void upsertPersistentState(const QQDeBreathARASourceInfo& sourceInfo,
                               const QQDeBreathBridgeAnalysisResult& analysisResult,
                               const juce::Array<double>& regionPeakCache = {});
    void updateRuntimePersistentState(const QQDeBreathARASourceInfo& sourceInfo,
                                      const QQDeBreathBridgeAnalysisResult& analysisResult,
                                      const juce::Array<double>& regionPeakCache = {});
    bool getPersistentStateForSource(const juce::String& sourceFingerprint,
                                     QQDeBreathARAPersistentState& state) const;
    bool tryGetPersistentStateForSource(const juce::String& sourceFingerprint,
                                        QQDeBreathARAPersistentState& state,
                                        bool& found) const;
    bool getFirstPersistentState(QQDeBreathARAPersistentState& state) const;
    void setSourceAudioCache(const juce::String& sourceFingerprint,
                             double sampleRate,
                             QQDeBreathARASourceAudioBuffer audio);
    bool tryGetSourceAudioCache(const juce::String& sourceFingerprint,
                                QQDeBreathARASourceAudioBuffer& audio,
                                double& sampleRate,
                                bool& found) const;
    void setPlaybackParams(const QQDeBreathARAPlaybackParams& params);
    QQDeBreathARAPlaybackParams getPlaybackParams() const;
    bool tryGetPlaybackParams(QQDeBreathARAPlaybackParams& params) const;
    std::uint64_t getPersistentStateRevision() const noexcept { return persistentStateRevision.load(std::memory_order_acquire); }
    std::uint64_t getPlaybackParamsRevision() const noexcept { return playbackParamsRevision.load(std::memory_order_acquire); }
    std::uint64_t getSourceAudioCacheRevision() const noexcept { return sourceAudioCacheRevision.load(std::memory_order_acquire); }
    bool hasRestoredPlaybackParams() const noexcept { return restoredPlaybackParams.load(std::memory_order_acquire); }

protected:
    juce::ARAPlaybackRenderer* doCreatePlaybackRenderer() override;
    bool doRestoreObjectsFromStream(juce::ARAInputStream& input, const juce::ARARestoreObjectsFilter* filter) noexcept override;
    bool doStoreObjectsToStream(juce::ARAOutputStream& output, const juce::ARAStoreObjectsFilter* filter) noexcept override;

private:
    struct SourceAudioCacheEntry
    {
        juce::String sourceFingerprint;
        double sampleRate = 0.0;
        QQDeBreathARASourceAudioBuffer audio;
    };

    mutable juce::CriticalSection persistentStateLock;
    juce::Array<QQDeBreathARAPersistentState> persistentStates;
    mutable juce::CriticalSection playbackParamsLock;
    QQDeBreathARAPlaybackParams playbackParams;
    mutable juce::CriticalSection sourceAudioCacheLock;
    juce::Array<SourceAudioCacheEntry> sourceAudioCaches;
    std::atomic<std::uint64_t> persistentStateRevision { 1 };
    std::atomic<std::uint64_t> playbackParamsRevision { 1 };
    std::atomic<std::uint64_t> sourceAudioCacheRevision { 1 };
    std::atomic<bool> restoredPlaybackParams { false };
};
