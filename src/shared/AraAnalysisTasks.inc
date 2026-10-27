// Document-owned analysis commits run on the host/model thread, including when no editor is open.
bool QQDeBreathARADocumentController::startSourceAnalysis(const QQDeBreathARASourceInfo& info,
                                                       const QQDeBreathBridgeAnalysisConfig& config)
{
    if (analysisJobs.busy(info.sourceFingerprint) || getDocumentController()->isHostEditingDocument()) return false;
    analysisJobs.cancelForSource(info.sourceFingerprint);
    for (const auto& mapping : info.playbackMappings) analysisJobs.cancelForSource(mapping.sourceFingerprint);
    QQDeBreathARASourceAudioBuffer audio; double rate = info.sampleRate; bool found = false;
    tryGetSourceAudioCache(info.sourceFingerprint, audio, rate, found);
    auto request = config;
    request.outputJson = juce::File{}; // The ARA document owns the result; no sidecar is needed.
    if (!analysisJobs.start(info, request, std::move(audio), rate)) return false;
    startTimer(50);
    return true;
}

void QQDeBreathARADocumentController::cancelSourceAnalysis(const juce::String& key)
{
    analysisJobs.cancelForSource(key);
    QQDeBreathARAPersistentState state;
    if (getPersistentStateForSource(key, state))
    {
        state.analysisResult.status = "Analysis cancelled; previous edits retained.";
        updateRuntimePersistentState(state.sourceInfo, state.analysisResult, state.playbackParams, state.regionPeakCache);
        ++analysisRevisions[key];
    }
}

void QQDeBreathARADocumentController::pollSourceAnalyses()
{
    if (getDocumentController()->isHostEditingDocument()) return;
    for (auto& completion : analysisJobs.takeCompleted())
    {
        const auto live = [&](const juce::String& key)
        {
            for (auto* source : getDocumentController()->getDocument<juce::ARADocument>()->getAudioSources<juce::ARAAudioSource>())
                if (buildAraSourceFingerprint(*source) == key && !source->isDeactivatedForUndoHistory()) return true;
            return false;
        };
        bool valid = completion.source.isComposite || live(completion.source.sourceFingerprint);
        for (const auto& mapping : completion.source.playbackMappings) valid = valid && live(mapping.sourceFingerprint);
        if (!valid) continue;
        if (completion.result.succeeded)
            commitSourceAnalysis(completion.source, completion.result, completion.peaks);
        else
        {
            QQDeBreathARAPersistentState state;
            if (getPersistentStateForSource(completion.source.sourceFingerprint, state))
            {
                state.analysisResult.status = completion.result.status;
                state.analysisResult.errorMessage = completion.result.errorMessage;
                updateRuntimePersistentState(state.sourceInfo, state.analysisResult, state.playbackParams, state.regionPeakCache);
                ++analysisRevisions[completion.source.sourceFingerprint];
            }
        }
    }
    if (analysisJobs.empty()) stopTimer();
}

void QQDeBreathARADocumentController::commitSourceAnalysis(const QQDeBreathARASourceInfo& info,
    const QQDeBreathBridgeAnalysisResult& result, const juce::Array<double>& peaks)
{
    const auto recount = [](QQDeBreathBridgeAnalysisResult& r)
    {
        r.breathCount = r.noizeCount = 0;
        r.othersCount = r.sibilanceCount = 0;
        for (auto& region : r.regions)
        {
            region.type = qqNormalizedRegionType(region.type);
            if (region.type == "Breath") ++r.breathCount;
            else if (region.type == "Noise") ++r.noizeCount;
            else ++r.othersCount;
        }
    };
    if (info.isComposite)
    {
        for (const auto& mapping : info.playbackMappings)
        {
            QQDeBreathARAPersistentState mapped;
            if (!getPersistentStateForSource(mapping.sourceFingerprint, mapped))
                mapped.playbackParams = getPlaybackParams();
            auto& source = mapped.sourceInfo;
            source.name = mapping.sourceName; source.persistentId = mapping.persistentId;
            source.sourceFingerprint = mapping.sourceFingerprint; source.sampleRate = mapping.sourceSampleRate;
            source.channelCount = mapping.sourceChannelCount; source.numSamples = mapping.sourceNumSamples;
            source.durationSeconds = source.numSamples / source.sampleRate;
            auto& r = mapped.analysisResult;
            const auto sourceStart = mapping.sourceStartSeconds;
            const auto sourceEnd = sourceStart + mapping.compositeEndSeconds - mapping.compositeStartSeconds;
            juce::Array<QQDeBreathBridgeRegion> regions;
            const auto append = [&](QQDeBreathBridgeRegion region, double start, double end)
            {
                if (end <= start) return;
                region.startTime = start; region.endTime = end;
                region.startSample = static_cast<juce::int64>(std::llround(start * source.sampleRate));
                region.endSample = static_cast<juce::int64>(std::llround(end * source.sampleRate));
                regions.add(region);
            };
            for (const auto& old : r.regions)
            {
                if (old.endTime <= sourceStart || old.startTime >= sourceEnd) regions.add(old);
                else
                {
                    append(old, old.startTime, juce::jmin(old.endTime, sourceStart));
                    append(old, juce::jmax(old.startTime, sourceEnd), old.endTime);
                }
            }
            for (const auto& region : result.regions)
            {
                const auto start = juce::jmax(region.startTime, mapping.compositeStartSeconds);
                const auto end = juce::jmin(region.endTime, mapping.compositeEndSeconds);
                if (end > start) append(region, sourceStart + start - mapping.compositeStartSeconds,
                                       sourceStart + end - mapping.compositeStartSeconds);
            }
            std::stable_sort(regions.begin(), regions.end(), [](const auto& a, const auto& b) { return a.startTime < b.startTime; });
            r = result; r.regions = std::move(regions); r.sourceKey = source.sourceFingerprint;
            r.sampleRate = static_cast<int>(source.sampleRate); r.channels = source.channelCount;
            r.numSamples = source.numSamples; r.durationSeconds = source.durationSeconds;
            recount(r);
            QQDeBreathARASourceAudioBuffer audio; double rate = 0; bool found = false;
            tryGetSourceAudioCache(source.sourceFingerprint, audio, rate, found);
            juce::Array<double> mappedPeaks;
            for (const auto& region : r.regions)
            {
                double peak = 0;
                if (audio != nullptr)
                {
                    const auto start = static_cast<int>(juce::jlimit<juce::int64>(0, audio->getNumSamples(), region.startSample));
                    const auto end = static_cast<int>(juce::jlimit<juce::int64>(start, audio->getNumSamples(), region.endSample));
                    if (end > start) peak = audio->getMagnitude(start, end - start);
                }
                mappedPeaks.add(peak);
            }
            upsertPersistentState(source, r, mapped.playbackParams, mappedPeaks);
            ++analysisRevisions[source.sourceFingerprint];
        }
    }
    QQDeBreathARAPersistentState current;
    if (!getPersistentStateForSource(info.sourceFingerprint, current)) current.playbackParams = getPlaybackParams();
    auto completed = result; recount(completed);
    upsertPersistentState(info, completed, current.playbackParams, peaks);
    ++analysisRevisions[info.sourceFingerprint];
}
