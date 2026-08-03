#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <array>
#include <limits>

namespace
{
constexpr auto stateMagic = "QQEasyToolVST3State";
constexpr int stateVersion = 7;
constexpr int exportBitDepth = 32;
constexpr int maxStateSamplesPerChannel = 48000 * 60 * 20; // Phase 6B guard: 20 minutes at 48 kHz.
constexpr double maxTimelineGapToFillSeconds = 60.0 * 30.0;
constexpr double initialRecordCapacitySeconds = 300.0;
constexpr double recordCapacityGrowSeconds = 300.0;

juce::String fnv1a64(const juce::String& text)
{
    constexpr std::uint64_t offset = 14695981039346656037ull;
    constexpr std::uint64_t prime = 1099511628211ull;

    auto hash = offset;
    const auto utf8 = text.toRawUTF8();

    for (auto* p = reinterpret_cast<const unsigned char*>(utf8); *p != 0; ++p)
    {
        hash ^= static_cast<std::uint64_t>(*p);
        hash *= prime;
    }

    return juce::String::toHexString(static_cast<juce::int64>(hash)).paddedLeft('0', 16);
}

juce::String buildRecordingFingerprint(double sampleRate, int channels, juce::int64 samples)
{
    if (sampleRate <= 0.0 || channels <= 0 || samples <= 0)
        return {};

    return fnv1a64("vst3-recorded|sr=" + juce::String(sampleRate, 8)
                 + "|ch=" + juce::String(channels)
                 + "|samples=" + juce::String(samples));
}

juce::String buildAraSourceFingerprint(const juce::ARAAudioSource& source)
{
    const auto id = juce::String::fromUTF8(source.getPersistentID().c_str());
    return fnv1a64(id + "|sr=" + juce::String(source.getSampleRate(), 8)
                 + "|ch=" + juce::String(static_cast<int>(source.getChannelCount()))
                 + "|samples=" + juce::String(static_cast<juce::int64>(source.getSampleCount())));
}

juce::String propString(const juce::var& object, const juce::Identifier& name)
{
    return object.getProperty(name, {}).toString();
}

juce::int64 propInt64(const juce::var& object, const juce::Identifier& name)
{
    const auto value = object.getProperty(name, {});
    if (value.isString())
        return value.toString().getLargeIntValue();

    return static_cast<juce::int64>(value);
}

double propDouble(const juce::var& object, const juce::Identifier& name)
{
    return static_cast<double>(object.getProperty(name, 0.0));
}

juce::var araSourceInfoToVar(const QQDeBreathARASourceInfo& info)
{
    auto* object = new juce::DynamicObject();
    object->setProperty("name", info.name);
    object->setProperty("persistent_id", info.persistentId);
    object->setProperty("source_fingerprint", info.sourceFingerprint);
    object->setProperty("is_composite", info.isComposite);
    object->setProperty("composite_playback_start_seconds", info.compositePlaybackStartSeconds);
    object->setProperty("composite_playback_end_seconds", info.compositePlaybackEndSeconds);
    object->setProperty("sample_rate", info.sampleRate);
    object->setProperty("channel_count", info.channelCount);
    object->setProperty("num_samples", juce::String(info.numSamples));
    object->setProperty("duration_seconds", info.durationSeconds);
    object->setProperty("exported_wav", info.exportedWav.getFullPathName());
    object->setProperty("export_status", info.exportStatus);

    juce::Array<juce::var> mappingArray;
    for (const auto& mapping : info.playbackMappings)
    {
        auto* item = new juce::DynamicObject();
        item->setProperty("source_name", mapping.sourceName);
        item->setProperty("persistent_id", mapping.persistentId);
        item->setProperty("source_fingerprint", mapping.sourceFingerprint);
        item->setProperty("source_sample_rate", mapping.sourceSampleRate);
        item->setProperty("source_channel_count", mapping.sourceChannelCount);
        item->setProperty("source_num_samples", juce::String(mapping.sourceNumSamples));
        item->setProperty("composite_start_seconds", mapping.compositeStartSeconds);
        item->setProperty("composite_end_seconds", mapping.compositeEndSeconds);
        item->setProperty("source_start_seconds", mapping.sourceStartSeconds);
        item->setProperty("playback_start_seconds", mapping.playbackStartSeconds);
        item->setProperty("playback_end_seconds", mapping.playbackEndSeconds);
        mappingArray.add(juce::var(item));
    }
    object->setProperty("playback_mappings", mappingArray);
    return juce::var(object);
}

bool araSourceInfoFromVar(const juce::var& value, QQDeBreathARASourceInfo& info)
{
    if (! value.isObject())
        return false;

    info.name = propString(value, "name");
    info.persistentId = propString(value, "persistent_id");
    info.sourceFingerprint = propString(value, "source_fingerprint");
    info.isComposite = static_cast<bool>(value.getProperty("is_composite", false));
    info.compositePlaybackStartSeconds = propDouble(value, "composite_playback_start_seconds");
    info.compositePlaybackEndSeconds = propDouble(value, "composite_playback_end_seconds");
    info.sampleRate = propDouble(value, "sample_rate");
    info.channelCount = static_cast<int>(value.getProperty("channel_count", 0));
    info.numSamples = propInt64(value, "num_samples");
    info.durationSeconds = propDouble(value, "duration_seconds");
    info.exportedWav = juce::File(propString(value, "exported_wav"));
    info.exportStatus = propString(value, "export_status");
    if (info.exportStatus.isEmpty())
        info.exportStatus = "ARA source restored from project.";

    info.playbackMappings.clear();
    if (auto* mappings = value.getProperty("playback_mappings", {}).getArray())
    {
        for (const auto& item : *mappings)
        {
            if (! item.isObject())
                continue;

            QQDeBreathARASourceInfo::PlaybackMapping mapping;
            mapping.sourceName = propString(item, "source_name");
            mapping.persistentId = propString(item, "persistent_id");
            mapping.sourceFingerprint = propString(item, "source_fingerprint");
            mapping.sourceSampleRate = propDouble(item, "source_sample_rate");
            mapping.sourceChannelCount = static_cast<int>(item.getProperty("source_channel_count", 0));
            mapping.sourceNumSamples = propInt64(item, "source_num_samples");
            mapping.compositeStartSeconds = propDouble(item, "composite_start_seconds");
            mapping.compositeEndSeconds = propDouble(item, "composite_end_seconds");
            mapping.sourceStartSeconds = propDouble(item, "source_start_seconds");
            mapping.playbackStartSeconds = propDouble(item, "playback_start_seconds");
            mapping.playbackEndSeconds = propDouble(item, "playback_end_seconds");
            info.playbackMappings.add(mapping);
        }
    }

    return info.sourceFingerprint.isNotEmpty() || info.persistentId.isNotEmpty();
}

juce::String serializeAraSourceInfo(const QQDeBreathARASourceInfo& info)
{
    if (info.sourceFingerprint.isEmpty() && info.persistentId.isEmpty() && ! info.exportedWav.existsAsFile())
        return {};

    return juce::JSON::toString(araSourceInfoToVar(info), false);
}

bool deserializeAraSourceInfo(const juce::String& text, QQDeBreathARASourceInfo& info)
{
    if (text.trim().isEmpty())
        return true;

    return araSourceInfoFromVar(juce::JSON::parse(text), info);
}

juce::var playbackParamsToVar(const QQDeBreathARAPlaybackParams& params)
{
    auto* object = new juce::DynamicObject();
    object->setProperty("monitor_voice", params.monitorVoice);
    object->setProperty("monitor_breath", params.monitorBreath);
    object->setProperty("monitor_noise", params.monitorNoize);
    object->setProperty("monitor_sibilance", params.monitorSibilance);
    object->setProperty("monitor_others", params.monitorOthers);
    object->setProperty("follow_playhead", params.followPlayhead);
    object->setProperty("enable_fade", params.enableFade);
    object->setProperty("normalize_breath", params.normalizeBreath);
    object->setProperty("normalize_sibilance", params.normalizeSibilance);
    object->setProperty("fade_in_ms", params.fadeInMs);
    object->setProperty("fade_out_ms", params.fadeOutMs);
    object->setProperty("breath_target_db", params.breathTargetDb);
    object->setProperty("sibilance_target_db", params.sibilanceTargetDb);
    object->setProperty("breath_gain_db", params.breathGainDb);
    object->setProperty("sibilance_gain_db", params.sibilanceGainDb);
    object->setProperty("waveform_display_gain", params.waveformDisplayGain);
    object->setProperty("breath_eq", serializeBreathEqState(params.breathEqState));
    object->setProperty("sibilance_eq", serializeBreathEqState(params.sibilanceEqState));
    return juce::var(object);
}

bool playbackParamsFromVar(const juce::var& value, QQDeBreathARAPlaybackParams& params)
{
    if (! value.isObject())
        return false;

    params.monitorVoice = static_cast<bool>(value.getProperty("monitor_voice", true));
    params.monitorBreath = static_cast<bool>(value.getProperty("monitor_breath", true));
    params.monitorNoize = static_cast<bool>(value.getProperty("monitor_noise", value.getProperty("monitor_noize", true)));
    params.monitorSibilance = static_cast<bool>(value.getProperty("monitor_sibilance", true));
    params.monitorOthers = static_cast<bool>(value.getProperty("monitor_others", true));
    params.followPlayhead = static_cast<bool>(value.getProperty("follow_playhead", false));
    params.enableFade = static_cast<bool>(value.getProperty("enable_fade", true));
    params.normalizeBreath = static_cast<bool>(value.getProperty("normalize_breath", false));
    params.normalizeSibilance = static_cast<bool>(value.getProperty("normalize_sibilance", false));
    params.fadeInMs = static_cast<double>(value.getProperty("fade_in_ms", 10.0));
    params.fadeOutMs = static_cast<double>(value.getProperty("fade_out_ms", 10.0));
    params.breathTargetDb = static_cast<double>(value.getProperty("breath_target_db", -6.0));
    params.sibilanceTargetDb = static_cast<double>(value.getProperty("sibilance_target_db", -12.0));
    params.breathGainDb = static_cast<double>(value.getProperty("breath_gain_db", 0.0));
    params.sibilanceGainDb = static_cast<double>(value.getProperty("sibilance_gain_db", 0.0));
    params.waveformDisplayGain = juce::jlimit(0.25, 8.0, static_cast<double>(value.getProperty("waveform_display_gain", 1.0)));
    QQDeBreathEqState restoredEq;
    if (deserializeBreathEqState(propString(value, "breath_eq"), restoredEq))
        params.breathEqState = restoredEq;
    if (deserializeBreathEqState(propString(value, "sibilance_eq"), restoredEq))
        params.sibilanceEqState = restoredEq;
    return true;
}

juce::String serializeAraPersistentStates(const juce::Array<QQDeBreathARAPersistentState>& states,
                                          const QQDeBreathARAPlaybackParams& playbackParams)
{
    auto* root = new juce::DynamicObject();
    root->setProperty("app", "QQEasyTool");
    root->setProperty("version", 1);
    root->setProperty("playback_params", playbackParamsToVar(playbackParams));

    juce::Array<juce::var> entries;
    for (const auto& state : states)
    {
        auto* entry = new juce::DynamicObject();
        entry->setProperty("source", araSourceInfoToVar(state.sourceInfo));
        entry->setProperty("analysis", QQDeBreathBridgeAnalysis::serializeResult(state.analysisResult));
        entry->setProperty("playback_params", playbackParamsToVar(state.playbackParams));
        juce::Array<juce::var> peaks;
        peaks.ensureStorageAllocated(state.regionPeakCache.size());
        for (const auto peak : state.regionPeakCache)
            peaks.add(peak);
        entry->setProperty("region_peaks", peaks);
        entries.add(juce::var(entry));
    }

    root->setProperty("entries", entries);
    return juce::JSON::toString(juce::var(root), false);
}

bool deserializeAraPersistentStates(const juce::String& text,
                                    juce::Array<QQDeBreathARAPersistentState>& states,
                                    QQDeBreathARAPlaybackParams& playbackParams)
{
    states.clear();

    if (text.trim().isEmpty())
        return true;

    const auto root = juce::JSON::parse(text);
    if (! root.isObject() || static_cast<int>(root.getProperty("version", 0)) != 1)
        return false;

    playbackParamsFromVar(root.getProperty("playback_params", {}), playbackParams);

    const auto entries = root.getProperty("entries", {});
    auto* array = entries.getArray();
    if (array == nullptr)
        return false;

    for (const auto& item : *array)
    {
        if (! item.isObject())
            continue;

        QQDeBreathARAPersistentState state;
        if (! araSourceInfoFromVar(item.getProperty("source", {}), state.sourceInfo))
            continue;

        state.playbackParams = playbackParams;
        playbackParamsFromVar(item.getProperty("playback_params", {}), state.playbackParams);
        QQDeBreathBridgeAnalysis::deserializeResult(propString(item, "analysis"), state.analysisResult);
        if (auto* peaks = item.getProperty("region_peaks", {}).getArray())
            for (const auto& peak : *peaks)
                state.regionPeakCache.add(static_cast<double>(peak));
        states.add(state);
    }

    return true;
}

bool playbackParamsEqual(const QQDeBreathARAPlaybackParams& a, const QQDeBreathARAPlaybackParams& b)
{
    constexpr auto epsilon = 1.0e-6;
    return a.monitorVoice == b.monitorVoice
        && a.monitorBreath == b.monitorBreath
        && a.monitorNoize == b.monitorNoize
        && a.monitorSibilance == b.monitorSibilance
        && a.monitorOthers == b.monitorOthers
        && a.followPlayhead == b.followPlayhead
        && a.enableFade == b.enableFade
        && a.normalizeBreath == b.normalizeBreath
        && a.normalizeSibilance == b.normalizeSibilance
        && std::abs(a.fadeInMs - b.fadeInMs) < epsilon
        && std::abs(a.fadeOutMs - b.fadeOutMs) < epsilon
        && std::abs(a.breathTargetDb - b.breathTargetDb) < epsilon
        && std::abs(a.sibilanceTargetDb - b.sibilanceTargetDb) < epsilon
        && std::abs(a.breathGainDb - b.breathGainDb) < epsilon
        && std::abs(a.sibilanceGainDb - b.sibilanceGainDb) < epsilon
        && std::abs(a.waveformDisplayGain - b.waveformDisplayGain) < epsilon
        && serializeBreathEqState(a.breathEqState) == serializeBreathEqState(b.breathEqState)
        && serializeBreathEqState(a.sibilanceEqState) == serializeBreathEqState(b.sibilanceEqState)
    ;
}

float rawParam(juce::AudioProcessorValueTreeState& parameters, const char* id, float fallback)
{
    if (auto* value = parameters.getRawParameterValue(id))
        return value->load();

    return fallback;
}

double dbToGain(double db)
{
    return std::pow(10.0, db / 20.0);
}

double regionWeight(juce::int64 sample,
                    juce::int64 start,
                    juce::int64 end,
                    int fadeInSamples,
                    int fadeOutSamples)
{
    if (sample < start || sample >= end)
        return 0.0;

    auto weight = 1.0;
    if (fadeInSamples > 0)
        weight = juce::jmin(weight, static_cast<double>(sample - start) / juce::jmax(1, fadeInSamples - 1));

    if (fadeOutSamples > 0)
        weight = juce::jmin(weight, static_cast<double>(end - 1 - sample) / juce::jmax(1, fadeOutSamples - 1));

    return juce::jlimit(0.0, 1.0, weight);
}

float sampleAt(const juce::AudioBuffer<float>& buffer, int channel, juce::int64 sample)
{
    if (sample < 0 || sample >= buffer.getNumSamples() || buffer.getNumChannels() <= 0)
        return 0.0f;

    return buffer.getSample(juce::jmin(channel, buffer.getNumChannels() - 1), static_cast<int>(sample));
}

juce::int64 regionStartSample(const QQDeBreathBridgeRegion& region, double sampleRate, int totalSamples)
{
    return juce::jlimit<juce::int64>(0,
                                     totalSamples,
                                     region.startSample > 0 ? region.startSample
                                                            : static_cast<juce::int64>(std::llround(region.startTime * sampleRate)));
}

juce::int64 regionEndSample(const QQDeBreathBridgeRegion& region, double sampleRate, int totalSamples)
{
    return juce::jlimit<juce::int64>(0,
                                     totalSamples,
                                     region.endSample > 0 ? region.endSample
                                                          : static_cast<juce::int64>(std::llround(region.endTime * sampleRate)));
}

bool hasAdjacentRegionBefore(const juce::Array<QQDeBreathBridgeRegion>& regions,
                             int regionIndex,
                             juce::int64 start,
                             int fadeSamples,
                             double sampleRate,
                             int totalSamples)
{
    const auto tolerance = juce::jmax<juce::int64>(1, fadeSamples);
    for (auto i = 0; i < regions.size(); ++i)
    {
        if (i == regionIndex)
            continue;

        const auto otherEnd = regionEndSample(regions.getReference(i), sampleRate, totalSamples);
        const auto distance = otherEnd > start ? otherEnd - start : start - otherEnd;
        if (distance <= tolerance)
            return true;
    }

    return false;
}

bool hasAdjacentRegionAfter(const juce::Array<QQDeBreathBridgeRegion>& regions,
                            int regionIndex,
                            juce::int64 end,
                            int fadeSamples,
                            double sampleRate,
                            int totalSamples)
{
    const auto tolerance = juce::jmax<juce::int64>(1, fadeSamples);
    for (auto i = 0; i < regions.size(); ++i)
    {
        if (i == regionIndex)
            continue;

        const auto otherStart = regionStartSample(regions.getReference(i), sampleRate, totalSamples);
        const auto distance = otherStart > end ? otherStart - end : end - otherStart;
        if (distance <= tolerance)
            return true;
    }

    return false;
}

double regionWeightForIndex(const juce::Array<QQDeBreathBridgeRegion>& regions,
                            int regionIndex,
                            juce::int64 sample,
                            double sampleRate,
                            int totalSamples,
                            int fadeInSamples,
                            int fadeOutSamples)
{
    const auto& region = regions.getReference(regionIndex);
    const auto start = regionStartSample(region, sampleRate, totalSamples);
    const auto end = regionEndSample(region, sampleRate, totalSamples);
    const auto adjacentBefore = fadeInSamples > 0 && hasAdjacentRegionBefore(regions, regionIndex, start, fadeInSamples, sampleRate, totalSamples);
    const auto adjacentAfter = fadeOutSamples > 0 && hasAdjacentRegionAfter(regions, regionIndex, end, fadeOutSamples, sampleRate, totalSamples);

    if (sample >= start && sample < end)
    {
        auto weight = 1.0;
        if (fadeInSamples > 0 && ! adjacentBefore)
            weight = juce::jmin(weight, static_cast<double>(sample - start) / juce::jmax(1, fadeInSamples - 1));

        if (fadeOutSamples > 0 && ! adjacentAfter)
            weight = juce::jmin(weight, static_cast<double>(end - 1 - sample) / juce::jmax(1, fadeOutSamples - 1));

        return juce::jlimit(0.0, 1.0, weight);
    }

    if (adjacentBefore && sample >= start - fadeInSamples && sample < start)
        return juce::jlimit(0.0, 1.0, static_cast<double>(sample - (start - fadeInSamples)) / juce::jmax(1, fadeInSamples));

    if (adjacentAfter && sample >= end && sample < end + fadeOutSamples)
        return juce::jlimit(0.0, 1.0, 1.0 - static_cast<double>(sample - end) / juce::jmax(1, fadeOutSamples));

    return 0.0;
}

double preparedRegionWeight(juce::int64 sample,
                            juce::int64 start,
                            juce::int64 end,
                            int fadeInSamples,
                            int fadeOutSamples,
                            bool adjacentBefore,
                            bool adjacentAfter)
{
    if (sample >= start && sample < end)
    {
        auto weight = 1.0;
        if (fadeInSamples > 0 && ! adjacentBefore)
            weight = juce::jmin(weight, static_cast<double>(sample - start) / juce::jmax(1, fadeInSamples - 1));
        if (fadeOutSamples > 0 && ! adjacentAfter)
            weight = juce::jmin(weight, static_cast<double>(end - 1 - sample) / juce::jmax(1, fadeOutSamples - 1));
        return juce::jlimit(0.0, 1.0, weight);
    }

    if (adjacentBefore && sample >= start - fadeInSamples && sample < start)
        return juce::jlimit(0.0, 1.0, static_cast<double>(sample - (start - fadeInSamples)) / juce::jmax(1, fadeInSamples));
    if (adjacentAfter && sample >= end && sample < end + fadeOutSamples)
        return juce::jlimit(0.0, 1.0, 1.0 - static_cast<double>(sample - end) / juce::jmax(1, fadeOutSamples));
    return 0.0;
}

class QQDeBreathARAPlaybackRenderer final : public juce::ARAPlaybackRenderer
{
public:
    QQDeBreathARAPlaybackRenderer(ARA::PlugIn::DocumentController* documentController,
                                  QQDeBreathARADocumentController& ownerIn)
        : juce::ARAPlaybackRenderer(documentController),
          owner(ownerIn)
    {
    }

    void prepareToPlay(double sampleRateIn,
                       int maximumSamplesPerBlockIn,
                       int numChannelsIn,
                       juce::AudioProcessor::ProcessingPrecision precision,
                       AlwaysNonRealtime alwaysNonRealtime = AlwaysNonRealtime::no) override
    {
        juce::ARAPlaybackRenderer::prepareToPlay(sampleRateIn, maximumSamplesPerBlockIn, numChannelsIn, precision, alwaysNonRealtime);
        sampleRate = sampleRateIn;
        numChannels = numChannelsIn;
        tempBuffer.setSize(numChannels, maximumSamplesPerBlockIn);
        breathBuffer.setSize(numChannels, maximumSamplesPerBlockIn);
        sibilanceBuffer.setSize(numChannels, maximumSamplesPerBlockIn);
        othersBuffer.setSize(numChannels, maximumSamplesPerBlockIn);
        noiseBuffer.setSize(numChannels, maximumSamplesPerBlockIn);
        regionBuffer.setSize(numChannels, maximumSamplesPerBlockIn);
        regionWeightBuffer.setSize(1, maximumSamplesPerBlockIn);
        sourceReadBuffer.setSize(juce::jmax(2, numChannels), maximumSamplesPerBlockIn * 16 + 32);
        sourceCaches = {};
        nextSourceCache = 0;
        cachedPlaybackParams = owner.getPlaybackParams();
        cachedPlaybackParamsRevision = owner.getPlaybackParamsRevision();
    }

    bool processBlock(juce::AudioBuffer<float>& buffer,
                      juce::AudioProcessor::Realtime /*realtime*/,
                      const juce::AudioPlayHead::PositionInfo& positionInfo) noexcept override
    {
        buffer.clear();

        if (! positionInfo.getIsPlaying() || sampleRate <= 0.0)
            return true;

        juce::int64 blockStart = 0;
        if (const auto timeInSamples = positionInfo.getTimeInSamples(); timeInSamples.hasValue())
            blockStart = *timeInSamples;
        else if (const auto timeInSeconds = positionInfo.getTimeInSeconds(); timeInSeconds.hasValue())
            blockStart = static_cast<juce::int64>(std::llround(*timeInSeconds * sampleRate));
        else
            return true;

        const auto blockEnd = blockStart + buffer.getNumSamples();
        refreshPlaybackParams();

        if (tempBuffer.getNumChannels() != buffer.getNumChannels() || tempBuffer.getNumSamples() < buffer.getNumSamples())
            tempBuffer.setSize(buffer.getNumChannels(), buffer.getNumSamples(), false, false, true);

        for (const auto* playbackRegion : getPlaybackRegions<juce::ARAPlaybackRegion>())
        {
            if (playbackRegion == nullptr)
                continue;

            const auto* audioModification = playbackRegion->getAudioModification();
            if (audioModification == nullptr)
                continue;

            auto* audioSource = audioModification->getAudioSource<juce::ARAAudioSource>();
            if (audioSource == nullptr || audioSource->getSampleRate() <= 0.0)
                continue;

            const auto sourceSampleRate = audioSource->getSampleRate();
            const auto regionStart = playbackRegion->getStartInPlaybackSamples(sampleRate);
            const auto regionEnd = playbackRegion->getEndInPlaybackSamples(sampleRate);
            if (blockEnd <= regionStart || regionEnd <= blockStart)
                continue;

            const auto copyStartInSong = juce::jmax<juce::int64>(blockStart, regionStart);
            const auto copyEndInSong = juce::jmin<juce::int64>(blockEnd, regionEnd);
            if (copyEndInSong <= copyStartInSong)
                continue;

            const auto destOffset = static_cast<int>(copyStartInSong - blockStart);
            const auto numSamples = static_cast<int>(copyEndInSong - copyStartInSong);
            const auto sourceStep = sourceSampleRate / sampleRate;
            const auto copyStartTime = static_cast<double>(copyStartInSong) / sampleRate;
            const auto sourceStartPosition = (playbackRegion->getStartInAudioModificationTime()
                                            + copyStartTime
                                            - playbackRegion->getStartInPlaybackTime()) * sourceSampleRate;
            auto& sourceCache = getSourceCache(*audioSource);
            if (! renderSourceAudio(*audioSource, sourceCache, sourceStartPosition, sourceStep, numSamples))
                continue;
            const auto& state = sourceCache.state;
            const auto hasState = sourceCache.hasState;
            const auto& params = sourceCache.hasPersistentState
                               ? state.playbackParams
                               : cachedPlaybackParams;

            if (! hasState)
            {
                // Until analysis state is available, the entire source belongs to Voice.
                // Never force dry audio back in because that defeats the monitor switches.
                if (params.monitorVoice)
                    for (auto channel = 0; channel < buffer.getNumChannels(); ++channel)
                        buffer.addFrom(channel, destOffset, tempBuffer, juce::jmin(channel, tempBuffer.getNumChannels() - 1), 0, numSamples);
                continue;
            }

            const auto fadeInSamples = params.enableFade ? static_cast<int>(std::llround(params.fadeInMs * sourceSampleRate / 1000.0)) : 0;
            const auto fadeOutSamples = params.enableFade ? static_cast<int>(std::llround(params.fadeOutMs * sourceSampleRate / 1000.0)) : 0;
            for (auto* component : { &breathBuffer, &sibilanceBuffer, &othersBuffer, &noiseBuffer })
                for (auto channel = 0; channel < component->getNumChannels(); ++channel)
                    component->clear(channel, 0, numSamples);
            regionWeightBuffer.clear(0, 0, numSamples);

            const auto regionCount = static_cast<size_t>(state.analysisResult.regions.size());
            if (sourceCache.regionEqProcessors.size() != regionCount)
            {
                sourceCache.regionEqProcessors.clear();
                sourceCache.regionEqProcessors.resize(regionCount);
                sourceCache.regionEqTailBlocks.assign(regionCount, 0);
                sourceCache.expectedNextSourcePosition = -1.0;
            }

            const auto discontinuityTolerance = juce::jmax(2.0, std::abs(sourceStep) * 2.0);
            const auto discontinuity = sourceCache.expectedNextSourcePosition >= 0.0
                                    && (std::abs(sourceStartPosition - sourceCache.expectedNextSourcePosition) > discontinuityTolerance
                                        || std::abs(sourceStep - sourceCache.expectedSourceStep) > 1.0e-6);
            if (discontinuity)
            {
                sourceCache.breathEqProcessor.reset();
                sourceCache.sibilanceEqProcessor.reset();
                for (auto& processor : sourceCache.regionEqProcessors)
                    processor.reset();
                std::fill(sourceCache.regionEqTailBlocks.begin(), sourceCache.regionEqTailBlocks.end(), 0);
            }
            sourceCache.expectedNextSourcePosition = sourceStartPosition + numSamples * sourceStep;
            sourceCache.expectedSourceStep = sourceStep;

            const auto totalSourceSamples = static_cast<int>(audioSource->getSampleCount());
            const auto sourceLastPosition = sourceStartPosition + juce::jmax(0, numSamples - 1) * sourceStep;
            const auto blockSourceStart = static_cast<juce::int64>(std::floor(std::min(sourceStartPosition, sourceLastPosition)));
            const auto blockSourceEnd = static_cast<juce::int64>(std::ceil(std::max(sourceStartPosition, sourceLastPosition))) + 1;

            for (auto regionIndex = 0; regionIndex < state.analysisResult.regions.size(); ++regionIndex)
            {
                const auto& region = state.analysisResult.regions.getReference(regionIndex);
                const auto type = qqNormalizedRegionType(region.type);
                auto& tailBlocks = sourceCache.regionEqTailBlocks[static_cast<size_t>(regionIndex)];
                const auto regionStart = regionStartSample(region, sourceSampleRate, totalSourceSamples);
                const auto regionEnd = regionEndSample(region, sourceSampleRate, totalSourceSamples);
                const auto potentialStart = regionStart - fadeInSamples;
                const auto potentialEnd = regionEnd + fadeOutSamples;
                const auto intersectsBlock = blockSourceEnd > potentialStart && blockSourceStart < potentialEnd;
                if (! intersectsBlock && tailBlocks <= 0)
                    continue;

                for (auto channel = 0; channel < regionBuffer.getNumChannels(); ++channel)
                    regionBuffer.clear(channel, 0, numSamples);

                auto regionContributed = false;
                const auto localGain = type == "Noise" ? 1.0 : dbToGain(juce::jlimit(-30.0, 30.0, region.gainDb));
                const auto globalGain = type == "Breath" ? dbToGain(juce::jlimit(-60.0, 30.0, params.breathGainDb))
                                      : type == "Sibilance" ? dbToGain(juce::jlimit(-60.0, 30.0, params.sibilanceGainDb)) : 1.0;
                auto normGain = 1.0;
                const auto shouldNormalize = (type == "Breath" && params.normalizeBreath)
                                          || (type == "Sibilance" && params.normalizeSibilance);
                if (shouldNormalize)
                {
                    const auto peak = regionIndex < state.regionPeakCache.size() ? state.regionPeakCache.getReference(regionIndex) : 0.0;
                    const auto target = dbToGain(type == "Breath" ? params.breathTargetDb : params.sibilanceTargetDb);
                    normGain = peak > 1.0e-9 ? target / peak : 1.0;
                }

                const auto adjacentBefore = intersectsBlock && fadeInSamples > 0
                                         && hasAdjacentRegionBefore(state.analysisResult.regions, regionIndex, regionStart,
                                                                    fadeInSamples, sourceSampleRate, totalSourceSamples);
                const auto adjacentAfter = intersectsBlock && fadeOutSamples > 0
                                        && hasAdjacentRegionAfter(state.analysisResult.regions, regionIndex, regionEnd,
                                                                  fadeOutSamples, sourceSampleRate, totalSourceSamples);
                if (intersectsBlock)
                {
                    for (auto sampleOffset = 0; sampleOffset < numSamples; ++sampleOffset)
                    {
                        const auto absoluteSourceSample = static_cast<juce::int64>(std::llround(sourceStartPosition + sampleOffset * sourceStep));
                        const auto weight = preparedRegionWeight(absoluteSourceSample, regionStart, regionEnd,
                                                                 fadeInSamples, fadeOutSamples, adjacentBefore, adjacentAfter);
                        if (weight <= 0.0)
                            continue;
                        regionContributed = true;
                        regionWeightBuffer.setSample(0, sampleOffset, juce::jlimit(0.0f, 1.0f, regionWeightBuffer.getSample(0, sampleOffset) + static_cast<float>(weight)));
                        for (auto channel = 0; channel < buffer.getNumChannels(); ++channel)
                        {
                            const auto dry = sampleAt(tempBuffer, juce::jmin(channel, tempBuffer.getNumChannels() - 1), sampleOffset);
                            regionBuffer.setSample(channel, sampleOffset, static_cast<float>(dry * weight * normGain * globalGain * localGain));
                        }
                    }
                }

                if (regionContributed)
                    tailBlocks = 4;
                else if (tailBlocks > 0)
                    --tailBlocks;
                else
                    continue;

                juce::AudioBuffer<float> activeRegionBuffer(regionBuffer.getArrayOfWritePointers(),
                                                            buffer.getNumChannels(),
                                                            0,
                                                            numSamples);
                if (type != "Noise")
                {
                    auto& processor = sourceCache.regionEqProcessors[static_cast<size_t>(regionIndex)];
                    processor.prepare(sampleRate, buffer.getNumChannels(), region.eqState);
                    processor.process(activeRegionBuffer);
                }

                auto* destination = type == "Breath" ? &breathBuffer
                                  : type == "Sibilance" ? &sibilanceBuffer
                                  : type == "Others" ? &othersBuffer
                                  : &noiseBuffer;
                for (auto channel = 0; channel < buffer.getNumChannels(); ++channel)
                    destination->addFrom(channel, 0, activeRegionBuffer, channel, 0, numSamples);
            }

            juce::AudioBuffer<float> activeBreathBuffer(breathBuffer.getArrayOfWritePointers(), buffer.getNumChannels(), 0, numSamples);
            juce::AudioBuffer<float> activeSibilanceBuffer(sibilanceBuffer.getArrayOfWritePointers(), buffer.getNumChannels(), 0, numSamples);
            sourceCache.breathEqProcessor.prepare(sampleRate, buffer.getNumChannels(), params.breathEqState);
            sourceCache.sibilanceEqProcessor.prepare(sampleRate, buffer.getNumChannels(), params.sibilanceEqState);
            sourceCache.breathEqProcessor.process(activeBreathBuffer);
            sourceCache.sibilanceEqProcessor.process(activeSibilanceBuffer);

            for (auto sampleOffset = 0; sampleOffset < numSamples; ++sampleOffset)
            {
                const auto voiceWeight = 1.0f - juce::jlimit(0.0f, 1.0f, regionWeightBuffer.getSample(0, sampleOffset));
                for (auto channel = 0; channel < buffer.getNumChannels(); ++channel)
                {
                    if (params.monitorVoice)
                        buffer.addSample(channel, destOffset + sampleOffset, tempBuffer.getSample(juce::jmin(channel, tempBuffer.getNumChannels() - 1), sampleOffset) * voiceWeight);
                }
            }
            for (auto channel = 0; channel < buffer.getNumChannels(); ++channel)
            {
                if (params.monitorNoize) buffer.addFrom(channel, destOffset, noiseBuffer, channel, 0, numSamples);
                if (params.monitorBreath) buffer.addFrom(channel, destOffset, breathBuffer, channel, 0, numSamples);
                if (params.monitorSibilance) buffer.addFrom(channel, destOffset, sibilanceBuffer, channel, 0, numSamples);
                if (params.monitorOthers) buffer.addFrom(channel, destOffset, othersBuffer, channel, 0, numSamples);
            }
        }

        return true;
    }

private:
    struct SourceRuntimeCache
    {
        juce::ARAAudioSource* source = nullptr;
        juce::String fingerprint;
        QQDeBreathARAPersistentState state;
        std::uint64_t revision = 0;
        bool hasState = false;
        bool hasPersistentState = false;
        QQDeBreathARASourceAudioBuffer audio;
        double audioSampleRate = 0.0;
        std::uint64_t audioRevision = 0;
        bool hasAudio = false;
        QQDeBreathEqProcessor breathEqProcessor;
        QQDeBreathEqProcessor sibilanceEqProcessor;
        std::vector<QQDeBreathEqProcessor> regionEqProcessors;
        std::vector<int> regionEqTailBlocks;
        double expectedNextSourcePosition = -1.0;
        double expectedSourceStep = 0.0;
    };

    void refreshPlaybackParams() noexcept
    {
        const auto revision = owner.getPlaybackParamsRevision();
        if (revision == cachedPlaybackParamsRevision)
            return;

        QQDeBreathARAPlaybackParams updated;
        if (owner.tryGetPlaybackParams(updated))
        {
            cachedPlaybackParams = updated;
            cachedPlaybackParamsRevision = revision;
        }
    }

    bool renderSourceAudio(juce::ARAAudioSource& source,
                           const SourceRuntimeCache& cache,
                           double sourceStartPosition,
                           double sourceStep,
                           int samplesToRender) noexcept
    {
        for (auto channel = 0; channel < tempBuffer.getNumChannels(); ++channel)
            tempBuffer.clear(channel, 0, samplesToRender);

        if (cache.hasAudio && cache.audio != nullptr)
        {
            const auto cachedSamples = cache.audio->getNumSamples();
            const auto cachedChannels = cache.audio->getNumChannels();
            if (cachedSamples <= 0 || cachedChannels <= 0)
                return false;

            for (auto channel = 0; channel < tempBuffer.getNumChannels(); ++channel)
            {
                const auto sourceChannel = juce::jmin(channel, cachedChannels - 1);
                const auto* sourceData = cache.audio->getReadPointer(sourceChannel);
                auto* destData = tempBuffer.getWritePointer(channel);

                for (auto sampleOffset = 0; sampleOffset < samplesToRender; ++sampleOffset)
                {
                    const auto position = sourceStartPosition + sampleOffset * sourceStep;
                    if (position < 0.0 || position >= static_cast<double>(cachedSamples))
                        continue;

                    const auto index0 = juce::jlimit(0, cachedSamples - 1, static_cast<int>(std::floor(position)));
                    const auto index1 = juce::jmin(cachedSamples - 1, index0 + 1);
                    const auto fraction = static_cast<float>(position - std::floor(position));
                    destData[sampleOffset] = sourceData[index0]
                                           + (sourceData[index1] - sourceData[index0]) * fraction;
                }
            }

            return true;
        }

        // The realtime callback never reads ARA source data. Audio is immutable and preloaded by the controller worker.
        return false;
    }
    SourceRuntimeCache& getSourceCache(juce::ARAAudioSource& source) noexcept
    {
        SourceRuntimeCache* cache = nullptr;

        for (auto& candidate : sourceCaches)
        {
            if (candidate.source == &source)
            {
                cache = &candidate;
                break;
            }
        }

        if (cache == nullptr)
        {
            for (auto& candidate : sourceCaches)
            {
                if (candidate.source == nullptr)
                {
                    cache = &candidate;
                    break;
                }
            }
        }

        if (cache == nullptr)
        {
            cache = &sourceCaches[nextSourceCache];
            nextSourceCache = (nextSourceCache + 1) % sourceCaches.size();
        }

        if (cache->source != &source)
        {
            *cache = {};
            cache->source = &source;
            cache->fingerprint = buildAraSourceFingerprint(source);
        }

        const auto revision = owner.getPersistentStateRevision();
        if (cache->revision != revision)
        {
            QQDeBreathARAPersistentState updated;
            auto found = false;
            if (owner.tryGetPersistentStateForSource(cache->fingerprint, updated, found))
            {
                cache->state = found ? std::move(updated) : QQDeBreathARAPersistentState {};
                cache->hasPersistentState = found;
                cache->hasState = found && cache->state.analysisResult.succeeded;
                cache->revision = revision;
                cache->breathEqProcessor.reset();
                cache->sibilanceEqProcessor.reset();
                cache->regionEqProcessors.clear();
                cache->regionEqTailBlocks.clear();
                cache->expectedNextSourcePosition = -1.0;
                cache->expectedSourceStep = 0.0;
            }
        }

        const auto audioRevision = owner.getSourceAudioCacheRevision();
        if (cache->audioRevision != audioRevision)
        {
            QQDeBreathARASourceAudioBuffer updatedAudio;
            auto updatedSampleRate = 0.0;
            auto found = false;
            if (owner.tryGetSourceAudioCache(cache->fingerprint, updatedAudio, updatedSampleRate, found))
            {
                cache->audio = found ? std::move(updatedAudio) : QQDeBreathARASourceAudioBuffer {};
                cache->audioSampleRate = found ? updatedSampleRate : 0.0;
                cache->hasAudio = found && cache->audio != nullptr;
                cache->audioRevision = audioRevision;
            }
        }

        return *cache;
    }

    QQDeBreathARADocumentController& owner;
    double sampleRate = 0.0;
    int numChannels = 0;
    juce::AudioBuffer<float> tempBuffer;
    juce::AudioBuffer<float> sourceReadBuffer;
    juce::AudioBuffer<float> breathBuffer;
    juce::AudioBuffer<float> sibilanceBuffer;
    juce::AudioBuffer<float> othersBuffer;
    juce::AudioBuffer<float> noiseBuffer;
    juce::AudioBuffer<float> regionBuffer;
    juce::AudioBuffer<float> regionWeightBuffer;
    std::array<SourceRuntimeCache, 32> sourceCaches {};
    size_t nextSourceCache = 0;
    QQDeBreathARAPlaybackParams cachedPlaybackParams;
    std::uint64_t cachedPlaybackParamsRevision = 0;
};

} // namespace

class QQDeBreathARADocumentController::CacheWarmupThread final : public juce::Thread
{
public:
    explicit CacheWarmupThread(QQDeBreathARADocumentController& ownerIn)
        : juce::Thread("QQEasyTool ARA source cache warmup"),
          owner(ownerIn)
    {
    }

    void trigger()
    {
        pending.store(true, std::memory_order_release);
        notify();
    }

    void run() override
    {
        while (! threadShouldExit())
        {
            wait(-1);
            if (threadShouldExit())
                break;

            if (! pending.exchange(false, std::memory_order_acq_rel))
                continue;

            for (auto attempt = 0; attempt < 120 && ! threadShouldExit(); ++attempt)
            {
                const auto complete = owner.warmSourceAudioCaches([this] { return threadShouldExit(); });
                if (complete)
                    break;

                wait(100);
            }
        }
    }

private:
    QQDeBreathARADocumentController& owner;
    std::atomic<bool> pending { false };
};

QQDeBreathARADocumentController::~QQDeBreathARADocumentController()
{
    std::unique_ptr<CacheWarmupThread> thread;
    {
        const juce::ScopedLock lock(cacheWarmupThreadLock);
        thread = std::move(cacheWarmupThread);
    }

    if (thread != nullptr)
        thread->stopThread(3000);
}

void QQDeBreathARADocumentController::requestSourceAudioCacheWarmup()
{
    const juce::ScopedLock lock(cacheWarmupThreadLock);
    if (cacheWarmupThread == nullptr)
    {
        cacheWarmupThread = std::make_unique<CacheWarmupThread>(*this);
        cacheWarmupThread->startThread();
    }

    cacheWarmupThread->trigger();
}

bool QQDeBreathARADocumentController::warmSourceAudioCaches(
    const std::function<bool()>& shouldExit)
{
    juce::StringArray requiredFingerprints;
    {
        const juce::ScopedLock lock(persistentStateLock);
        for (const auto& state : persistentStates)
        {
            if (state.sourceInfo.sourceFingerprint.isNotEmpty())
                requiredFingerprints.addIfNotAlreadyThere(state.sourceInfo.sourceFingerprint);

            for (const auto& mapping : state.sourceInfo.playbackMappings)
                if (mapping.sourceFingerprint.isNotEmpty())
                    requiredFingerprints.addIfNotAlreadyThere(mapping.sourceFingerprint);
        }
    }

    if (requiredFingerprints.isEmpty())
        return true;

    auto* document = getDocumentController()->getDocument<juce::ARADocument>();
    if (document == nullptr)
        return false;

    const auto sources = document->getAudioSources<juce::ARAAudioSource>();
    for (auto* source : sources)
    {
        if (shouldExit())
            return false;

        if (source == nullptr)
            continue;

        const auto fingerprint = buildAraSourceFingerprint(*source);
        if (! requiredFingerprints.contains(fingerprint))
            continue;

        QQDeBreathARASourceAudioBuffer cachedAudio;
        auto cachedSampleRate = 0.0;
        auto found = false;
        if (tryGetSourceAudioCache(fingerprint, cachedAudio, cachedSampleRate, found) && found)
            continue;

        if (! source->isSampleAccessEnabled())
            continue;

        const auto sampleRate = source->getSampleRate();
        const auto channels = static_cast<int>(source->getChannelCount());
        const auto samples64 = static_cast<juce::int64>(source->getSampleCount());
        if (sampleRate <= 0.0 || channels <= 0 || samples64 <= 0
            || samples64 > static_cast<juce::int64>(std::numeric_limits<int>::max()))
            continue;

        auto audio = std::make_shared<juce::AudioBuffer<float>>(channels, static_cast<int>(samples64));
        audio->clear();

        juce::ARAAudioSourceReader reader(source);
        if (! reader.isValid())
            continue;

        constexpr juce::int64 blockSize = 65536;
        auto failed = false;
        for (juce::int64 position = 0; position < samples64;)
        {
            if (shouldExit())
                return false;

            const auto samplesThisBlock = static_cast<int>(juce::jmin(blockSize, samples64 - position));
            if (! reader.read(audio.get(),
                              static_cast<int>(position),
                              samplesThisBlock,
                              position,
                              true,
                              true))
            {
                failed = true;
                break;
            }

            position += samplesThisBlock;
        }

        if (! failed)
            setSourceAudioCache(fingerprint, sampleRate, audio);
    }

    for (const auto& fingerprint : requiredFingerprints)
    {
        QQDeBreathARASourceAudioBuffer cachedAudio;
        auto cachedSampleRate = 0.0;
        auto found = false;
        if (! tryGetSourceAudioCache(fingerprint, cachedAudio, cachedSampleRate, found) || ! found)
            return false;
    }

    return true;
}

QQDeBreathAudioProcessor::QQDeBreathAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "QQDeBreathParameters", QQDeBreath::createParameterLayout())
{
}

void QQDeBreathAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    prepareToPlayForARA(sampleRate, samplesPerBlock, getMainBusNumOutputChannels(), getProcessingPrecision());
    const auto outputChannels = getMainBusNumOutputChannels();
    previewBreathBuffer.setSize(outputChannels, samplesPerBlock);
    previewSibilanceBuffer.setSize(outputChannels, samplesPerBlock);
    previewOthersBuffer.setSize(outputChannels, samplesPerBlock);
    previewNoiseBuffer.setSize(outputChannels, samplesPerBlock);
    previewDryBuffer.setSize(outputChannels, samplesPerBlock);
    previewRegionBuffer.setSize(outputChannels, samplesPerBlock);
    previewWeightBuffer.setSize(1, samplesPerBlock);
    vst3BreathEqProcessor.reset();
    vst3SibilanceEqProcessor.reset();
    vst3RegionEqProcessors.clear();
    vst3RegionEqTailBlocks.clear();
    previewExpectedNextSample = -1;

    const juce::ScopedLock lock(recordedBufferLock);

    if (recordedSampleRate <= 0.0)
        recordedSampleRate = sampleRate;

    recordedPreviewReady.store(recordedBuffer.getNumChannels() > 0
                               && recordedLengthSamples > 0
                               && recordedSampleRate > 0.0
                               && recordingStartTimelineSeconds >= 0.0,
                               std::memory_order_release);
}

void QQDeBreathAudioProcessor::releaseResources()
{
    releaseResourcesForARA();
}

bool QQDeBreathAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& input = layouts.getMainInputChannelSet();
    const auto& output = layouts.getMainOutputChannelSet();

    if (input.isDisabled() || output.isDisabled())
        return false;

    if (input != output)
        return false;

    return input == juce::AudioChannelSet::mono() || input == juce::AudioChannelSet::stereo();
}

void QQDeBreathAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused(midiMessages);

    bool hostIsPlaying = false;
    double hostTimeSeconds = -1.0;
    auto* playHead = getPlayHead();
    if (playHead != nullptr)
    {
        if (auto position = playHead->getPosition())
        {
            hostIsPlaying = position->getIsPlaying();
            if (auto timeInSamples = position->getTimeInSamples(); timeInSamples.hasValue() && getSampleRate() > 0.0)
                hostTimeSeconds = static_cast<double>(*timeInSamples) / getSampleRate();
            else if (auto time = position->getTimeInSeconds())
                hostTimeSeconds = *time;
        }
    }

    if (isBoundToAraHost())
    {
        if (hasAraPlaybackRendererRole())
        {
            processBlockForARA(buffer, isRealtime(), playHead);
            return;
        }

        // Cubase may bind the editor role to an ordinary input/output processor
        // without assigning the ARA playback-renderer role to that same instance.
        // processBlockForARA() still reports success in that case, even though it
        // leaves the dry input untouched. Process the actual host input directly.
        processBlockForARA(buffer, isRealtime(), playHead);
        if (hostIsPlaying)
            renderAraInputBlock(buffer, hostTimeSeconds);
        return;
    }

    const auto armed = recordArmed.load(std::memory_order_acquire);
    const auto wasRecording = recording.load(std::memory_order_acquire);
    recording.store(armed && hostIsPlaying, std::memory_order_release);

    if (armed && hostIsPlaying)
        appendToRecordedBuffer(buffer, hostTimeSeconds);
    else if (armed && wasRecording && ! hostIsPlaying)
        recordArmed.store(false, std::memory_order_release);

    const auto totalInputChannels = getTotalNumInputChannels();
    const auto totalOutputChannels = getTotalNumOutputChannels();

    for (auto channel = totalInputChannels; channel < totalOutputChannels; ++channel)
        buffer.clear(channel, 0, buffer.getNumSamples());

    if (hostIsPlaying && ! armed && ! recording.load(std::memory_order_acquire))
    {
        if (renderPreviewBlock(buffer, hostTimeSeconds))
            return;
    }

    // If preview is not ready, leave existing input samples untouched for passthrough.
}

juce::AudioProcessorEditor* QQDeBreathAudioProcessor::createEditor()
{
    return new QQDeBreathAudioProcessorEditor(*this);
}

juce::Array<juce::ARAPlaybackRegion*> QQDeBreathAudioProcessor::getAssignedAraPlaybackRegions() const
{
    juce::Array<juce::ARAPlaybackRegion*> regions;
    if (auto* renderer = getPlaybackRenderer())
        for (auto* region : renderer->getPlaybackRegions<juce::ARAPlaybackRegion>())
            if (region != nullptr)
                regions.addIfNotAlreadyThere(region);
    return regions;
}

double QQDeBreathAudioProcessor::getTailLengthSeconds() const
{
    auto tail = 0.0;
    if (getTailLengthSecondsForARA(tail))
        return tail;

    return 0.0;
}

void QQDeBreathAudioProcessor::setCurrentProgram(int /*index*/)
{
}

const juce::String QQDeBreathAudioProcessor::getProgramName(int /*index*/)
{
    return {};
}

void QQDeBreathAudioProcessor::changeProgramName(int /*index*/, const juce::String& /*newName*/)
{
}

void QQDeBreathAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    const auto state = parameters.copyState();
    const auto xml = state.createXml();

    juce::MemoryOutputStream stream(destData, false);
    stream.writeString(stateMagic);
    stream.writeInt(stateVersion);
    stream.writeString(xml != nullptr ? xml->toString() : juce::String());

    {
        const auto captureActive = recordArmed.load(std::memory_order_acquire)
                                || recording.load(std::memory_order_acquire);
        const juce::ScopedLock lock(recordedBufferLock);
        const auto channels = recordedBuffer.getNumChannels();
        const auto samples = juce::jmin<juce::int64>(recordedLengthSamples, recordedBuffer.getNumSamples());
        const auto canPersist = ! captureActive
                             && recordedSampleRate > 0.0
                             && channels > 0
                             && samples > 0
                             && samples <= maxStateSamplesPerChannel;

        stream.writeBool(canPersist);
        stream.writeDouble(recordedSampleRate);
        stream.writeInt(channels);
        stream.writeInt(static_cast<int>(samples));
        stream.writeDouble(recordingStartTimelineSeconds);

        if (canPersist)
        {
            for (auto channel = 0; channel < channels; ++channel)
                stream.write(recordedBuffer.getReadPointer(channel), static_cast<size_t>(samples) * sizeof(float));
        }
    }

    {
        const juce::ScopedLock analysisScope(analysisLock);
        stream.writeString(QQDeBreathBridgeAnalysis::serializeResult(analysisResult));
    }

    {
        const juce::ScopedLock araScope(araSourceInfoLock);
        stream.writeString(serializeAraSourceInfo(araSourceInfo));
    }

    {
        const juce::ScopedLock eqScope(breathEqLock);
        stream.writeString(serializeBreathEqState(breathEqState));
    }

    {
        const juce::ScopedLock eqScope(sibilanceEqLock);
        stream.writeString(serializeBreathEqState(sibilanceEqState));
    }

}

void QQDeBreathAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    juce::MemoryInputStream stream(data, static_cast<size_t>(sizeInBytes), false);
    const auto magic = stream.readString();

    if (magic == stateMagic)
    {
        const auto version = stream.readInt();
        const auto xmlText = stream.readString();
        recordedPreviewReady.store(false, std::memory_order_release);
        analysisPreviewReady.store(false, std::memory_order_release);
        araInputPreviewReady.store(false, std::memory_order_release);

        if (auto xml = juce::XmlDocument::parse(xmlText))
        {
            if (xml->hasTagName(parameters.state.getType()))
                parameters.replaceState(juce::ValueTree::fromXml(*xml));
        }

        const auto hasRecording = stream.readBool();
        const auto sampleRate = stream.readDouble();
        const auto channels = stream.readInt();
        const auto samples = stream.readInt();
        const auto restoredStartTimelineSeconds = version >= 4 ? stream.readDouble() : -1.0;

        {
            const juce::ScopedLock lock(recordedBufferLock);
            recordArmed.store(false, std::memory_order_relaxed);
            recording.store(false, std::memory_order_relaxed);

            if (version >= 2 && hasRecording && sampleRate > 0.0 && channels > 0 && samples > 0 && samples <= maxStateSamplesPerChannel)
            {
                recordedSampleRate = sampleRate;
                recordingStartTimelineSeconds = restoredStartTimelineSeconds;
                recordedBuffer.setSize(channels, static_cast<int>(samples), false, false, true);
                recordedLengthSamples = samples;

                for (auto channel = 0; channel < channels; ++channel)
                {
                    auto* dest = recordedBuffer.getWritePointer(channel);
                    const auto bytesToRead = static_cast<size_t>(samples) * sizeof(float);

                    if (stream.read(dest, bytesToRead) != bytesToRead)
                    {
                        recordedBuffer.setSize(0, 0);
                        recordedLengthSamples = 0;
                        recordingStartTimelineSeconds = -1.0;
                        break;
                    }
                }
            }
            else
            {
                recordedSampleRate = sampleRate > 0.0 ? sampleRate : recordedSampleRate;
                recordedLengthSamples = 0;
                recordingStartTimelineSeconds = -1.0;
                recordedBuffer.setSize(0, 0);
            }

            recordedPreviewReady.store(recordedBuffer.getNumChannels() > 0
                                       && recordedLengthSamples > 0
                                       && recordedSampleRate > 0.0
                                       && recordingStartTimelineSeconds >= 0.0,
                                       std::memory_order_release);
        }

        if (version >= 3 && ! stream.isExhausted())
        {
            QQDeBreathBridgeAnalysisResult restored;
            if (QQDeBreathBridgeAnalysis::deserializeResult(stream.readString(), restored))
                setAnalysisResult(restored);
        }

        if (version >= 5 && ! stream.isExhausted())
        {
            QQDeBreathARASourceInfo restoredAraSource;
            if (deserializeAraSourceInfo(stream.readString(), restoredAraSource))
                setAraSourceInfo(restoredAraSource);
        }

        if (version >= 6 && ! stream.isExhausted())
        {
            QQDeBreathEqState restoredEq;
            if (deserializeBreathEqState(stream.readString(), restoredEq))
                setBreathEqState(restoredEq);
        }

        if (version >= 7 && ! stream.isExhausted())
        {
            QQDeBreathEqState restoredEq;
            if (deserializeBreathEqState(stream.readString(), restoredEq))
                setSibilanceEqState(restoredEq);
        }

        restoredStateInformation.store(true, std::memory_order_release);
        return;
    }

    const auto xml = getXmlFromBinary(data, sizeInBytes);

    if (xml != nullptr && xml->hasTagName(parameters.state.getType()))
    {
        parameters.replaceState(juce::ValueTree::fromXml(*xml));
        restoredStateInformation.store(true, std::memory_order_release);
    }
}

void QQDeBreathAudioProcessor::startRecording()
{
    auto hostTimeSeconds = -1.0;
    if (auto* playHead = getPlayHead())
    {
        if (const auto position = playHead->getPosition(); position.hasValue())
        {
            if (const auto timeInSamples = position->getTimeInSamples(); timeInSamples.hasValue() && getSampleRate() > 0.0)
                hostTimeSeconds = static_cast<double>(*timeInSamples) / getSampleRate();
            else if (const auto time = position->getTimeInSeconds())
                hostTimeSeconds = *time;
        }
    }

    const juce::ScopedLock lock(recordedBufferLock);
    recordedSampleRate = getSampleRate() > 0.0 ? getSampleRate() : recordedSampleRate;
    const auto inputChannels = juce::jmax(1, getTotalNumInputChannels());
    const auto sr = recordedSampleRate > 0.0 ? recordedSampleRate : 48000.0;
    const auto initialCapacity = static_cast<int>(std::ceil(initialRecordCapacitySeconds * sr));

    if (recordedLengthSamples <= 0)
    {
        recordedBuffer.setSize(inputChannels, juce::jmax(1, initialCapacity), false, true, true);
        recordedBuffer.clear();
        recordedLengthSamples = 0;
        recordingStartTimelineSeconds = -1.0;
    }
    else
    {
        const auto requiredCapacity = static_cast<int>(juce::jmin<juce::int64>(maxStateSamplesPerChannel,
                                                                               recordedLengthSamples + initialCapacity));
        recordedBuffer.setSize(inputChannels,
                               juce::jmax(recordedBuffer.getNumSamples(), requiredCapacity),
                               true,
                               true,
                               true);

        if (hostTimeSeconds >= 0.0 && recordingStartTimelineSeconds >= 0.0 && hostTimeSeconds < recordingStartTimelineSeconds)
        {
            const auto prependSamples = static_cast<int>(std::llround((recordingStartTimelineSeconds - hostTimeSeconds) * sr));
            if (prependSamples > 0 && recordedLengthSamples + prependSamples <= recordedBuffer.getNumSamples())
            {
                for (auto channel = 0; channel < recordedBuffer.getNumChannels(); ++channel)
                {
                    auto* data = recordedBuffer.getWritePointer(channel);
                    std::memmove(data + prependSamples,
                                 data,
                                 static_cast<size_t>(recordedLengthSamples) * sizeof(float));
                    juce::FloatVectorOperations::clear(data, prependSamples);
                }

                recordedLengthSamples += prependSamples;
                recordingStartTimelineSeconds = hostTimeSeconds;
            }
        }
    }

    droppedRecordBlocks.store(0, std::memory_order_relaxed);
    recording.store(false, std::memory_order_release);
    recordArmed.store(true, std::memory_order_release);
    clearInternalPreviewPosition();
    clearAnalysisResult();
}

void QQDeBreathAudioProcessor::stopRecording()
{
    recordArmed.store(false, std::memory_order_release);
    recording.store(false, std::memory_order_release);
}

void QQDeBreathAudioProcessor::clearRecording()
{
    recordArmed.store(false, std::memory_order_release);
    recording.store(false, std::memory_order_release);
    recordedPreviewReady.store(false, std::memory_order_release);
    analysisPreviewReady.store(false, std::memory_order_release);
    const juce::ScopedLock lock(recordedBufferLock);
    recordedBuffer.setSize(0, 0);
    recordedLengthSamples = 0;
    recordingStartTimelineSeconds = -1.0;
    droppedRecordBlocks.store(0, std::memory_order_relaxed);
    clearInternalPreviewPosition();
    clearAnalysisResult();
}

QQDeBreathAudioProcessor::RecordedBufferInfo QQDeBreathAudioProcessor::getRecordedBufferInfo() const
{
    RecordedBufferInfo info;
    info.isRecordArmed = recordArmed.load(std::memory_order_acquire);
    info.isRecording = recording.load(std::memory_order_acquire);

    const juce::ScopedLock lock(recordedBufferLock);
    info.sampleRate = recordedSampleRate;
    info.channelCount = recordedBuffer.getNumChannels();
    info.numSamples = juce::jmin<juce::int64>(recordedLengthSamples, recordedBuffer.getNumSamples());
    info.durationSeconds = info.sampleRate > 0.0 ? static_cast<double>(info.numSamples) / info.sampleRate : 0.0;
    info.recordingStartTimelineSeconds = recordingStartTimelineSeconds;
    info.hasRecording = info.channelCount > 0 && info.numSamples > 0;
    info.sourceFingerprint = buildRecordingFingerprint(info.sampleRate, info.channelCount, info.numSamples);

    const auto dropped = droppedRecordBlocks.load(std::memory_order_relaxed);
    if (info.isRecording)
        info.status = "Recording while DAW is playing...";
    else if (info.isRecordArmed)
        info.status = "Record armed. Press play in the DAW to start capture.";
    else
        info.status = info.hasRecording ? "Recording available." : "No recorded buffer.";

    if (dropped > 0)
        info.status += " Dropped blocks: " + juce::String(dropped);

    return info;
}

bool QQDeBreathAudioProcessor::copyRecordedBuffer(juce::AudioBuffer<float>& dest, double& sampleRate) const
{
    const juce::ScopedLock lock(recordedBufferLock);
    const auto samples = juce::jmin<juce::int64>(recordedLengthSamples, recordedBuffer.getNumSamples());
    if (recordedBuffer.getNumChannels() <= 0 || samples <= 0 || recordedSampleRate <= 0.0)
        return false;

    dest.setSize(recordedBuffer.getNumChannels(), static_cast<int>(samples), false, false, true);
    for (auto channel = 0; channel < recordedBuffer.getNumChannels(); ++channel)
        dest.copyFrom(channel, 0, recordedBuffer, channel, 0, static_cast<int>(samples));
    sampleRate = recordedSampleRate;
    return true;
}

bool QQDeBreathAudioProcessor::tryCopyRecordedBuffer(juce::AudioBuffer<float>& dest, double& sampleRate) const
{
    const juce::CriticalSection::ScopedTryLockType lock(recordedBufferLock);
    if (! lock.isLocked())
        return false;

    const auto samples = juce::jmin<juce::int64>(recordedLengthSamples, recordedBuffer.getNumSamples());
    if (recordedBuffer.getNumChannels() <= 0 || samples <= 0 || recordedSampleRate <= 0.0)
        return false;

    dest.setSize(recordedBuffer.getNumChannels(), static_cast<int>(samples), false, false, true);
    for (auto channel = 0; channel < recordedBuffer.getNumChannels(); ++channel)
        dest.copyFrom(channel, 0, recordedBuffer, channel, 0, static_cast<int>(samples));
    sampleRate = recordedSampleRate;
    return true;
}

bool QQDeBreathAudioProcessor::exportRecordedBufferToTempWav(juce::File& exportedFile, juce::String& status) const
{
    auto outDir = getRecordingExportDirectory();
    if (! outDir.createDirectory())
    {
        status = "Could not create temp export directory: " + outDir.getFullPathName();
        return false;
    }

    const auto fileName = "QQDeBreath_recorded_"
                        + juce::Time::getCurrentTime().formatted("%Y%m%d_%H%M%S")
                        + ".wav";
    auto outFile = outDir.getChildFile(fileName);

    if (! exportRecordedBufferToWavUnchecked(outFile, status))
        return false;

    exportedFile = outFile;
    return true;
}

bool QQDeBreathAudioProcessor::exportRecordedBufferToWav(const juce::File& outputFile, juce::String& status) const
{
    return exportRecordedBufferToWavUnchecked(outputFile, status);
}

bool QQDeBreathAudioProcessor::exportRecordedBufferToWavUnchecked(const juce::File& outputFile, juce::String& status) const
{
    juce::AudioBuffer<float> copy;
    double sampleRate = 0.0;

    {
        const juce::ScopedLock lock(recordedBufferLock);
        const auto samples = juce::jmin<juce::int64>(recordedLengthSamples, recordedBuffer.getNumSamples());
        if (recordedBuffer.getNumChannels() <= 0 || samples <= 0 || recordedSampleRate <= 0.0)
        {
            status = "No recorded buffer to export.";
            return false;
        }

        copy.setSize(recordedBuffer.getNumChannels(), static_cast<int>(samples), false, false, true);
        for (auto channel = 0; channel < recordedBuffer.getNumChannels(); ++channel)
            copy.copyFrom(channel, 0, recordedBuffer, channel, 0, static_cast<int>(samples));
        sampleRate = recordedSampleRate;
    }

    auto outFile = outputFile;
    if (! outFile.getParentDirectory().createDirectory())
    {
        status = "Could not create export directory: " + outFile.getParentDirectory().getFullPathName();
        return false;
    }

    outFile.deleteFile();

    juce::WavAudioFormat wavFormat;
    std::unique_ptr<juce::FileOutputStream> stream(outFile.createOutputStream());
    if (stream == nullptr)
    {
        status = "Could not open temp wav for writing: " + outFile.getFullPathName();
        return false;
    }

    std::unique_ptr<juce::AudioFormatWriter> writer(wavFormat.createWriterFor(stream.get(),
                                                                              sampleRate,
                                                                              static_cast<unsigned int>(copy.getNumChannels()),
                                                                              exportBitDepth,
                                                                              {},
                                                                              0));
    if (writer == nullptr)
    {
        status = "Could not create wav writer.";
        return false;
    }
    stream.release();

    if (! writer->writeFromAudioSampleBuffer(copy, 0, copy.getNumSamples()))
    {
        status = "Failed while writing temp wav.";
        return false;
    }

    writer.reset();
    status = "Exported recorded wav: " + outFile.getFullPathName();
    return true;
}

void QQDeBreathAudioProcessor::setAnalysisResult(const QQDeBreathBridgeAnalysisResult& result)
{
    auto normalizedResult = result;
    normalizedResult.breathCount = 0;
    normalizedResult.noizeCount = 0;
    normalizedResult.sibilanceCount = 0;
    normalizedResult.othersCount = 0;
    for (auto& region : normalizedResult.regions)
    {
        region.type = qqNormalizedRegionType(region.type);
        if (region.type == "Breath")
            ++normalizedResult.breathCount;
        else if (region.type == "Noise")
            ++normalizedResult.noizeCount;
        else if (region.type == "Others")
            ++normalizedResult.othersCount;
    }

    const auto peakCache = buildRegionPeakCacheForResult(normalizedResult);

    {
        const juce::ScopedLock lock(analysisLock);
        analysisResult = normalizedResult;
        analysisRegionPeakCache = peakCache;
        vst3BreathEqProcessor.reset();
        vst3SibilanceEqProcessor.reset();
        vst3RegionEqProcessors.clear();
        vst3RegionEqTailBlocks.clear();
        previewExpectedNextSample = -1;
    }
    analysisPreviewReady.store(normalizedResult.succeeded, std::memory_order_release);
}


void QQDeBreathAudioProcessor::setAnalysisRegionPeakCache(const juce::Array<double>& peakCache)
{
    const juce::ScopedLock lock(analysisLock);
    analysisRegionPeakCache = peakCache;
    while (analysisRegionPeakCache.size() < analysisResult.regions.size())
        analysisRegionPeakCache.add(0.0);
    while (analysisRegionPeakCache.size() > analysisResult.regions.size())
        analysisRegionPeakCache.removeLast();
}
void QQDeBreathAudioProcessor::updateAnalysisRegionsPreservingCaches(const juce::Array<QQDeBreathBridgeRegion>& regions)
{
    const juce::ScopedLock lock(analysisLock);
    analysisResult.hasResult = true;
    analysisResult.succeeded = true;
    analysisResult.regions = regions;
    analysisResult.breathCount = 0;
    analysisResult.noizeCount = 0;
    analysisResult.sibilanceCount = 0;
    analysisResult.othersCount = 0;

    for (const auto& region : regions)
    {
        if (region.type.equalsIgnoreCase("Breath"))
            ++analysisResult.breathCount;
        else if (qqNormalizedRegionType(region.type).equalsIgnoreCase("Noise"))
            ++analysisResult.noizeCount;
        else if (qqNormalizedRegionType(region.type).equalsIgnoreCase("Sibilance"))
            ++analysisResult.sibilanceCount;
        else if (qqNormalizedRegionType(region.type).equalsIgnoreCase("Others"))
            ++analysisResult.othersCount;
    }

    vst3BreathEqProcessor.reset();
    vst3SibilanceEqProcessor.reset();
    vst3RegionEqProcessors.clear();
    vst3RegionEqTailBlocks.clear();
    previewExpectedNextSample = -1;
    analysisPreviewReady.store(true, std::memory_order_release);
}

QQDeBreathBridgeAnalysisResult QQDeBreathAudioProcessor::getAnalysisResult() const
{
    const juce::ScopedLock lock(analysisLock);
    return analysisResult;
}

void QQDeBreathAudioProcessor::clearAnalysisResult()
{
    analysisPreviewReady.store(false, std::memory_order_release);
    const juce::ScopedLock lock(analysisLock);
    analysisResult = {};
    analysisRegionPeakCache.clear();
    vst3BreathEqProcessor.reset();
    vst3SibilanceEqProcessor.reset();
    vst3RegionEqProcessors.clear();
    vst3RegionEqTailBlocks.clear();
    previewExpectedNextSample = -1;
}

void QQDeBreathAudioProcessor::setAraSourceInfo(const QQDeBreathARASourceInfo& info)
{
    const juce::ScopedLock lock(araSourceInfoLock);
    araSourceInfo = info;
    araInputPreviewReady.store(info.sourceFingerprint.isNotEmpty(), std::memory_order_release);
}

QQDeBreathARASourceInfo QQDeBreathAudioProcessor::getAraSourceInfo() const
{
    const juce::ScopedLock lock(araSourceInfoLock);
    return araSourceInfo;
}

void QQDeBreathAudioProcessor::clearAraSourceInfo()
{
    const juce::ScopedLock lock(araSourceInfoLock);
    araSourceInfo = {};
    araInputPreviewReady.store(false, std::memory_order_release);
}

QQDeBreathEqState QQDeBreathAudioProcessor::getBreathEqState() const
{
    const juce::ScopedLock lock(breathEqLock);
    return breathEqState;
}

void QQDeBreathAudioProcessor::setBreathEqState(const QQDeBreathEqState& state)
{
    const juce::ScopedLock lock(breathEqLock);
    breathEqState = sanitizeBreathEqState(state);
}

void QQDeBreathAudioProcessor::applyBreathEqToBuffer(juce::AudioBuffer<float>& buffer, double sampleRate) const
{
    QQDeBreathEqState state;
    {
        const juce::ScopedLock lock(breathEqLock);
        state = breathEqState;
    }

    if (! state.hasActiveProcessing() || buffer.getNumSamples() <= 0 || buffer.getNumChannels() <= 0 || sampleRate <= 0.0)
        return;

    QQDeBreathEqProcessor processor;
    processor.prepare(sampleRate, buffer.getNumChannels(), state);
    processor.process(buffer);
}

QQDeBreathEqState QQDeBreathAudioProcessor::getSibilanceEqState() const
{
    const juce::ScopedLock lock(sibilanceEqLock);
    return sibilanceEqState;
}

void QQDeBreathAudioProcessor::setSibilanceEqState(const QQDeBreathEqState& state)
{
    const juce::ScopedLock lock(sibilanceEqLock);
    sibilanceEqState = sanitizeBreathEqState(state);
}

void QQDeBreathAudioProcessor::applySibilanceEqToBuffer(juce::AudioBuffer<float>& buffer, double sampleRate) const
{
    const auto state = getSibilanceEqState();
    if (! state.hasActiveProcessing() || buffer.getNumSamples() <= 0 || buffer.getNumChannels() <= 0 || sampleRate <= 0.0)
        return;
    QQDeBreathEqProcessor processor;
    processor.prepare(sampleRate, buffer.getNumChannels(), state);
    processor.process(buffer);
}

void QQDeBreathAudioProcessor::setInternalPreviewPosition(double localSeconds, double hostTimeSeconds)
{
    const juce::ScopedLock lock(recordedBufferLock);
    const auto duration = recordedSampleRate > 0.0
                        ? static_cast<double>(recordedLengthSamples) / recordedSampleRate
                        : 0.0;
    internalPreviewAnchorLocalSeconds.store(juce::jlimit(0.0, juce::jmax(0.0, duration), localSeconds), std::memory_order_release);
    internalPreviewAnchorHostSeconds.store(hostTimeSeconds >= 0.0 ? hostTimeSeconds : recordingStartTimelineSeconds, std::memory_order_release);
    internalPreviewActive.store(true, std::memory_order_release);
}

void QQDeBreathAudioProcessor::setInternalPreviewLoopRange(double startSeconds, double endSeconds, double hostTimeSeconds)
{
    if (endSeconds <= startSeconds)
        return;

    internalPreviewLoopStartSeconds.store(juce::jmax(0.0, startSeconds), std::memory_order_release);
    internalPreviewLoopEndSeconds.store(juce::jmax(startSeconds, endSeconds), std::memory_order_release);
    internalPreviewLoopEnabled.store(true, std::memory_order_release);
    setInternalPreviewPosition(startSeconds, hostTimeSeconds);
}

void QQDeBreathAudioProcessor::clearInternalPreviewLoop()
{
    internalPreviewLoopEnabled.store(false, std::memory_order_release);
    internalPreviewLoopStartSeconds.store(0.0, std::memory_order_release);
    internalPreviewLoopEndSeconds.store(0.0, std::memory_order_release);
}

void QQDeBreathAudioProcessor::clearInternalPreviewPosition()
{
    internalPreviewActive.store(false, std::memory_order_release);
    internalPreviewLoopEnabled.store(false, std::memory_order_release);
    internalPreviewAnchorLocalSeconds.store(0.0, std::memory_order_release);
    internalPreviewAnchorHostSeconds.store(0.0, std::memory_order_release);
    internalPreviewLoopStartSeconds.store(0.0, std::memory_order_release);
    internalPreviewLoopEndSeconds.store(0.0, std::memory_order_release);
}

double QQDeBreathAudioProcessor::getInternalPreviewPosition(double hostTimeSeconds) const
{
    const juce::ScopedLock lock(recordedBufferLock);
    return getInternalPreviewPositionUnlocked(hostTimeSeconds);
}

void QQDeBreathAudioProcessor::appendToRecordedBuffer(const juce::AudioBuffer<float>& buffer, double hostTimeSeconds)
{
    const juce::CriticalSection::ScopedTryLockType lock(recordedBufferLock);
    if (! lock.isLocked())
    {
        droppedRecordBlocks.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    const auto inputChannels = juce::jmax(1, getTotalNumInputChannels());
    const auto samplesToWrite = buffer.getNumSamples();
    const auto sr = recordedSampleRate > 0.0 ? recordedSampleRate : 48000.0;

    if (recordedLengthSamples == 0 && hostTimeSeconds >= 0.0)
        recordingStartTimelineSeconds = hostTimeSeconds;

    if (recordedBuffer.getNumChannels() != inputChannels)
        recordedBuffer.setSize(inputChannels, recordedBuffer.getNumSamples(), true, true, true);

    juce::int64 writeStart = recordedLengthSamples;
    if (hostTimeSeconds >= 0.0 && recordingStartTimelineSeconds >= 0.0)
        writeStart = static_cast<juce::int64>(std::llround((hostTimeSeconds - recordingStartTimelineSeconds) * sr));

    if (writeStart < 0)
    {
        const auto prependSamples = static_cast<int>(-writeStart);
        const auto requiredWithPrepend = recordedLengthSamples + prependSamples + samplesToWrite;
        if (requiredWithPrepend > recordedBuffer.getNumSamples())
        {
            const auto growSamples = juce::jmax<int>(samplesToWrite, static_cast<int>(std::ceil(recordCapacityGrowSeconds * sr)));
            const auto newCapacity = static_cast<int>(juce::jmin<juce::int64>(maxStateSamplesPerChannel,
                                                                              requiredWithPrepend + growSamples));
            recordedBuffer.setSize(inputChannels, newCapacity, true, true, true);
        }

        if (recordedLengthSamples + prependSamples <= recordedBuffer.getNumSamples())
        {
            for (auto channel = 0; channel < inputChannels; ++channel)
            {
                auto* data = recordedBuffer.getWritePointer(channel);
                std::memmove(data + prependSamples,
                             data,
                             static_cast<size_t>(recordedLengthSamples) * sizeof(float));
                juce::FloatVectorOperations::clear(data, prependSamples);
            }

            recordedLengthSamples += prependSamples;
            recordingStartTimelineSeconds = hostTimeSeconds;
            writeStart = 0;
        }
    }

    writeStart = juce::jmax<juce::int64>(0, writeStart);
    const auto requiredSamples = writeStart + samplesToWrite;
    if (requiredSamples > maxStateSamplesPerChannel)
    {
        droppedRecordBlocks.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    if (requiredSamples > recordedBuffer.getNumSamples())
    {
        const auto growSamples = juce::jmax<int>(samplesToWrite, static_cast<int>(std::ceil(recordCapacityGrowSeconds * sr)));
        const auto newCapacity = static_cast<int>(juce::jmin<juce::int64>(maxStateSamplesPerChannel,
                                                                          requiredSamples + growSamples));
        recordedBuffer.setSize(inputChannels, newCapacity, true, true, true);
    }

    if (writeStart > recordedLengthSamples)
    {
        const auto gapSamples = static_cast<int>(writeStart - recordedLengthSamples);
        for (auto channel = 0; channel < inputChannels; ++channel)
            recordedBuffer.clear(channel, static_cast<int>(recordedLengthSamples), gapSamples);
    }

    for (auto channel = 0; channel < inputChannels; ++channel)
    {
        if (channel < buffer.getNumChannels())
            recordedBuffer.copyFrom(channel, static_cast<int>(writeStart), buffer, channel, 0, samplesToWrite);
        else
            recordedBuffer.clear(channel, static_cast<int>(writeStart), samplesToWrite);
    }

    recordedLengthSamples = juce::jmax(recordedLengthSamples, requiredSamples);
    recordedPreviewReady.store(recordedLengthSamples > 0
                               && recordedSampleRate > 0.0
                               && recordingStartTimelineSeconds >= 0.0,
                               std::memory_order_release);
}

bool QQDeBreathAudioProcessor::renderAraInputBlock(juce::AudioBuffer<float>& buffer, double hostTimeSeconds)
{
    const auto hostSampleRate = getSampleRate();
    if (hostTimeSeconds < 0.0 || hostSampleRate <= 0.0
        || rawParam(parameters, QQDeBreath::ParamIDs::bypass, 0.0f) >= 0.5f)
        return false;

    const auto protectedPreview = araInputPreviewReady.load(std::memory_order_acquire)
                               && analysisPreviewReady.load(std::memory_order_acquire);
    const juce::CriticalSection::ScopedTryLockType sourceScope(araSourceInfoLock);
    if (! sourceScope.isLocked())
    {
        if (protectedPreview)
        {
            buffer.clear();
            return true;
        }
        return false;
    }

    if (araSourceInfo.sourceFingerprint.isEmpty())
    {
        araInputPreviewReady.store(false, std::memory_order_release);
        return false;
    }

    const juce::CriticalSection::ScopedTryLockType analysisScope(analysisLock);
    if (! analysisScope.isLocked())
    {
        if (protectedPreview)
        {
            buffer.clear();
            return true;
        }
        return false;
    }

    const auto samples = buffer.getNumSamples();
    const auto channels = buffer.getNumChannels();
    if (samples <= 0 || channels <= 0)
        return false;

    const auto blockDuration = static_cast<double>(samples) / hostSampleRate;
    auto localStartSeconds = -1.0;

    if (araSourceInfo.isComposite)
    {
        if (hostTimeSeconds + blockDuration > araSourceInfo.compositePlaybackStartSeconds
            && hostTimeSeconds < araSourceInfo.compositePlaybackEndSeconds)
            localStartSeconds = hostTimeSeconds - araSourceInfo.compositePlaybackStartSeconds;
    }
    else
    {
        for (const auto& mapping : araSourceInfo.playbackMappings)
        {
            if (mapping.playbackEndSeconds <= mapping.playbackStartSeconds)
                continue;

            if (hostTimeSeconds + blockDuration > mapping.playbackStartSeconds
                && hostTimeSeconds < mapping.playbackEndSeconds)
            {
                localStartSeconds = mapping.sourceStartSeconds
                                  + hostTimeSeconds
                                  - mapping.playbackStartSeconds;
                break;
            }
        }
    }

    if (localStartSeconds < -blockDuration)
        return false;

    const auto monitorVoice = rawParam(parameters, QQDeBreath::ParamIDs::monitorVoice, 1.0f) >= 0.5f;
    const auto monitorNoise = rawParam(parameters, QQDeBreath::ParamIDs::monitorNoize, 1.0f) >= 0.5f;
    const auto monitorBreath = rawParam(parameters, QQDeBreath::ParamIDs::monitorBreath, 1.0f) >= 0.5f;
    const auto monitorSibilance = rawParam(parameters, QQDeBreath::ParamIDs::monitorSibilance, 1.0f) >= 0.5f;
    const auto monitorOthers = rawParam(parameters, QQDeBreath::ParamIDs::monitorOthers, 1.0f) >= 0.5f;

    if (previewDryBuffer.getNumChannels() != channels || previewDryBuffer.getNumSamples() < samples)
        previewDryBuffer.setSize(channels, samples, false, false, true);
    for (auto channel = 0; channel < channels; ++channel)
        previewDryBuffer.copyFrom(channel, 0, buffer, channel, 0, samples);

    if (! analysisResult.succeeded || ! analysisResult.hasResult)
    {
        if (! monitorVoice)
            buffer.clear();
        return true;
    }

    const auto sourceSampleRate = analysisResult.sampleRate > 0
                                ? static_cast<double>(analysisResult.sampleRate)
                                : araSourceInfo.sampleRate;
    if (sourceSampleRate <= 0.0)
        return false;

    const auto sourceSamples64 = analysisResult.numSamples > 0
                               ? analysisResult.numSamples
                               : araSourceInfo.numSamples;
    const auto totalSourceSamples = static_cast<int>(juce::jlimit<juce::int64>(
        1, static_cast<juce::int64>(0x7fffffff), sourceSamples64));
    const auto sourceStep = sourceSampleRate / hostSampleRate;
    const auto sourceStartPosition = localStartSeconds * sourceSampleRate;
    const auto blockStart = static_cast<juce::int64>(std::floor(sourceStartPosition));
    const auto sourceLastPosition = sourceStartPosition + juce::jmax(0, samples - 1) * sourceStep;
    const auto blockEnd = static_cast<juce::int64>(std::ceil(sourceLastPosition)) + 1;

    const auto enableFade = rawParam(parameters, QQDeBreath::ParamIDs::enableFade, 1.0f) >= 0.5f;
    const auto fadeInSamples = enableFade ? static_cast<int>(std::llround(
        rawParam(parameters, QQDeBreath::ParamIDs::fadeInMs, 10.0f) * sourceSampleRate / 1000.0)) : 0;
    const auto fadeOutSamples = enableFade ? static_cast<int>(std::llround(
        rawParam(parameters, QQDeBreath::ParamIDs::fadeOutMs, 10.0f) * sourceSampleRate / 1000.0)) : 0;
    const auto normalizeBreath = rawParam(parameters, QQDeBreath::ParamIDs::normalizeBreath, 0.0f) >= 0.5f;
    const auto normalizeSibilance = rawParam(parameters, QQDeBreath::ParamIDs::normalizeSibilance, 0.0f) >= 0.5f;
    const auto breathTarget = dbToGain(rawParam(parameters, QQDeBreath::ParamIDs::breathTargetDb, -6.0f));
    const auto sibilanceTarget = dbToGain(rawParam(parameters, QQDeBreath::ParamIDs::sibilanceTargetDb, -12.0f));
    const auto breathGlobalGain = dbToGain(juce::jlimit(-60.0, 30.0, static_cast<double>(
        rawParam(parameters, QQDeBreath::ParamIDs::breathGainDb, 0.0f))));
    const auto sibilanceGlobalGain = dbToGain(juce::jlimit(-60.0, 30.0, static_cast<double>(
        rawParam(parameters, QQDeBreath::ParamIDs::sibilanceGainDb, 0.0f))));
    const auto breathEq = getBreathEqState();
    const auto sibilanceEq = getSibilanceEqState();

    buffer.clear();
    for (auto* component : { &previewBreathBuffer, &previewSibilanceBuffer,
                             &previewOthersBuffer, &previewNoiseBuffer, &previewRegionBuffer })
    {
        if (component->getNumChannels() != channels || component->getNumSamples() < samples)
            component->setSize(channels, samples, false, false, true);
        for (auto channel = 0; channel < channels; ++channel)
            component->clear(channel, 0, samples);
    }
    if (previewWeightBuffer.getNumSamples() < samples)
        previewWeightBuffer.setSize(1, samples, false, false, true);
    previewWeightBuffer.clear(0, 0, samples);

    const auto regionCount = static_cast<size_t>(analysisResult.regions.size());
    if (vst3RegionEqProcessors.size() != regionCount)
    {
        vst3BreathEqProcessor.reset();
        vst3SibilanceEqProcessor.reset();
        vst3RegionEqProcessors.clear();
        vst3RegionEqProcessors.resize(regionCount);
        vst3RegionEqTailBlocks.assign(regionCount, 0);
        previewExpectedNextSample = -1;
    }

    const auto roundedBlockStart = static_cast<juce::int64>(std::llround(sourceStartPosition));
    const auto playbackDiscontinuity = previewExpectedNextSample >= 0
                                    && std::abs(roundedBlockStart - previewExpectedNextSample) > 2;
    if (playbackDiscontinuity)
    {
        vst3BreathEqProcessor.reset();
        vst3SibilanceEqProcessor.reset();
        for (auto& processor : vst3RegionEqProcessors)
            processor.reset();
        std::fill(vst3RegionEqTailBlocks.begin(), vst3RegionEqTailBlocks.end(), 0);
    }
    previewExpectedNextSample = static_cast<juce::int64>(std::llround(
        sourceStartPosition + samples * sourceStep));

    for (auto regionIndex = 0; regionIndex < analysisResult.regions.size(); ++regionIndex)
    {
        const auto& region = analysisResult.regions.getReference(regionIndex);
        const auto type = qqNormalizedRegionType(region.type);
        auto& tailBlocks = vst3RegionEqTailBlocks[static_cast<size_t>(regionIndex)];
        const auto regionStart = regionStartSample(region, sourceSampleRate, totalSourceSamples);
        const auto regionEnd = regionEndSample(region, sourceSampleRate, totalSourceSamples);
        const auto intersectsBlock = blockEnd > regionStart - fadeInSamples
                                  && blockStart < regionEnd + fadeOutSamples;
        if (! intersectsBlock && tailBlocks <= 0)
            continue;

        for (auto channel = 0; channel < channels; ++channel)
            previewRegionBuffer.clear(channel, 0, samples);

        auto contributed = false;
        const auto localGain = type == "Noise"
                             ? 1.0
                             : dbToGain(juce::jlimit(-30.0, 30.0, region.gainDb));
        const auto globalGain = type == "Breath" ? breathGlobalGain
                              : type == "Sibilance" ? sibilanceGlobalGain : 1.0;
        auto normGain = 1.0;
        if ((type == "Breath" && normalizeBreath)
            || (type == "Sibilance" && normalizeSibilance))
        {
            const auto peak = regionIndex < analysisRegionPeakCache.size()
                            ? analysisRegionPeakCache.getReference(regionIndex) : 0.0;
            const auto target = type == "Breath" ? breathTarget : sibilanceTarget;
            normGain = peak > 1.0e-9 ? target / peak : 1.0;
        }

        const auto adjacentBefore = intersectsBlock && fadeInSamples > 0
                                 && hasAdjacentRegionBefore(analysisResult.regions, regionIndex, regionStart,
                                                            fadeInSamples, sourceSampleRate, totalSourceSamples);
        const auto adjacentAfter = intersectsBlock && fadeOutSamples > 0
                                && hasAdjacentRegionAfter(analysisResult.regions, regionIndex, regionEnd,
                                                          fadeOutSamples, sourceSampleRate, totalSourceSamples);
        if (intersectsBlock)
        {
            for (auto i = 0; i < samples; ++i)
            {
                const auto sourceSample = static_cast<juce::int64>(std::llround(
                    sourceStartPosition + i * sourceStep));
                if (sourceSample < 0 || sourceSample >= totalSourceSamples)
                    continue;

                const auto weight = preparedRegionWeight(sourceSample, regionStart, regionEnd,
                                                         fadeInSamples, fadeOutSamples,
                                                         adjacentBefore, adjacentAfter);
                if (weight <= 0.0)
                    continue;

                contributed = true;
                previewWeightBuffer.setSample(0, i, juce::jlimit(
                    0.0f, 1.0f,
                    previewWeightBuffer.getSample(0, i) + static_cast<float>(weight)));
                for (auto channel = 0; channel < channels; ++channel)
                {
                    const auto dry = previewDryBuffer.getSample(channel, i);
                    previewRegionBuffer.setSample(channel, i, static_cast<float>(
                        dry * weight * normGain * globalGain * localGain));
                }
            }
        }

        if (contributed)
            tailBlocks = 4;
        else if (tailBlocks > 0)
            --tailBlocks;
        else
            continue;

        juce::AudioBuffer<float> activeRegionBuffer(
            previewRegionBuffer.getArrayOfWritePointers(), channels, 0, samples);
        if (type != "Noise")
        {
            auto& processor = vst3RegionEqProcessors[static_cast<size_t>(regionIndex)];
            processor.prepare(hostSampleRate, channels, region.eqState);
            processor.process(activeRegionBuffer);
        }

        auto* destination = type == "Breath" ? &previewBreathBuffer
                          : type == "Sibilance" ? &previewSibilanceBuffer
                          : type == "Others" ? &previewOthersBuffer
                          : &previewNoiseBuffer;
        for (auto channel = 0; channel < channels; ++channel)
            destination->addFrom(channel, 0, activeRegionBuffer, channel, 0, samples);
    }

    juce::AudioBuffer<float> activeBreathBuffer(
        previewBreathBuffer.getArrayOfWritePointers(), channels, 0, samples);
    juce::AudioBuffer<float> activeSibilanceBuffer(
        previewSibilanceBuffer.getArrayOfWritePointers(), channels, 0, samples);
    vst3BreathEqProcessor.prepare(hostSampleRate, channels, breathEq);
    vst3SibilanceEqProcessor.prepare(hostSampleRate, channels, sibilanceEq);
    vst3BreathEqProcessor.process(activeBreathBuffer);
    vst3SibilanceEqProcessor.process(activeSibilanceBuffer);

    for (auto i = 0; i < samples; ++i)
    {
        const auto voiceWeight = 1.0f - juce::jlimit(
            0.0f, 1.0f, previewWeightBuffer.getSample(0, i));
        for (auto channel = 0; channel < channels; ++channel)
        {
            if (monitorVoice)
                buffer.addSample(channel, i,
                                 previewDryBuffer.getSample(channel, i) * voiceWeight);
            if (monitorNoise)
                buffer.addSample(channel, i, previewNoiseBuffer.getSample(channel, i));
            if (monitorBreath)
                buffer.addSample(channel, i, previewBreathBuffer.getSample(channel, i));
            if (monitorSibilance)
                buffer.addSample(channel, i, previewSibilanceBuffer.getSample(channel, i));
            if (monitorOthers)
                buffer.addSample(channel, i, previewOthersBuffer.getSample(channel, i));
        }
    }

    return true;
}

bool QQDeBreathAudioProcessor::renderPreviewBlock(juce::AudioBuffer<float>& buffer, double hostTimeSeconds)
{
    if (hostTimeSeconds < 0.0 || rawParam(parameters, QQDeBreath::ParamIDs::bypass, 0.0f) >= 0.5f)
        return false;

    const auto protectedPreview = recordedPreviewReady.load(std::memory_order_acquire)
                               && analysisPreviewReady.load(std::memory_order_acquire);
    const juce::CriticalSection::ScopedTryLockType recordingLock(recordedBufferLock);
    if (! recordingLock.isLocked())
    {
        if (protectedPreview)
        {
            buffer.clear();
            return true;
        }
        return false;
    }

    if (recordedBuffer.getNumChannels() <= 0 || recordedLengthSamples <= 0 || recordedSampleRate <= 0.0 || recordingStartTimelineSeconds < 0.0)
    {
        recordedPreviewReady.store(false, std::memory_order_release);
        return false;
    }

    const juce::CriticalSection::ScopedTryLockType analysisScope(analysisLock);
    if (! analysisScope.isLocked())
    {
        if (protectedPreview)
        {
            buffer.clear();
            return true;
        }
        return false;
    }

    const auto monitorVoice = rawParam(parameters, QQDeBreath::ParamIDs::monitorVoice, 1.0f) >= 0.5f;
    const auto monitorNoise = rawParam(parameters, QQDeBreath::ParamIDs::monitorNoize, 1.0f) >= 0.5f;
    const auto monitorBreath = rawParam(parameters, QQDeBreath::ParamIDs::monitorBreath, 1.0f) >= 0.5f;
    const auto monitorSibilance = rawParam(parameters, QQDeBreath::ParamIDs::monitorSibilance, 1.0f) >= 0.5f;
    const auto monitorOthers = rawParam(parameters, QQDeBreath::ParamIDs::monitorOthers, 1.0f) >= 0.5f;
    const auto enableFade = rawParam(parameters, QQDeBreath::ParamIDs::enableFade, 1.0f) >= 0.5f;
    const auto fadeInSamples = enableFade ? static_cast<int>(std::llround(rawParam(parameters, QQDeBreath::ParamIDs::fadeInMs, 10.0f) * recordedSampleRate / 1000.0)) : 0;
    const auto fadeOutSamples = enableFade ? static_cast<int>(std::llround(rawParam(parameters, QQDeBreath::ParamIDs::fadeOutMs, 10.0f) * recordedSampleRate / 1000.0)) : 0;
    const auto normalizeBreath = rawParam(parameters, QQDeBreath::ParamIDs::normalizeBreath, 0.0f) >= 0.5f;
    const auto normalizeSibilance = rawParam(parameters, QQDeBreath::ParamIDs::normalizeSibilance, 0.0f) >= 0.5f;
    const auto breathTarget = dbToGain(rawParam(parameters, QQDeBreath::ParamIDs::breathTargetDb, -6.0f));
    const auto sibilanceTarget = dbToGain(rawParam(parameters, QQDeBreath::ParamIDs::sibilanceTargetDb, -12.0f));
    const auto breathGlobalGain = dbToGain(juce::jlimit(-60.0, 30.0, static_cast<double>(rawParam(parameters, QQDeBreath::ParamIDs::breathGainDb, 0.0f))));
    const auto sibilanceGlobalGain = dbToGain(juce::jlimit(-60.0, 30.0, static_cast<double>(rawParam(parameters, QQDeBreath::ParamIDs::sibilanceGainDb, 0.0f))));
    const auto breathEq = getBreathEqState();
    const auto sibilanceEq = getSibilanceEqState();
    const auto blockStart = static_cast<juce::int64>(std::llround(getInternalPreviewPositionUnlocked(hostTimeSeconds) * recordedSampleRate));

    buffer.clear();
    const auto channels = buffer.getNumChannels();
    const auto samples = buffer.getNumSamples();
    for (auto* component : { &previewBreathBuffer, &previewSibilanceBuffer, &previewOthersBuffer, &previewNoiseBuffer, &previewRegionBuffer })
    {
        if (component->getNumChannels() != channels || component->getNumSamples() < samples)
            component->setSize(channels, samples, false, false, true);
        for (auto channel = 0; channel < channels; ++channel)
            component->clear(channel, 0, samples);
    }
    if (previewWeightBuffer.getNumSamples() < samples)
        previewWeightBuffer.setSize(1, samples, false, false, true);
    previewWeightBuffer.clear(0, 0, samples);

    const auto regionCount = static_cast<size_t>(analysisResult.regions.size());
    if (vst3RegionEqProcessors.size() != regionCount)
    {
        vst3BreathEqProcessor.reset();
        vst3SibilanceEqProcessor.reset();
        vst3RegionEqProcessors.clear();
        vst3RegionEqProcessors.resize(regionCount);
        vst3RegionEqTailBlocks.assign(regionCount, 0);
        previewExpectedNextSample = -1;
    }

    const auto playbackDiscontinuity = previewExpectedNextSample >= 0
                                    && std::abs(blockStart - previewExpectedNextSample) > 2;
    if (playbackDiscontinuity)
    {
        vst3BreathEqProcessor.reset();
        vst3SibilanceEqProcessor.reset();
        for (auto& processor : vst3RegionEqProcessors)
            processor.reset();
        std::fill(vst3RegionEqTailBlocks.begin(), vst3RegionEqTailBlocks.end(), 0);
    }
    previewExpectedNextSample = blockStart + samples;

    const auto previewBlockEnd = blockStart + samples;
    const auto previewTotalSamples = static_cast<int>(recordedLengthSamples);
    for (auto regionIndex = 0; regionIndex < analysisResult.regions.size(); ++regionIndex)
    {
        const auto& region = analysisResult.regions.getReference(regionIndex);
        const auto type = qqNormalizedRegionType(region.type);
        auto& tailBlocks = vst3RegionEqTailBlocks[static_cast<size_t>(regionIndex)];
        const auto regionStart = regionStartSample(region, recordedSampleRate, previewTotalSamples);
        const auto regionEnd = regionEndSample(region, recordedSampleRate, previewTotalSamples);
        const auto potentialStart = regionStart - fadeInSamples;
        const auto potentialEnd = regionEnd + fadeOutSamples;
        const auto intersectsBlock = previewBlockEnd > potentialStart && blockStart < potentialEnd;
        if (! intersectsBlock && tailBlocks <= 0)
            continue;

        for (auto channel = 0; channel < channels; ++channel)
            previewRegionBuffer.clear(channel, 0, samples);

        auto contributed = false;
        const auto localGain = type == "Noise" ? 1.0 : dbToGain(juce::jlimit(-30.0, 30.0, region.gainDb));
        const auto globalGain = type == "Breath" ? breathGlobalGain : type == "Sibilance" ? sibilanceGlobalGain : 1.0;
        auto normGain = 1.0;
        if ((type == "Breath" && normalizeBreath) || (type == "Sibilance" && normalizeSibilance))
        {
            const auto peak = regionIndex < analysisRegionPeakCache.size() ? analysisRegionPeakCache.getReference(regionIndex) : 0.0;
            const auto target = type == "Breath" ? breathTarget : sibilanceTarget;
            normGain = peak > 1.0e-9 ? target / peak : 1.0;
        }

        const auto adjacentBefore = intersectsBlock && fadeInSamples > 0
                                 && hasAdjacentRegionBefore(analysisResult.regions, regionIndex, regionStart,
                                                            fadeInSamples, recordedSampleRate, previewTotalSamples);
        const auto adjacentAfter = intersectsBlock && fadeOutSamples > 0
                                && hasAdjacentRegionAfter(analysisResult.regions, regionIndex, regionEnd,
                                                          fadeOutSamples, recordedSampleRate, previewTotalSamples);
        if (intersectsBlock)
        {
            for (auto i = 0; i < samples; ++i)
            {
                const auto sourceSample = blockStart + i;
                if (sourceSample < 0 || sourceSample >= recordedLengthSamples)
                    continue;
                const auto weight = preparedRegionWeight(sourceSample, regionStart, regionEnd,
                                                         fadeInSamples, fadeOutSamples, adjacentBefore, adjacentAfter);
                if (weight <= 0.0)
                    continue;
                contributed = true;
                previewWeightBuffer.setSample(0, i, juce::jlimit(0.0f, 1.0f, previewWeightBuffer.getSample(0, i) + static_cast<float>(weight)));
                for (auto channel = 0; channel < channels; ++channel)
                {
                    const auto dry = sampleAt(recordedBuffer, channel, sourceSample);
                    previewRegionBuffer.setSample(channel, i, static_cast<float>(dry * weight * normGain * globalGain * localGain));
                }
            }
        }

        if (contributed)
            tailBlocks = 4;
        else if (tailBlocks > 0)
            --tailBlocks;
        else
            continue;

        juce::AudioBuffer<float> activeRegionBuffer(previewRegionBuffer.getArrayOfWritePointers(), channels, 0, samples);
        if (type != "Noise")
        {
            auto& processor = vst3RegionEqProcessors[static_cast<size_t>(regionIndex)];
            processor.prepare(recordedSampleRate, channels, region.eqState);
            processor.process(activeRegionBuffer);
        }

        auto* destination = type == "Breath" ? &previewBreathBuffer
                          : type == "Sibilance" ? &previewSibilanceBuffer
                          : type == "Others" ? &previewOthersBuffer
                          : &previewNoiseBuffer;
        for (auto channel = 0; channel < channels; ++channel)
            destination->addFrom(channel, 0, activeRegionBuffer, channel, 0, samples);
    }

    juce::AudioBuffer<float> activeBreathBuffer(previewBreathBuffer.getArrayOfWritePointers(), channels, 0, samples);
    juce::AudioBuffer<float> activeSibilanceBuffer(previewSibilanceBuffer.getArrayOfWritePointers(), channels, 0, samples);
    vst3BreathEqProcessor.prepare(recordedSampleRate, channels, breathEq);
    vst3SibilanceEqProcessor.prepare(recordedSampleRate, channels, sibilanceEq);
    vst3BreathEqProcessor.process(activeBreathBuffer);
    vst3SibilanceEqProcessor.process(activeSibilanceBuffer);

    for (auto i = 0; i < samples; ++i)
    {
        const auto sourceSample = blockStart + i;
        if (sourceSample < 0 || sourceSample >= recordedLengthSamples)
            continue;
        const auto voiceWeight = 1.0f - juce::jlimit(0.0f, 1.0f, previewWeightBuffer.getSample(0, i));
        for (auto channel = 0; channel < channels; ++channel)
        {
            if (monitorVoice) buffer.addSample(channel, i, sampleAt(recordedBuffer, channel, sourceSample) * voiceWeight);
            if (monitorNoise) buffer.addSample(channel, i, previewNoiseBuffer.getSample(channel, i));
            if (monitorBreath) buffer.addSample(channel, i, previewBreathBuffer.getSample(channel, i));
            if (monitorSibilance) buffer.addSample(channel, i, previewSibilanceBuffer.getSample(channel, i));
            if (monitorOthers) buffer.addSample(channel, i, previewOthersBuffer.getSample(channel, i));
        }
    }
    return true;
}
double QQDeBreathAudioProcessor::getInternalPreviewPositionUnlocked(double hostTimeSeconds) const
{
    auto localSeconds = hostTimeSeconds - recordingStartTimelineSeconds;

    if (internalPreviewActive.load(std::memory_order_acquire))
    {
        const auto anchorLocal = internalPreviewAnchorLocalSeconds.load(std::memory_order_acquire);
        const auto anchorHost = internalPreviewAnchorHostSeconds.load(std::memory_order_acquire);
        localSeconds = anchorLocal + (hostTimeSeconds >= 0.0 && anchorHost >= 0.0 ? hostTimeSeconds - anchorHost : 0.0);

        if (internalPreviewLoopEnabled.load(std::memory_order_acquire))
        {
            const auto loopStart = internalPreviewLoopStartSeconds.load(std::memory_order_acquire);
            const auto loopEnd = internalPreviewLoopEndSeconds.load(std::memory_order_acquire);
            const auto loopLength = loopEnd - loopStart;
            if (loopLength > 0.001 && localSeconds >= loopEnd)
                localSeconds = loopStart + std::fmod(localSeconds - loopStart, loopLength);
        }
    }

    return juce::jmax(0.0, localSeconds);
}

juce::Array<double> QQDeBreathAudioProcessor::buildRegionPeakCacheForResult(const QQDeBreathBridgeAnalysisResult& result) const
{
    juce::Array<double> peakCache;

    const juce::ScopedLock lock(recordedBufferLock);
    const auto totalSamples = juce::jmin<juce::int64>(recordedLengthSamples, recordedBuffer.getNumSamples());

    for (const auto& region : result.regions)
    {
        auto peak = 0.0f;

        if (qqNormalizedRegionType(region.type) != "Noise" && recordedSampleRate > 0.0 && totalSamples > 0)
        {
            const auto start = regionStartSample(region, recordedSampleRate, totalSamples);
            const auto end = regionEndSample(region, recordedSampleRate, totalSamples);

            for (auto channel = 0; channel < recordedBuffer.getNumChannels(); ++channel)
            {
                const auto* data = recordedBuffer.getReadPointer(channel);
                for (auto sample = start; sample < end; ++sample)
                    peak = juce::jmax(peak, std::abs(data[static_cast<int>(sample)]));
            }
        }

        peakCache.add(static_cast<double>(peak));
    }

    return peakCache;
}

juce::File QQDeBreathAudioProcessor::getRecordingExportDirectory()
{
    return juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getChildFile("QQEasyTool")
        .getChildFile("VST3")
        .getChildFile("Phase6B");
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new QQDeBreathAudioProcessor();
}

const ARA::ARAFactory* JUCE_CALLTYPE createARAFactory()
{
    return juce::ARADocumentControllerSpecialisation::createARAFactory<QQDeBreathARADocumentController>();
}

void QQDeBreathARADocumentController::upsertPersistentState(const QQDeBreathARASourceInfo& sourceInfo,
                                                            const QQDeBreathBridgeAnalysisResult& analysisResult,
                                                            const QQDeBreathARAPlaybackParams& playbackParams,
                                                            const juce::Array<double>& regionPeakCache)
{
    if (sourceInfo.sourceFingerprint.isEmpty())
        return;

    {
        const juce::ScopedLock lock(persistentStateLock);
        QQDeBreathARAPersistentState state;
        state.sourceInfo = sourceInfo;
        state.analysisResult = analysisResult;
        state.regionPeakCache = regionPeakCache;
        state.playbackParams = playbackParams;

        auto replaced = false;
        for (auto i = 0; i < persistentStates.size(); ++i)
        {
            if (persistentStates.getReference(i).sourceInfo.sourceFingerprint == sourceInfo.sourceFingerprint)
            {
                if (state.regionPeakCache.isEmpty()
                    && persistentStates.getReference(i).regionPeakCache.size() == analysisResult.regions.size())
                    state.regionPeakCache = persistentStates.getReference(i).regionPeakCache;
                persistentStates.set(i, state);
                replaced = true;
                break;
            }
        }

        if (! replaced)
            persistentStates.add(state);
    }

    persistentStateRevision.fetch_add(1, std::memory_order_release);
    if (auto* updateController = getDocumentController()->getHostModelUpdateController())
        updateController->notifyDocumentDataChanged();
}

void QQDeBreathARADocumentController::updateRuntimePersistentState(const QQDeBreathARASourceInfo& sourceInfo,
                                                                   const QQDeBreathBridgeAnalysisResult& analysisResult,
                                                                   const QQDeBreathARAPlaybackParams& playbackParams,
                                                                   const juce::Array<double>& regionPeakCache)
{
    if (sourceInfo.sourceFingerprint.isEmpty())
        return;

    {
        const juce::ScopedLock lock(persistentStateLock);
        QQDeBreathARAPersistentState state;
        state.sourceInfo = sourceInfo;
        state.analysisResult = analysisResult;
        state.regionPeakCache = regionPeakCache;
        state.playbackParams = playbackParams;

        auto replaced = false;
        for (auto i = 0; i < persistentStates.size(); ++i)
        {
            if (persistentStates.getReference(i).sourceInfo.sourceFingerprint == sourceInfo.sourceFingerprint)
            {
                if (state.regionPeakCache.isEmpty()
                    && persistentStates.getReference(i).regionPeakCache.size() == analysisResult.regions.size())
                    state.regionPeakCache = persistentStates.getReference(i).regionPeakCache;
                persistentStates.set(i, state);
                replaced = true;
                break;
            }
        }

        if (! replaced)
            persistentStates.add(state);
    }

    persistentStateRevision.fetch_add(1, std::memory_order_release);
}

void QQDeBreathARADocumentController::setPlaybackParamsForSource(
    const QQDeBreathARASourceInfo& sourceInfo,
    const QQDeBreathARAPlaybackParams& params)
{
    if (sourceInfo.sourceFingerprint.isEmpty())
        return;

    auto changed = false;
    {
        const juce::ScopedLock lock(persistentStateLock);
        auto found = false;
        for (auto& state : persistentStates)
        {
            if (state.sourceInfo.sourceFingerprint != sourceInfo.sourceFingerprint)
                continue;

            found = true;
            changed = ! playbackParamsEqual(state.playbackParams, params);
            if (changed)
                state.playbackParams = params;
            break;
        }

        if (! found)
        {
            QQDeBreathARAPersistentState state;
            state.sourceInfo = sourceInfo;
            state.playbackParams = params;
            persistentStates.add(state);
            changed = true;
        }
    }

    if (! changed)
        return;

    persistentStateRevision.fetch_add(1, std::memory_order_release);
    if (auto* updateController = getDocumentController()->getHostModelUpdateController())
        updateController->notifyDocumentDataChanged();
}

bool QQDeBreathARADocumentController::getPersistentStateForSource(const juce::String& sourceFingerprint,
                                                                  QQDeBreathARAPersistentState& state) const
{
    const juce::ScopedLock lock(persistentStateLock);

    for (const auto& item : persistentStates)
    {
        if (item.sourceInfo.sourceFingerprint == sourceFingerprint)
        {
            state = item;
            return true;
        }
    }

    return false;
}

bool QQDeBreathARADocumentController::tryGetPersistentStateForSource(
    const juce::String& sourceFingerprint,
    QQDeBreathARAPersistentState& state,
    bool& found) const
{
    const juce::CriticalSection::ScopedTryLockType lock(persistentStateLock);
    if (! lock.isLocked())
        return false;

    found = false;
    for (const auto& item : persistentStates)
    {
        if (item.sourceInfo.sourceFingerprint == sourceFingerprint)
        {
            state = item;
            found = true;
            break;
        }
    }

    return true;
}

bool QQDeBreathARADocumentController::getFirstPersistentState(QQDeBreathARAPersistentState& state) const
{
    const juce::ScopedLock lock(persistentStateLock);
    if (persistentStates.isEmpty())
        return false;

    state = persistentStates.getFirst();
    return true;
}

void QQDeBreathARADocumentController::setSourceAudioCache(
    const juce::String& sourceFingerprint,
    double sampleRate,
    QQDeBreathARASourceAudioBuffer audio)
{
    if (sourceFingerprint.isEmpty() || sampleRate <= 0.0 || audio == nullptr
        || audio->getNumChannels() <= 0 || audio->getNumSamples() <= 0)
        return;

    {
        const juce::ScopedLock lock(sourceAudioCacheLock);
        auto replaced = false;
        for (auto i = 0; i < sourceAudioCaches.size(); ++i)
        {
            if (sourceAudioCaches.getReference(i).sourceFingerprint == sourceFingerprint)
            {
                sourceAudioCaches.set(i, { sourceFingerprint, sampleRate, std::move(audio) });
                replaced = true;
                break;
            }
        }

        if (! replaced)
        {
            if (sourceAudioCaches.size() >= 32)
                sourceAudioCaches.remove(0);
            sourceAudioCaches.add({ sourceFingerprint, sampleRate, std::move(audio) });
        }
    }

    sourceAudioCacheRevision.fetch_add(1, std::memory_order_release);
}

bool QQDeBreathARADocumentController::tryGetSourceAudioCache(
    const juce::String& sourceFingerprint,
    QQDeBreathARASourceAudioBuffer& audio,
    double& sampleRate,
    bool& found) const
{
    const juce::CriticalSection::ScopedTryLockType lock(sourceAudioCacheLock);
    if (! lock.isLocked())
        return false;

    found = false;
    for (const auto& entry : sourceAudioCaches)
    {
        if (entry.sourceFingerprint == sourceFingerprint)
        {
            audio = entry.audio;
            sampleRate = entry.sampleRate;
            found = audio != nullptr;
            break;
        }
    }

    return true;
}
void QQDeBreathARADocumentController::setPlaybackParams(const QQDeBreathARAPlaybackParams& params)
{
    auto changed = false;
    {
        const juce::ScopedLock lock(playbackParamsLock);
        changed = ! playbackParamsEqual(playbackParams, params);
        if (changed)
            playbackParams = params;
    }

    if (changed)
    {
        playbackParamsRevision.fetch_add(1, std::memory_order_release);
        if (auto* updateController = getDocumentController()->getHostModelUpdateController())
            updateController->notifyDocumentDataChanged();
    }
}

QQDeBreathARAPlaybackParams QQDeBreathARADocumentController::getPlaybackParams() const
{
    const juce::ScopedLock lock(playbackParamsLock);
    return playbackParams;
}

bool QQDeBreathARADocumentController::tryGetPlaybackParams(QQDeBreathARAPlaybackParams& params) const
{
    const juce::CriticalSection::ScopedTryLockType lock(playbackParamsLock);
    if (! lock.isLocked())
        return false;

    params = playbackParams;
    return true;
}

bool QQDeBreathARADocumentController::doRestoreObjectsFromStream(juce::ARAInputStream& input,
                                                                 const juce::ARARestoreObjectsFilter* filter) noexcept
{
    juce::ignoreUnused(filter);

    auto restored = juce::Array<QQDeBreathARAPersistentState>();
    QQDeBreathARAPlaybackParams restoredParams;
    const auto payload = input.readString();
    if (! deserializeAraPersistentStates(payload, restored, restoredParams))
        return false;

    {
        const juce::ScopedLock lock(persistentStateLock);
        persistentStates = restored;
    }
    {
        const juce::ScopedLock lock(playbackParamsLock);
        playbackParams = restoredParams;
    }
    {
        const juce::ScopedLock lock(sourceAudioCacheLock);
        sourceAudioCaches.clear();
    }
    sourceAudioCacheRevision.fetch_add(1, std::memory_order_release);
    persistentStateRevision.fetch_add(1, std::memory_order_release);
    playbackParamsRevision.fetch_add(1, std::memory_order_release);
    restoredPlaybackParams.store(true, std::memory_order_release);
    requestSourceAudioCacheWarmup();

    if (auto* archivingController = getDocumentController()->getHostArchivingController())
        archivingController->notifyDocumentUnarchivingProgress(1.0f);

    return ! input.failed();
}

juce::ARAPlaybackRenderer* QQDeBreathARADocumentController::doCreatePlaybackRenderer()
{
    requestSourceAudioCacheWarmup();
    return new QQDeBreathARAPlaybackRenderer(getDocumentController(), *this);
}

bool QQDeBreathARADocumentController::doStoreObjectsToStream(juce::ARAOutputStream& output,
                                                             const juce::ARAStoreObjectsFilter* filter) noexcept
{
    juce::ignoreUnused(filter);

    juce::Array<QQDeBreathARAPersistentState> snapshot;
    QQDeBreathARAPlaybackParams playbackSnapshot;
    {
        const juce::ScopedLock lock(persistentStateLock);
        snapshot = persistentStates;
    }
    {
        const juce::ScopedLock lock(playbackParamsLock);
        playbackSnapshot = playbackParams;
    }

    const auto payload = serializeAraPersistentStates(snapshot, playbackSnapshot);
    const auto ok = output.writeString(payload);

    if (auto* archivingController = getDocumentController()->getHostArchivingController())
        archivingController->notifyDocumentArchivingProgress(1.0f);

    return ok;
}
