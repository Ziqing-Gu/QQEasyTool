#pragma once

#include <juce_core/juce_core.h>

#include <atomic>
#include <functional>

#include "shared/BreathEq.h"

inline juce::String qqNormalizedRegionType(const juce::String& type)
{
    if (type.equalsIgnoreCase("Breath"))
        return "Breath";
    // QQEasyTool 0.95 removed Sibilance as a region type. Preserve old projects
    // by treating their Sibilance regions as user-defined Others regions.
    if (type.equalsIgnoreCase("Sibilance") || type.equalsIgnoreCase("Sibilant"))
        return "Others";
    if (type.equalsIgnoreCase("Others") || type.equalsIgnoreCase("Other"))
        return "Others";
    if (type.equalsIgnoreCase("Noise") || type.equalsIgnoreCase("Noize"))
        return "Noise";
    return "Others";
}

inline bool qqRegionSupportsProcessing(const juce::String& type)
{
    return ! qqNormalizedRegionType(type).equalsIgnoreCase("Noise");
}

struct QQDeBreathBridgeRegion
{
    juce::String type;
    juce::int64 startSample = 0;
    juce::int64 endSample = 0;
    double startTime = 0.0;
    double endTime = 0.0;
    double gainDb = 0.0;
    QQDeBreathEqState eqState;
};

struct QQDeBreathBridgeAnalysisResult
{
    bool hasResult = false;
    bool succeeded = false;
    bool cancelled = false;
    juce::String sourceKey;
    juce::String status = "No analysis.";
    juce::String errorMessage;
    int schemaVersion = 0;
    int sampleRate = 0;
    int channels = 0;
    juce::int64 numSamples = 0;
    double durationSeconds = 0.0;
    int breathCount = 0;
    int noizeCount = 0;
    int sibilanceCount = 0;
    int othersCount = 0;
    juce::File resultJson;
    juce::File vocalOnlyFile;
    juce::File breathFile;
    juce::File noizeFile;
    juce::Array<QQDeBreathBridgeRegion> regions;
};

struct QQDeBreathBridgeAnalysisConfig
{
    juce::File bridgeExe;
    juce::File toolExe;
    juce::File inputWav;
    juce::File outputJson;
    juce::File outDir;
    juce::File cancelFile;
    juce::String sourceKey;
    double threshold = 0.86;
    double fadeMs = 10.0;
    double breathTargetDb = -6.0;
    double breathGainDb = 0.0;
    bool outputBreathRegions = true;
    bool outputSibilanceRegions = false;
    bool detectNoize = false;
    int timeoutMs = 10 * 60 * 1000;
};

class QQDeBreathBridgeAnalysis
{
public:
    using ShouldCancel = std::function<bool()>;

    static juce::File getDefaultBridgeExe();
    static juce::File getDefaultToolExe();
    static QQDeBreathBridgeAnalysisResult run(const QQDeBreathBridgeAnalysisConfig& config,
                                              const ShouldCancel& shouldCancel);
    static juce::String serializeResult(const QQDeBreathBridgeAnalysisResult& result);
    static bool deserializeResult(const juce::String& text, QQDeBreathBridgeAnalysisResult& result);

private:
    static bool parseResultJson(const juce::File& jsonFile,
                                const juce::String& sourceKey,
                                QQDeBreathBridgeAnalysisResult& result);
};
