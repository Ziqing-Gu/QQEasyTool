#pragma once

#include "shared/BridgeAnalysis.h"
#include <juce_audio_basics/juce_audio_basics.h>

class QQDeBreathNativeAnalysis
{
public:
    using ShouldCancel = QQDeBreathBridgeAnalysis::ShouldCancel;

    static QQDeBreathBridgeAnalysisResult run(const QQDeBreathBridgeAnalysisConfig& config,
                                              const ShouldCancel& shouldCancel);
    static QQDeBreathBridgeAnalysisResult runWithAudio(const QQDeBreathBridgeAnalysisConfig& config,
        const juce::AudioBuffer<float>& audio, double sampleRate, const ShouldCancel& shouldCancel);
};
