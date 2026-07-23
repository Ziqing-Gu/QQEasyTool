#pragma once

#include "shared/BridgeAnalysis.h"

class QQDeBreathNativeAnalysis
{
public:
    using ShouldCancel = QQDeBreathBridgeAnalysis::ShouldCancel;

    static QQDeBreathBridgeAnalysisResult run(const QQDeBreathBridgeAnalysisConfig& config,
                                              const ShouldCancel& shouldCancel);
};
