#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace QQDeBreath
{
namespace ParamIDs
{
static constexpr auto bypass = "bypass";
static constexpr auto monitorVoice = "monitorVoice";
static constexpr auto monitorBreath = "monitorBreath";
static constexpr auto monitorNoize = "monitorNoise";
static constexpr auto monitorSibilance = "monitorSibilance";
static constexpr auto monitorOthers = "monitorOthers";
static constexpr auto followPlayhead = "followPlayhead";
static constexpr auto enableFade = "enableFade";
static constexpr auto normalizeBreath = "normalizeBreath";
static constexpr auto normalizeSibilance = "normalizeSibilance";
static constexpr auto breathTargetDb = "breathTargetDb";
static constexpr auto sibilanceTargetDb = "sibilanceTargetDb";
static constexpr auto breathGainDb = "breathGainDb";
static constexpr auto sibilanceGainDb = "sibilanceGainDb";
static constexpr auto fadeInMs = "fadeInMs";
static constexpr auto fadeOutMs = "fadeOutMs";
static constexpr auto waveformDisplayGain = "waveformDisplayGain";
} // namespace ParamIDs

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

} // namespace QQDeBreath
