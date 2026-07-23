#include "PluginProcessor.h"
#include "shared/WaveformEditorComponent.h"

#include <cmath>
#include <iostream>
#include <memory>

namespace
{
constexpr double sampleRate = 48000.0;
constexpr int blockSize = 4096;

void setParameter(QQDeBreathAudioProcessor& processor, const char* id, float value)
{
    auto* parameter = processor.parameters.getParameter(id);
    if (parameter == nullptr)
        throw std::runtime_error(std::string("Missing parameter: ") + id);

    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}

void fill(juce::AudioBuffer<float>& buffer, float value)
{
    for (auto channel = 0; channel < buffer.getNumChannels(); ++channel)
        juce::FloatVectorOperations::fill(buffer.getWritePointer(channel), value, buffer.getNumSamples());
}

double meanAbsolute(const juce::AudioBuffer<float>& buffer)
{
    auto sum = 0.0;
    for (auto channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (auto sample = 0; sample < buffer.getNumSamples(); ++sample)
            sum += std::abs(buffer.getSample(channel, sample));
    return sum / static_cast<double>(buffer.getNumChannels() * buffer.getNumSamples());
}

double peakAbsolute(const juce::AudioBuffer<float>& buffer)
{
    auto peak = 0.0;
    for (auto channel = 0; channel < buffer.getNumChannels(); ++channel)
        peak = juce::jmax(peak, static_cast<double>(buffer.getMagnitude(channel, 0, buffer.getNumSamples())));
    return peak;
}

double tailRms(const juce::AudioBuffer<float>& buffer, int startSample)
{
    auto sumSquares = 0.0;
    auto count = 0;
    for (auto channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (auto sample = startSample; sample < buffer.getNumSamples(); ++sample)
        {
            const auto value = static_cast<double>(buffer.getSample(channel, sample));
            sumSquares += value * value;
            ++count;
        }
    return count > 0 ? std::sqrt(sumSquares / count) : 0.0;
}

bool near(double actual, double expected, double tolerance = 0.001)
{
    return std::abs(actual - expected) <= tolerance;
}

QQDeBreathBridgeAnalysisResult makeResult(const juce::String& type,
                                          const QQDeBreathEqState& regionEq = {})
{
    QQDeBreathBridgeAnalysisResult result;
    result.hasResult = true;
    result.succeeded = true;
    result.sampleRate = static_cast<int>(sampleRate);
    result.channels = 2;
    result.numSamples = static_cast<juce::int64>(sampleRate);
    result.durationSeconds = 1.0;

    if (type.isNotEmpty())
    {
        QQDeBreathBridgeRegion region;
        region.type = type;
        region.startTime = 0.0;
        region.endTime = 1.0;
        region.startSample = 0;
        region.endSample = static_cast<juce::int64>(sampleRate);
        region.eqState = regionEq;
        result.regions.add(region);
        if (type == "Breath") result.breathCount = 1;
        if (type == "Others") result.othersCount = 1;
    }

    return result;
}

void setResult(QQDeBreathAudioProcessor& processor,
               const juce::String& type,
               const QQDeBreathEqState& regionEq = {})
{
    const auto result = makeResult(type, regionEq);
    processor.setAnalysisResult(result);
    juce::Array<double> peaks;
    if (! result.regions.isEmpty())
        peaks.add(0.25);
    processor.setAnalysisRegionPeakCache(peaks);
}

void requireNear(const char* name, double actual, double expected, double tolerance)
{
    if (std::abs(actual - expected) > tolerance)
    {
        std::cerr << name << " failed: actual=" << actual << " expected=" << expected << '\n';
        std::exit(1);
    }
    std::cout << name << " ok: " << actual << '\n';
}

void requireBelow(const char* name, double actual, double limit)
{
    if (actual >= limit)
    {
        std::cerr << name << " failed: actual=" << actual << " limit=" << limit << '\n';
        std::exit(1);
    }
    std::cout << name << " ok: " << actual << '\n';
}

void requireString(const char* name, const juce::String& actual, const juce::String& expected)
{
    if (actual != expected)
    {
        std::cerr << name << " failed: actual=" << actual << " expected=" << expected << '\n';
        std::exit(1);
    }
    std::cout << name << " ok: " << actual << '\n';
}

void requireTrue(const char* name, bool condition)
{
    if (! condition)
    {
        std::cerr << name << " failed\n";
        std::exit(1);
    }
    std::cout << name << " ok\n";
}
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    QQDeBreathAudioProcessor processor;

    {
        auto firstEditor = std::unique_ptr<juce::AudioProcessorEditor>(processor.createEditor());
        requireTrue("First editor creation", firstEditor != nullptr);

        setParameter(processor, QQDeBreath::ParamIDs::normalizeBreath, 1.0f);
        setParameter(processor, QQDeBreath::ParamIDs::breathTargetDb, -37.3f);
        setParameter(processor, QQDeBreath::ParamIDs::breathGainDb, 7.2f);

        QQDeBreathEqState expectedEq;
        expectedEq.enabled = true;
        expectedEq.highPassEnabled = true;
        expectedEq.highPassHz = 173.0;
        expectedEq.highPassSlopeDbPerOct = 24;
        expectedEq.bands[0].enabled = true;
        expectedEq.bands[0].frequencyHz = 1840.0;
        expectedEq.bands[0].gainDb = -4.7;
        expectedEq.bands[0].q = 1.8;
        processor.setBreathEqState(expectedEq);

        QQDeBreathEqState expectedRegionEq;
        expectedRegionEq.enabled = true;
        expectedRegionEq.lowPassEnabled = true;
        expectedRegionEq.lowPassHz = 9230.0;
        expectedRegionEq.lowPassSlopeDbPerOct = 36;
        expectedRegionEq.bands[1].enabled = true;
        expectedRegionEq.bands[1].frequencyHz = 3270.0;
        expectedRegionEq.bands[1].gainDb = 3.4;
        expectedRegionEq.bands[1].q = 2.1;

        auto editorStateResult = makeResult("Breath", expectedRegionEq);
        editorStateResult.regions.getReference(0).gainDb = -8.6;
        processor.setAnalysisResult(editorStateResult);

        firstEditor.reset();
        auto reopenedEditor = std::unique_ptr<juce::AudioProcessorEditor>(processor.createEditor());
        requireTrue("Reopened editor creation", reopenedEditor != nullptr);

        requireNear("Editor reopen Breath Norm",
                    processor.parameters.getRawParameterValue(QQDeBreath::ParamIDs::normalizeBreath)->load(),
                    1.0,
                    0.001);
        requireNear("Editor reopen Breath Target",
                    processor.parameters.getRawParameterValue(QQDeBreath::ParamIDs::breathTargetDb)->load(),
                    -37.3,
                    0.001);
        requireNear("Editor reopen Global Gain",
                    processor.parameters.getRawParameterValue(QQDeBreath::ParamIDs::breathGainDb)->load(),
                    7.2,
                    0.001);

        const auto actualEq = processor.getBreathEqState();
        requireTrue("Editor reopen Global EQ",
                    actualEq.enabled
                    && actualEq.highPassEnabled
                    && near(actualEq.highPassHz, 173.0)
                    && actualEq.highPassSlopeDbPerOct == 24
                    && actualEq.bands[0].enabled
                    && near(actualEq.bands[0].frequencyHz, 1840.0)
                    && near(actualEq.bands[0].gainDb, -4.7)
                    && near(actualEq.bands[0].q, 1.8));

        const auto actualAnalysis = processor.getAnalysisResult();
        requireTrue("Editor reopen selected region Gain/EQ",
                    actualAnalysis.regions.size() == 1
                    && near(actualAnalysis.regions[0].gainDb, -8.6)
                    && actualAnalysis.regions[0].eqState.enabled
                    && actualAnalysis.regions[0].eqState.lowPassEnabled
                    && near(actualAnalysis.regions[0].eqState.lowPassHz, 9230.0)
                    && actualAnalysis.regions[0].eqState.lowPassSlopeDbPerOct == 36
                    && actualAnalysis.regions[0].eqState.bands[1].enabled
                    && near(actualAnalysis.regions[0].eqState.bands[1].frequencyHz, 3270.0)
                    && near(actualAnalysis.regions[0].eqState.bands[1].gainDb, 3.4)
                    && near(actualAnalysis.regions[0].eqState.bands[1].q, 2.1));
    }

    // Keep the editor-recreation regression fixture isolated from the existing
    // audio-routing checks below.
    processor.setBreathEqState({});

    processor.prepareToPlay(sampleRate, blockSize);

    QQDeBreathARASourceInfo sourceInfo;
    processor.setRateAndBufferSizeDetails(sampleRate, blockSize);
    sourceInfo.name = "Route probe";
    sourceInfo.persistentId = "route-probe";
    sourceInfo.sourceFingerprint = "route-probe-fingerprint";
    sourceInfo.sampleRate = sampleRate;
    sourceInfo.channelCount = 2;
    sourceInfo.numSamples = static_cast<juce::int64>(sampleRate);
    sourceInfo.durationSeconds = 1.0;
    QQDeBreathARASourceInfo::PlaybackMapping mapping;
    mapping.sourceFingerprint = sourceInfo.sourceFingerprint;
    mapping.sourceSampleRate = sampleRate;
    mapping.sourceChannelCount = 2;
    mapping.sourceNumSamples = sourceInfo.numSamples;
    mapping.sourceStartSeconds = 0.0;
    mapping.playbackStartSeconds = 10.0;
    mapping.playbackEndSeconds = 11.0;
    sourceInfo.playbackMappings.add(mapping);
    processor.setAraSourceInfo(sourceInfo);

    setParameter(processor, QQDeBreath::ParamIDs::enableFade, 0.0f);
    setParameter(processor, QQDeBreath::ParamIDs::monitorVoice, 1.0f);
    setParameter(processor, QQDeBreath::ParamIDs::monitorBreath, 1.0f);
    setParameter(processor, QQDeBreath::ParamIDs::monitorOthers, 1.0f);
    setParameter(processor, QQDeBreath::ParamIDs::normalizeBreath, 0.0f);

    juce::AudioBuffer<float> buffer(2, blockSize);

    setResult(processor, "Breath");
    setParameter(processor, QQDeBreath::ParamIDs::breathGainDb, -6.0f);
    fill(buffer, 0.25f);
    if (! processor.renderAraInputBlockForTesting(buffer, 10.0))
        return 2;
    requireNear("Breath Gain", meanAbsolute(buffer), 0.25 * std::pow(10.0, -6.0 / 20.0), 1.0e-5);

    setParameter(processor, QQDeBreath::ParamIDs::breathGainDb, 0.0f);
    auto liveRegions = processor.getAnalysisResult().regions;
    liveRegions.getReference(0).gainDb = -12.0;
    processor.updateAnalysisRegionsPreservingCaches(liveRegions);
    fill(buffer, 0.25f);
    processor.renderAraInputBlockForTesting(buffer, 10.0);
    requireNear("Live Region Breath Gain", meanAbsolute(buffer),
                0.25 * std::pow(10.0, -12.0 / 20.0), 1.0e-5);

    setParameter(processor, QQDeBreath::ParamIDs::monitorBreath, 0.0f);
    fill(buffer, 0.25f);
    processor.renderAraInputBlockForTesting(buffer, 10.0);
    requireBelow("Breath monitor off", peakAbsolute(buffer), 1.0e-7);

    setParameter(processor, QQDeBreath::ParamIDs::monitorBreath, 1.0f);
    setParameter(processor, QQDeBreath::ParamIDs::breathGainDb, 0.0f);
    setParameter(processor, QQDeBreath::ParamIDs::normalizeBreath, 1.0f);
    setParameter(processor, QQDeBreath::ParamIDs::breathTargetDb, -6.0f);
    setResult(processor, "Breath");
    fill(buffer, 0.25f);
    processor.renderAraInputBlockForTesting(buffer, 10.0);
    requireNear("Breath Norm", meanAbsolute(buffer), std::pow(10.0, -6.0 / 20.0), 1.0e-5);

    setParameter(processor, QQDeBreath::ParamIDs::normalizeBreath, 0.0f);
    QQDeBreathEqState highPass;
    highPass.enabled = true;
    highPass.highPassEnabled = true;
    highPass.highPassHz = 1000.0;
    highPass.highPassSlopeDbPerOct = 12;
    processor.setBreathEqState(highPass);
    setResult(processor, "Breath");
    fill(buffer, 0.25f);
    processor.renderAraInputBlockForTesting(buffer, 10.0);
    requireBelow("Global Breath EQ", tailRms(buffer, blockSize / 2), 1.0e-4);

    processor.setBreathEqState({});
    setResult(processor, "Breath", highPass);
    fill(buffer, 0.25f);
    processor.renderAraInputBlockForTesting(buffer, 10.0);
    requireBelow("Region Breath EQ", tailRms(buffer, blockSize / 2), 1.0e-4);

    setResult(processor, "Others");
    setParameter(processor, QQDeBreath::ParamIDs::monitorOthers, 0.0f);
    fill(buffer, 0.25f);
    processor.renderAraInputBlockForTesting(buffer, 10.0);
    requireBelow("Others monitor off", peakAbsolute(buffer), 1.0e-7);

    setResult(processor, {});
    setParameter(processor, QQDeBreath::ParamIDs::monitorVoice, 0.0f);
    fill(buffer, 0.25f);
    processor.renderAraInputBlockForTesting(buffer, 10.0);
    requireBelow("Voice monitor off", peakAbsolute(buffer), 1.0e-7);

    fill(buffer, 0.25f);
    if (processor.renderAraInputBlockForTesting(buffer, 12.0))
    {
        std::cerr << "Timeline mapping failed: out-of-range block was processed\n";
        return 1;
    }
    requireNear("Timeline passthrough outside event", meanAbsolute(buffer), 0.25, 1.0e-7);

    QQDeBreathWaveformEditor waveform;
    waveform.setBounds(0, 0, 1000, 400);
    juce::AudioBuffer<float> waveformAudio(1, static_cast<int>(sampleRate));
    waveformAudio.clear();
    waveform.setAudioBuffer(waveformAudio, sampleRate, 1.0);
    auto waveformResult = makeResult("Breath");
    waveformResult.regions.getReference(0).endTime = 0.2;
    waveformResult.regions.getReference(0).endSample = static_cast<juce::int64>(sampleRate * 0.2);
    waveform.setAnalysisResult(waveformResult);

    const auto xAt = [](double seconds) { return static_cast<float>(10.0 + seconds * 980.0); };
    const auto mouseEvent = [&waveform](float x, int modifiers, float mouseDownX, bool dragged)
    {
        const auto now = juce::Time::getCurrentTime();
        return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),
                                { x, 100.0f },
                                juce::ModifierKeys(modifiers),
                                1.0f,
                                0.0f,
                                0.0f,
                                0.0f,
                                0.0f,
                                &waveform,
                                &waveform,
                                now,
                                { mouseDownX, 100.0f },
                                now,
                                1,
                                dragged);
    };

    waveform.mouseDown(mouseEvent(xAt(0.1), juce::ModifierKeys::rightButtonModifier, xAt(0.1), false));
    requireString("Right-click remembered region type", waveform.getCreationType(), "Others");

    const auto createModifiers = juce::ModifierKeys::leftButtonModifier | juce::ModifierKeys::shiftModifier;
    waveform.mouseDown(mouseEvent(xAt(0.4), createModifiers, xAt(0.4), false));
    waveform.mouseDrag(mouseEvent(xAt(0.6), createModifiers, xAt(0.4), true));
    waveform.mouseUp(mouseEvent(xAt(0.6), 0, xAt(0.4), true));
    requireString("Shift-draw reused remembered type",
                  waveform.getRegion(waveform.getSelectedRegionIndex()).type,
                  "Others");

    processor.releaseResources();
    std::cout << "All QQEasyTool 0.99 route checks passed.\n";
    return 0;
}
