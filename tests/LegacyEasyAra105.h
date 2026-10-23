#pragma once
// Frozen QQEasyTool 1.05 ARA renderer. Test-only DSP oracle; not linked into the plugin.
#include "PluginProcessor.h"
#include <array>
namespace LegacyEasyAra105 {
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

double dbToGain(double db)
{
    return std::pow(10.0, db / 20.0);
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


}
