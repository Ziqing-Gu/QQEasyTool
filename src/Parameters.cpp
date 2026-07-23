#include "Parameters.h"

namespace QQDeBreath
{

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { ParamIDs::bypass, 1 },
        "Bypass",
        false));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { ParamIDs::monitorVoice, 1 },
        "Monitor Voice",
        true));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { ParamIDs::monitorBreath, 1 },
        "Monitor Breath",
        true));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { ParamIDs::monitorNoize, 1 },
        "Monitor Noise",
        true));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { ParamIDs::monitorSibilance, 1 },
        "Monitor Sibilance",
        true));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { ParamIDs::monitorOthers, 1 },
        "Monitor Others",
        true));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { ParamIDs::followPlayhead, 1 },
        "Follow Playhead",
        false));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { ParamIDs::enableFade, 1 },
        "Fade",
        true));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { ParamIDs::normalizeBreath, 1 },
        "Breath Norm",
        false));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { ParamIDs::normalizeSibilance, 1 },
        "Sibilance Norm",
        false));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamIDs::breathTargetDb, 1 },
        "Breath Target dB",
        juce::NormalisableRange<float> { -80.0f, 0.0f, 0.1f },
        -6.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamIDs::sibilanceTargetDb, 1 },
        "Sibilance Target dB",
        juce::NormalisableRange<float> { -80.0f, 0.0f, 0.1f },
        -12.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamIDs::breathGainDb, 1 },
        "Global Breath Gain dB",
        juce::NormalisableRange<float> { -60.0f, 30.0f, 0.1f },
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamIDs::sibilanceGainDb, 1 },
        "Global Sibilance Gain dB",
        juce::NormalisableRange<float> { -60.0f, 30.0f, 0.1f },
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));


    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamIDs::fadeInMs, 1 },
        "Fade In ms",
        juce::NormalisableRange<float> { 0.0f, 200.0f, 0.1f },
        10.0f,
        juce::AudioParameterFloatAttributes().withLabel("ms")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamIDs::fadeOutMs, 1 },
        "Fade Out ms",
        juce::NormalisableRange<float> { 0.0f, 200.0f, 0.1f },
        10.0f,
        juce::AudioParameterFloatAttributes().withLabel("ms")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamIDs::waveformDisplayGain, 1 },
        "Waveform Display Gain",
        juce::NormalisableRange<float> { 0.25f, 8.0f, 0.05f },
        1.0f,
        juce::AudioParameterFloatAttributes().withLabel("x")));

    return { params.begin(), params.end() };
}

} // namespace QQDeBreath
