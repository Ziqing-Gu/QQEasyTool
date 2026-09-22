#include "LegacyEasyWaveform104.h"

#include <cmath>

namespace
{
constexpr auto minViewSeconds = 0.1;
constexpr auto minRegionSeconds = 0.02;

bool sameRegionTime(const QQDeBreathBridgeRegion& a, const QQDeBreathBridgeRegion& b)
{
    return std::abs(a.startTime - b.startTime) < 1.0e-9
        && std::abs(a.endTime - b.endTime) < 1.0e-9
        && qqNormalizedRegionType(a.type) == qqNormalizedRegionType(b.type);
}
} // namespace

LegacyEasyWaveform104::LegacyEasyWaveform104()
{
    formatManager.registerBasicFormats();
    setWantsKeyboardFocus(true);
    horizontalScrollBar.addListener(this);
    addAndMakeVisible(horizontalScrollBar);
}

bool LegacyEasyWaveform104::loadAudioFile(const juce::File& file, juce::String& status)
{
    if (! file.existsAsFile())
    {
        status = "Waveform source file not found: " + file.getFullPathName();
        clearAudio();
        return false;
    }

    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
    if (reader == nullptr)
    {
        status = "Could not read waveform source: " + file.getFullPathName();
        clearAudio();
        return false;
    }

    juce::AudioBuffer<float> temp(static_cast<int>(reader->numChannels),
                                  static_cast<int>(reader->lengthInSamples));
    reader->read(&temp, 0, static_cast<int>(reader->lengthInSamples), 0, true, true);

    monoWaveform.setSize(1, temp.getNumSamples());
    monoWaveform.clear();

    for (auto channel = 0; channel < temp.getNumChannels(); ++channel)
        monoWaveform.addFrom(0, 0, temp, channel, 0, temp.getNumSamples(), 1.0f / juce::jmax(1, temp.getNumChannels()));

    sampleRate = reader->sampleRate;
    timelineDuration = getDurationSeconds();
    playhead = 0.0;
    rebuildBreathNormGainCache();
    fitView();
    updateScrollBar();
    status = "Waveform loaded.";
    repaint();
    return true;
}

void LegacyEasyWaveform104::setAudioBuffer(const juce::AudioBuffer<float>& audio,
                                              double sourceSampleRate,
                                              double timelineDurationSeconds,
                                              bool preserveView)
{
    const auto hadAudio = monoWaveform.getNumSamples() > 0 && sampleRate > 0.0;
    const auto oldViewStart = viewStart;
    const auto oldViewEnd = viewEnd;

    sampleRate = sourceSampleRate;
    monoWaveform.setSize(1, audio.getNumSamples());
    monoWaveform.clear();

    for (auto channel = 0; channel < audio.getNumChannels(); ++channel)
        monoWaveform.addFrom(0, 0, audio, channel, 0, audio.getNumSamples(), 1.0f / juce::jmax(1, audio.getNumChannels()));

    timelineDuration = juce::jmax(getDurationSeconds(), timelineDurationSeconds);
    rebuildBreathNormGainCache();

    if (preserveView && hadAudio)
        setView(oldViewStart, oldViewEnd);
    else
        fitView();

    updateScrollBar();
    repaint();
}

void LegacyEasyWaveform104::clearAudio()
{
    recordingOverlay = false;
    recordingOverlayText.clear();
    monoWaveform.setSize(0, 0);
    sampleRate = 0.0;
    timelineDuration = 0.0;
    viewStart = 0.0;
    viewEnd = 1.0;
    playhead = 0.0;
    selectedRegion = -1;
    undoStack.clear();
    redoStack.clear();
    breathNormGainCache.clear();
    displayRegions.clear();
    processedBreathDisplay.setSize(0, 0);
    processedSibilanceDisplay.setSize(0, 0);
    processedOthersDisplay.setSize(0, 0);
    processedBreathDisplayKey.clear();
    updateScrollBar();
    repaint();
}

void LegacyEasyWaveform104::setAnalysisResult(const QQDeBreathBridgeAnalysisResult& result)
{
    regions = result.regions;
    for (auto& region : regions)
        region.type = qqNormalizedRegionType(region.type);
    selectedRegion = -1;
    undoStack.clear();
    redoStack.clear();
    rebuildBreathNormGainCache();
    repaint();
}

void LegacyEasyWaveform104::clearRegions()
{
    if (! regions.isEmpty())
        pushUndoState();

    regions.clear();
    selectedRegion = -1;
    notifyRegionsChanged();
    repaint();
}

QQDeBreathBridgeRegion LegacyEasyWaveform104::getRegion(int index) const
{
    if (index >= 0 && index < regions.size())
        return regions.getReference(index);

    return {};
}

juce::Array<double> LegacyEasyWaveform104::buildRegionPeakCache(
    const juce::Array<QQDeBreathBridgeRegion>& regionsToMeasure) const
{
    juce::Array<double> peaks;
    peaks.ensureStorageAllocated(regionsToMeasure.size());

    if (sampleRate <= 0.0 || monoWaveform.getNumSamples() <= 0)
    {
        for (auto i = 0; i < regionsToMeasure.size(); ++i)
            peaks.add(0.0);
        return peaks;
    }

    for (const auto& region : regionsToMeasure)
    {
        const auto start = regionStartSample(region);
        const auto end = regionEndSample(region);
        const auto length = static_cast<int>(juce::jmax<juce::int64>(0, end - start));
        const auto peak = length > 0
                        ? monoWaveform.getMagnitude(0, static_cast<int>(start), length)
                        : 0.0f;
        peaks.add(static_cast<double>(peak));
    }

    return peaks;
}

void LegacyEasyWaveform104::setRegionProcessing(int index, double gainDb, const QQDeBreathEqState& eqState, bool notifyChange, bool shouldRebuildDisplay)
{
    if (index < 0 || index >= regions.size())
        return;

    auto& region = regions.getReference(index);
    if (! qqRegionSupportsProcessing(region.type))
        return;

    const auto oldGainDb = region.gainDb;
    const auto oldEq = serializeBreathEqState(region.eqState);
    region.gainDb = juce::jlimit(-30.0, 30.0, gainDb);
    region.eqState = sanitizeBreathEqState(eqState);
    if (notifyChange)
        notifyRegionsChanged(std::abs(oldGainDb - region.gainDb) > 1.0e-6);
    else if (shouldRebuildDisplay
             && (std::abs(oldGainDb - region.gainDb) > 1.0e-6
                 || oldEq != serializeBreathEqState(region.eqState)))
        rebuildBreathNormGainCache();
    repaint();
}

void LegacyEasyWaveform104::setCreationType(const juce::String& type)
{
    creationType = qqNormalizedRegionType(type);
    repaint();
}

void LegacyEasyWaveform104::setSelectedRegionType(const juce::String& type)
{
    if (selectedRegion < 0 || selectedRegion >= regions.size())
        return;

    pushUndoState();
    auto& region = regions.getReference(selectedRegion);
    region.type = qqNormalizedRegionType(type);
    creationType = region.type;
    if (! qqRegionSupportsProcessing(region.type))
    {
        region.gainDb = 0.0;
        region.eqState = {};
    }
    notifyRegionsChanged();
    if (onSelectedRegionChanged)
        onSelectedRegionChanged(selectedRegion);
    repaint();
}

double LegacyEasyWaveform104::getDurationSeconds() const
{
    if (sampleRate <= 0.0 || monoWaveform.getNumSamples() <= 0)
        return 0.0;

    return static_cast<double>(monoWaveform.getNumSamples()) / sampleRate;
}

double LegacyEasyWaveform104::getTimelineDurationSeconds() const
{
    return juce::jmax(getDurationSeconds(), timelineDuration);
}

bool LegacyEasyWaveform104::canUndo() const
{
    return ! undoStack.isEmpty();
}

bool LegacyEasyWaveform104::canRedo() const
{
    return ! redoStack.isEmpty();
}

void LegacyEasyWaveform104::undo()
{
    if (undoStack.isEmpty())
        return;

    redoStack.add(regions);
    regions = undoStack.getLast();
    undoStack.removeLast();
    selectedRegion = -1;
    rebuildBreathNormGainCache();
    notifyRegionsChanged();
    repaint();
}

void LegacyEasyWaveform104::redo()
{
    if (redoStack.isEmpty())
        return;

    undoStack.add(regions);
    regions = redoStack.getLast();
    redoStack.removeLast();
    selectedRegion = -1;
    rebuildBreathNormGainCache();
    notifyRegionsChanged();
    repaint();
}

void LegacyEasyWaveform104::setTimelineDurationSeconds(double seconds)
{
    timelineDuration = juce::jmax(getDurationSeconds(), seconds);
    if (followPlayhead && playhead > viewEnd)
    {
        const auto span = juce::jmax(minViewSeconds, viewEnd - viewStart);
        setView(playhead, playhead + span);
    }
    updateScrollBar();
    repaint();
}

void LegacyEasyWaveform104::setPlayheadSeconds(double seconds)
{
    const auto previousPlayhead = playhead;
    const auto wasVisible = previousPlayhead >= viewStart && previousPlayhead <= viewEnd;
    playhead = juce::jmax(0.0, seconds);
    timelineDuration = juce::jmax(timelineDuration, playhead);
    const auto isVisible = playhead >= viewStart && playhead <= viewEnd;

    if (followPlayhead && (playhead < viewStart || playhead > viewEnd))
    {
        const auto span = juce::jmax(minViewSeconds, viewEnd - viewStart);
        setView(playhead, playhead + span);
        return;
    }

    if (wasVisible || isVisible)
        repaint();
}

void LegacyEasyWaveform104::setWaveformDisplayGain(double gain)
{
    waveformDisplayGain = juce::jlimit(0.25, 8.0, gain);
    repaint();
}

void LegacyEasyWaveform104::setFollowPlayhead(bool shouldFollow)
{
    if (followPlayhead == shouldFollow)
        return;

    followPlayhead = shouldFollow;
    if (followPlayhead && (playhead < viewStart || playhead > viewEnd))
    {
        const auto span = juce::jmax(minViewSeconds, viewEnd - viewStart);
        setView(playhead, playhead + span);
        return;
    }

    repaint();
}

void LegacyEasyWaveform104::setRecordingOverlay(bool shouldShow, const juce::String& text)
{
    if (recordingOverlay == shouldShow && recordingOverlayText == text)
        return;

    recordingOverlay = shouldShow;
    recordingOverlayText = text;
    updateScrollBar();
    repaint();
}

void LegacyEasyWaveform104::setMonitorState(bool voiceEnabled, bool breathEnabled, bool noiseEnabled, bool sibilanceEnabled, bool othersEnabled)
{
    if (monitorVoice == voiceEnabled && monitorBreath == breathEnabled && monitorNoize == noiseEnabled
        && monitorSibilance == sibilanceEnabled && monitorOthers == othersEnabled)
        return;

    monitorVoice = voiceEnabled;
    monitorBreath = breathEnabled;
    monitorNoize = noiseEnabled;
    monitorSibilance = sibilanceEnabled;
    monitorOthers = othersEnabled;
    repaint();
}

void LegacyEasyWaveform104::setProcessingParams(const DisplayProcessingParams& params)
{
    const auto normChanged = processingParams.normalizeBreath != params.normalizeBreath
                          || processingParams.normalizeSibilance != params.normalizeSibilance
                          || std::abs(processingParams.breathTargetDb - params.breathTargetDb) > 1.0e-6
                          || std::abs(processingParams.sibilanceTargetDb - params.sibilanceTargetDb) > 1.0e-6;
    const auto eqChanged = serializeBreathEqState(processingParams.breathEqState) != serializeBreathEqState(params.breathEqState);
    const auto fadeChanged = processingParams.enableFade != params.enableFade
                          || std::abs(processingParams.fadeInMs - params.fadeInMs) > 1.0e-6
                          || std::abs(processingParams.fadeOutMs - params.fadeOutMs) > 1.0e-6;
    const auto breathGainChanged = std::abs(processingParams.breathGainDb - params.breathGainDb) > 1.0e-6;
    const auto sibilanceGlobalChanged = std::abs(processingParams.sibilanceGainDb - params.sibilanceGainDb) > 1.0e-6
                                      || serializeBreathEqState(processingParams.sibilanceEqState) != serializeBreathEqState(params.sibilanceEqState);
    processingParams = params;

    if (normChanged || fadeChanged || breathGainChanged || eqChanged || sibilanceGlobalChanged)
        rebuildBreathNormGainCache();

    repaint();
}

void LegacyEasyWaveform104::refreshProcessedDisplay()
{
    processedBreathDisplayKey.clear();
    rebuildBreathNormGainCache();
    repaint();
}

void LegacyEasyWaveform104::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.fillAll(juce::Colour(0xff07111f));
    g.setColour(juce::Colour(0xff1e293b));
    g.drawRoundedRectangle(bounds.reduced(1.0f), 6.0f, 1.0f);

    auto area = getWaveformArea();
    g.setColour(juce::Colour(0xff94a3b8));
    g.setFont(13.0f);

    if (recordingOverlay)
    {
        g.setColour(juce::Colour(0xff07111f));
        g.fillRoundedRectangle(area.toFloat(), 5.0f);
        g.setColour(juce::Colour(0xff1e293b));
        g.drawRoundedRectangle(area.toFloat(), 5.0f, 1.0f);
        g.setColour(juce::Colour(0xff38bdf8).withAlpha(0.16f));
        g.fillRect(area.reduced(12).toFloat());
        g.setColour(juce::Colour(0xffe2e8f0));
        g.setFont(juce::Font(42.0f, juce::Font::bold));
        g.drawText(recordingOverlayText.isNotEmpty() ? recordingOverlayText : "Recording...",
                   area,
                   juce::Justification::centred);
        g.setFont(14.0f);
        g.setColour(juce::Colour(0xff94a3b8));
        g.drawText("Waveform will be generated after recording stops.",
                   area.reduced(0, 54),
                   juce::Justification::centredBottom);
        return;
    }

    if (monoWaveform.getNumSamples() <= 0 || sampleRate <= 0.0)
    {
        g.drawText("Waveform: load or analyze audio, then create Noise / Breath / Others regions.",
                   area, juce::Justification::centred);
        return;
    }

    const auto duration = getTimelineDurationSeconds();
    viewStart = juce::jlimit(0.0, juce::jmax(0.0, duration - minViewSeconds), viewStart);
    viewEnd = juce::jlimit(viewStart + minViewSeconds, duration, viewEnd);

    for (auto i = 0; i < regions.size(); ++i)
    {
        const auto& region = regions.getReference(i);
        const auto x1 = timeToX(region.startTime);
        const auto x2 = timeToX(region.endTime);

        if (x2 < area.getX() || x1 > area.getRight())
            continue;

        g.setColour(regionColour(region.type));
        g.fillRect(juce::Rectangle<int>(x1, area.getY(), juce::jmax(1, x2 - x1), area.getHeight()).toFloat());

        if (i == selectedRegion)
        {
            g.setColour(juce::Colour(0xfff8fafc));
            g.drawRect(juce::Rectangle<int>(x1, area.getY(), juce::jmax(1, x2 - x1), area.getHeight()), 2);
        }
    }

    if (selectedRegion >= 0 && selectedRegion < regions.size())
    {
        const auto selectedType = qqNormalizedRegionType(regions.getReference(selectedRegion).type);
        auto badge = juce::Rectangle<int>(area.getX() + 8, area.getY() + 8, 176, 28);
        g.setColour(juce::Colour(0xff020617).withAlpha(0.88f));
        g.fillRoundedRectangle(badge.toFloat(), 5.0f);
        g.setColour(regionColour(selectedType).withAlpha(0.95f));
        g.drawRoundedRectangle(badge.toFloat(), 5.0f, 2.0f);
        g.setFont(juce::Font(14.0f, juce::Font::bold));
        g.setColour(juce::Colour(0xfff8fafc));
        g.drawText("Selected: " + selectedType, badge.reduced(8, 0), juce::Justification::centredLeft);
    }

    if (dragMode == DragMode::create)
    {
        const auto x1 = timeToX(juce::jmin(createStart, createEnd));
        const auto x2 = timeToX(juce::jmax(createStart, createEnd));
        g.setColour(regionColour(creationType).withAlpha(0.45f));
        g.fillRect(juce::Rectangle<int>(x1, area.getY(), juce::jmax(1, x2 - x1), area.getHeight()).toFloat());
        g.setColour(juce::Colour(0xfff8fafc));
        g.drawRect(juce::Rectangle<int>(x1, area.getY(), juce::jmax(1, x2 - x1), area.getHeight()), 1);
    }

    drawFadeGuides(g, area);

    const auto midY = area.getCentreY();
    g.setColour(juce::Colour(0xff334155));
    g.drawHorizontalLine(midY, static_cast<float>(area.getX()), static_cast<float>(area.getRight()));

    const auto audioDuration = getDurationSeconds();
    const auto drawStartTime = juce::jlimit(0.0, audioDuration, viewStart);
    const auto drawEndTime = juce::jlimit(drawStartTime, audioDuration, viewEnd);
    const auto startSample = juce::jlimit(0, monoWaveform.getNumSamples() - 1, static_cast<int>(std::floor(drawStartTime * sampleRate)));
    const auto endSample = juce::jlimit(startSample + 1, monoWaveform.getNumSamples(), static_cast<int>(std::ceil(drawEndTime * sampleRate)));
    auto visiblePeak = 0.0f;
    const auto peakStep = juce::jmax(1, (endSample - startSample) / 4000);
    const auto* data = monoWaveform.getReadPointer(0);
    for (auto s = startSample; s < endSample; s += peakStep)
        visiblePeak = juce::jmax(visiblePeak, std::abs(data[s]));

    const auto autoGain = visiblePeak > 1.0e-5f
                        ? juce::jlimit(1.0, 32.0, 0.86 / static_cast<double>(visiblePeak))
                        : 1.0;
    const auto totalDisplayGain = static_cast<float>(autoGain * waveformDisplayGain);
    const auto heightScale = area.getHeight() * 0.42f;
    const auto drawLeft = timeToX(drawStartTime);
    const auto drawRight = timeToX(drawEndTime);

    const auto drawWave = [&](bool activeComponents, juce::Colour colour)
    {
        g.setColour(colour);
        for (auto x = juce::jmax(area.getX(), drawLeft); x < juce::jmin(area.getRight(), drawRight); ++x)
        {
            const auto ratio1 = (xToTime(x) - drawStartTime) / juce::jmax(1.0e-9, drawEndTime - drawStartTime);
            const auto ratio2 = (xToTime(x + 1) - drawStartTime) / juce::jmax(1.0e-9, drawEndTime - drawStartTime);
            auto s1 = startSample + static_cast<int>(ratio1 * (endSample - startSample));
            auto s2 = startSample + static_cast<int>(ratio2 * (endSample - startSample));
            s1 = juce::jlimit(startSample, endSample - 1, s1);
            s2 = juce::jlimit(s1 + 1, endSample, s2);

            auto lo = 0.0;
            auto hi = 0.0;
            const auto sampleStep = juce::jmax(1, (s2 - s1) / 12);
            for (auto s = s1; s < s2; s += sampleStep)
            {
                const auto sample = renderedDisplaySample(s, activeComponents);
                lo = juce::jmin(lo, sample);
                hi = juce::jmax(hi, sample);
            }

            if (std::abs(lo) < 1.0e-9 && std::abs(hi) < 1.0e-9)
                continue;

            const auto y1 = midY - static_cast<int>(juce::jlimit(-1.0, 1.0, hi * totalDisplayGain) * heightScale);
            const auto y2 = midY - static_cast<int>(juce::jlimit(-1.0, 1.0, lo * totalDisplayGain) * heightScale);
            g.drawVerticalLine(x, static_cast<float>(y1), static_cast<float>(y2));
        }
    };

    drawWave(false, juce::Colour(0xff64748b).withAlpha(0.48f));
    drawWave(true, juce::Colour(0xffe2e8f0));

    if (playhead >= viewStart && playhead <= viewEnd)
    {
        const auto x = timeToX(playhead);
        g.setColour(juce::Colour(0xfffacc15));
        g.drawVerticalLine(x, static_cast<float>(area.getY()), static_cast<float>(area.getBottom()));
    }

    g.setColour(juce::Colour(0xffcbd5e1));
    g.setFont(12.0f);
    g.drawText(juce::String(viewStart, 2) + "s", area.removeFromBottom(18), juce::Justification::centredLeft);
    g.drawText(juce::String(viewEnd, 2) + "s", area, juce::Justification::bottomRight);
}

void LegacyEasyWaveform104::resized()
{
    auto area = getLocalBounds().reduced(10);
    horizontalScrollBar.setBounds(area.removeFromBottom(14));
    updateScrollBar();
}

void LegacyEasyWaveform104::mouseDown(const juce::MouseEvent& event)
{
    grabKeyboardFocus();

    const auto startEdgeHit = hitTestRegionStartEdge(event.x);
    const auto endEdgeHit = hitTestRegionEndEdge(event.x);
    if (event.mods.isLeftButtonDown() && ! event.mods.isPopupMenu() && (startEdgeHit >= 0 || endEdgeHit >= 0))
    {
        pushUndoState();
        resizeRegion = startEdgeHit >= 0 ? startEdgeHit : endEdgeHit;
        dragMode = startEdgeHit >= 0 ? DragMode::resizeStart : DragMode::resizeEnd;
        selectRegion(resizeRegion);
        repaint();
        return;
    }

    if (event.mods.isShiftDown() && event.mods.isLeftButtonDown())
    {
        dragMode = DragMode::create;
        createStart = xToTime(event.x);
        createEnd = createStart;
        repaint();
        return;
    }

    const auto hit = hitTestRegion(event.x);
    selectRegion(hit);

    if (event.getNumberOfClicks() >= 2 && event.mods.isLeftButtonDown() && hit >= 0)
    {
        if (onRegionDoubleClicked)
            onRegionDoubleClicked(hit);
        return;
    }

    if (event.mods.isRightButtonDown() && hit >= 0)
    {
        const auto& region = regions.getReference(hit);
        const auto current = qqNormalizedRegionType(region.type);
        setSelectedRegionType(current == "Breath" ? "Others"
                            : current == "Others" ? "Noise"
                                                  : "Breath");
        return;
    }

    if (event.mods.isLeftButtonDown() && ! event.mods.isPopupMenu())
    {
        const auto time = xToTime(event.x);
        setPlayheadSeconds(time);
        if (onSeekRequested)
            onSeekRequested(time);

        if (hit >= 0)
        {
            const auto& region = regions.getReference(hit);
            dragMode = DragMode::move;
            moveRegion = hit;
            movePointerStartTime = time;
            moveOriginalStartTime = region.startTime;
            moveOriginalEndTime = region.endTime;
            moveUndoPushed = false;
        }
    }
}
void LegacyEasyWaveform104::mouseMove(const juce::MouseEvent& event)
{
    updateMouseCursor(event.x);
}

void LegacyEasyWaveform104::mouseDrag(const juce::MouseEvent& event)
{
    if (dragMode == DragMode::create)
    {
        createEnd = xToTime(event.x);
        repaint();
    }
    else if (dragMode == DragMode::move && moveRegion >= 0 && moveRegion < regions.size())
    {
        if (! moveUndoPushed)
        {
            if (event.getDistanceFromDragStart() < 3)
                return;

            pushUndoState();
            moveUndoPushed = true;
        }

        const auto duration = juce::jmax(minRegionSeconds, moveOriginalEndTime - moveOriginalStartTime);
        const auto previousEnd = moveRegion > 0 ? regions.getReference(moveRegion - 1).endTime : 0.0;
        const auto nextStart = moveRegion + 1 < regions.size()
                             ? regions.getReference(moveRegion + 1).startTime
                             : getTimelineDurationSeconds();
        const auto latestStart = juce::jmax(previousEnd, nextStart - duration);
        const auto requestedStart = moveOriginalStartTime + xToTime(event.x) - movePointerStartTime;
        const auto newStart = juce::jlimit(previousEnd, latestStart, requestedStart);
        auto& region = regions.getReference(moveRegion);
        region.startTime = newStart;
        region.endTime = juce::jmin(getTimelineDurationSeconds(), newStart + duration);
        region.startSample = sampleRate > 0.0 ? static_cast<juce::int64>(std::round(region.startTime * sampleRate)) : region.startSample;
        region.endSample = sampleRate > 0.0 ? static_cast<juce::int64>(std::round(region.endTime * sampleRate)) : region.endSample;
        deferredRegionDisplayRebuild = true;
        repaint();
    }
    else if ((dragMode == DragMode::resizeStart || dragMode == DragMode::resizeEnd)
          && resizeRegion >= 0
          && resizeRegion < regions.size())
    {
        auto& region = regions.getReference(resizeRegion);
        const auto time = juce::jlimit(0.0, getDurationSeconds(), xToTime(event.x));
        const auto previousEnd = resizeRegion > 0 ? regions.getReference(resizeRegion - 1).endTime : 0.0;
        const auto nextStart = resizeRegion + 1 < regions.size() ? regions.getReference(resizeRegion + 1).startTime : getTimelineDurationSeconds();

        if (dragMode == DragMode::resizeStart)
            region.startTime = juce::jlimit(previousEnd, region.endTime - minRegionSeconds, time);
        else
            region.endTime = juce::jlimit(region.startTime + minRegionSeconds, nextStart, time);

        region.startSample = sampleRate > 0.0 ? static_cast<juce::int64>(std::round(region.startTime * sampleRate)) : region.startSample;
        region.endSample = sampleRate > 0.0 ? static_cast<juce::int64>(std::round(region.endTime * sampleRate)) : region.endSample;
        deferredRegionDisplayRebuild = true;
        repaint();
    }
}
void LegacyEasyWaveform104::mouseUp(const juce::MouseEvent& /*event*/)
{
    if (dragMode == DragMode::create)
    {
        const auto start = juce::jlimit(0.0, getDurationSeconds(), juce::jmin(createStart, createEnd));
        const auto end = juce::jlimit(0.0, getDurationSeconds(), juce::jmax(createStart, createEnd));

        if (end - start >= 0.02)
        {
            QQDeBreathBridgeRegion region;
            region.type = creationType;
            region.startTime = start;
            region.endTime = end;
            region.startSample = sampleRate > 0.0 ? static_cast<juce::int64>(std::round(start * sampleRate)) : 0;
            region.endSample = sampleRate > 0.0 ? static_cast<juce::int64>(std::round(end * sampleRate)) : 0;
            const auto newIndex = insertRegionReplacingOverlaps(region);
            selectRegion(newIndex);
            notifyRegionsChanged();
        }
    }

    if (dragMode == DragMode::move && moveUndoPushed)
    {
        deferredRegionDisplayRebuild = false;
        notifyRegionsChanged();
    }

    if (dragMode == DragMode::resizeStart || dragMode == DragMode::resizeEnd)
    {
        deferredRegionDisplayRebuild = false;
        notifyRegionsChanged();
    }

    dragMode = DragMode::none;
    resizeRegion = -1;
    moveRegion = -1;
    moveUndoPushed = false;
    repaint();
}
void LegacyEasyWaveform104::mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    const auto duration = getTimelineDurationSeconds();
    if (duration <= 0.0)
        return;

    auto span = juce::jmax(minViewSeconds, viewEnd - viewStart);

    if (event.mods.isShiftDown())
    {
        const auto amount = std::abs(wheel.deltaY) >= std::abs(wheel.deltaX) ? wheel.deltaY : -wheel.deltaX;
        const auto step = span * (amount < 0.0f ? 0.18 : -0.18);
        setView(viewStart + step, viewEnd + step);
    }
    else if (std::abs(wheel.deltaY) > std::abs(wheel.deltaX))
    {
        const auto centre = xToTime(event.x);
        const auto factor = wheel.deltaY > 0.0f ? 0.8 : 1.25;
        const auto newSpan = juce::jlimit(minViewSeconds, duration, span * factor);
        const auto ratio = (centre - viewStart) / span;
        setView(centre - ratio * newSpan, centre + (1.0 - ratio) * newSpan);
    }
    else
    {
        const auto step = span * (wheel.deltaX < 0.0f ? 0.18 : -0.18);
        setView(viewStart + step, viewEnd + step);
    }
}

bool LegacyEasyWaveform104::keyPressed(const juce::KeyPress& key)
{
    const auto modifiers = key.getModifiers();
    const auto keyCode = key.getKeyCode();
    if ((modifiers.isCommandDown() || modifiers.isCtrlDown()) && (keyCode == 'z' || keyCode == 'Z'))
    {
        if (modifiers.isShiftDown())
            redo();
        else
            undo();

        return true;
    }

    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
    {
        if (selectedRegion >= 0 && selectedRegion < regions.size())
        {
            pushUndoState();
            regions.remove(selectedRegion);
            selectRegion(-1);
            notifyRegionsChanged();
            repaint();
            return true;
        }
    }

    return false;
}
int LegacyEasyWaveform104::timeToX(double seconds) const
{
    const auto area = getWaveformArea();
    return area.getX() + static_cast<int>((seconds - viewStart) / juce::jmax(1.0e-9, viewEnd - viewStart) * area.getWidth());
}

double LegacyEasyWaveform104::xToTime(int x) const
{
    const auto area = getWaveformArea();
    return viewStart + (static_cast<double>(x - area.getX()) / juce::jmax(1, area.getWidth())) * (viewEnd - viewStart);
}

int LegacyEasyWaveform104::hitTestRegion(int x) const
{
    const auto time = xToTime(x);
    for (auto i = regions.size(); --i >= 0;)
    {
        const auto& region = regions.getReference(i);
        if (time >= region.startTime && time <= region.endTime)
            return i;
    }

    return -1;
}

int LegacyEasyWaveform104::hitTestRegionStartEdge(int x) const
{
    const auto tolerance = juce::jmax(5, getWaveformArea().getWidth() / 240);
    for (auto i = regions.size(); --i >= 0;)
    {
        const auto edgeX = timeToX(regions.getReference(i).startTime);
        if (std::abs(edgeX - x) <= tolerance)
            return i;
    }

    return -1;
}

int LegacyEasyWaveform104::hitTestRegionEndEdge(int x) const
{
    const auto tolerance = juce::jmax(5, getWaveformArea().getWidth() / 240);
    for (auto i = regions.size(); --i >= 0;)
    {
        const auto edgeX = timeToX(regions.getReference(i).endTime);
        if (std::abs(edgeX - x) <= tolerance)
            return i;
    }

    return -1;
}

void LegacyEasyWaveform104::updateMouseCursor(int x)
{
    if (hitTestRegionStartEdge(x) >= 0 || hitTestRegionEndEdge(x) >= 0)
        setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
    else if (hitTestRegion(x) >= 0)
        setMouseCursor(juce::MouseCursor::DraggingHandCursor);
    else
        setMouseCursor(juce::MouseCursor::NormalCursor);
}

void LegacyEasyWaveform104::selectRegion(int index)
{
    selectedRegion = index >= 0 && index < regions.size() ? index : -1;

    if (onSelectedRegionChanged != nullptr)
        onSelectedRegionChanged(selectedRegion);
}

void LegacyEasyWaveform104::pushUndoState()
{
    undoStack.add(regions);
    redoStack.clear();

    constexpr auto maxUndoSteps = 64;
    while (undoStack.size() > maxUndoSteps)
        undoStack.remove(0);
}

void LegacyEasyWaveform104::notifyRegionsChanged(bool rebuildDisplay)
{
    for (auto& region : regions)
    {
        region.type = qqNormalizedRegionType(region.type);
        region.startTime = juce::jlimit(0.0, getTimelineDurationSeconds(), region.startTime);
        region.endTime = juce::jlimit(region.startTime, getTimelineDurationSeconds(), region.endTime);
        region.startSample = sampleRate > 0.0 ? static_cast<juce::int64>(std::round(region.startTime * sampleRate)) : region.startSample;
        region.endSample = sampleRate > 0.0 ? static_cast<juce::int64>(std::round(region.endTime * sampleRate)) : region.endSample;
        region.gainDb = juce::jlimit(-30.0, 30.0, region.gainDb);
        region.eqState = sanitizeBreathEqState(region.eqState);
    }

    if (rebuildDisplay)
        rebuildBreathNormGainCache();

    if (onRegionsChanged != nullptr)
        onRegionsChanged(regions);
}

int LegacyEasyWaveform104::insertRegionReplacingOverlaps(QQDeBreathBridgeRegion region)
{
    pushUndoState();

    region.type = qqNormalizedRegionType(region.type);
    region.startTime = juce::jlimit(0.0, getTimelineDurationSeconds(), region.startTime);
    region.endTime = juce::jlimit(region.startTime, getTimelineDurationSeconds(), region.endTime);
    region.startSample = sampleRate > 0.0 ? static_cast<juce::int64>(std::round(region.startTime * sampleRate)) : 0;
    region.endSample = sampleRate > 0.0 ? static_cast<juce::int64>(std::round(region.endTime * sampleRate)) : 0;

    juce::Array<QQDeBreathBridgeRegion> adjusted;
    for (const auto& oldRegion : regions)
    {
        if (oldRegion.endTime <= region.startTime || oldRegion.startTime >= region.endTime)
        {
            adjusted.add(oldRegion);
            continue;
        }

        if (oldRegion.startTime < region.startTime && region.startTime - oldRegion.startTime >= minRegionSeconds)
        {
            auto left = oldRegion;
            left.endTime = region.startTime;
            left.endSample = region.startSample;
            adjusted.add(left);
        }

        if (oldRegion.endTime > region.endTime && oldRegion.endTime - region.endTime >= minRegionSeconds)
        {
            auto right = oldRegion;
            right.startTime = region.endTime;
            right.startSample = region.endSample;
            adjusted.add(right);
        }
    }

    adjusted.add(region);

    for (auto i = 0; i < adjusted.size(); ++i)
        for (auto j = i + 1; j < adjusted.size(); ++j)
            if (adjusted.getReference(j).startTime < adjusted.getReference(i).startTime)
                adjusted.swap(i, j);

    regions = adjusted;
    rebuildBreathNormGainCache();

    for (auto i = 0; i < regions.size(); ++i)
        if (sameRegionTime(regions.getReference(i), region))
            return i;

    return -1;
}

void LegacyEasyWaveform104::fitView()
{
    viewStart = 0.0;
    viewEnd = juce::jmax(minViewSeconds, getTimelineDurationSeconds());
    updateScrollBar();
}

void LegacyEasyWaveform104::setView(double start, double end)
{
    const auto duration = getTimelineDurationSeconds();
    auto span = juce::jlimit(minViewSeconds, juce::jmax(minViewSeconds, duration), end - start);
    start = juce::jlimit(0.0, juce::jmax(0.0, duration - span), start);
    viewStart = start;
    viewEnd = juce::jmin(duration, start + span);
    updateScrollBar();
    repaint();
}

juce::Rectangle<int> LegacyEasyWaveform104::getWaveformArea() const
{
    auto area = getLocalBounds().reduced(10);
    area.removeFromBottom(16);
    return area;
}

void LegacyEasyWaveform104::updateScrollBar()
{
    if (recordingOverlay)
    {
        horizontalScrollBar.setVisible(false);
        return;
    }

    const auto duration = getTimelineDurationSeconds();
    const auto span = juce::jmax(minViewSeconds, viewEnd - viewStart);
    updatingScrollBar = true;
    horizontalScrollBar.setRangeLimits(0.0, juce::jmax(minViewSeconds, duration));
    horizontalScrollBar.setCurrentRange(viewStart, span);
    horizontalScrollBar.setVisible(duration > span + 0.001);
    updatingScrollBar = false;
}

void LegacyEasyWaveform104::scrollBarMoved(juce::ScrollBar* scrollBarThatHasMoved, double newRangeStart)
{
    if (updatingScrollBar || scrollBarThatHasMoved != &horizontalScrollBar)
        return;

    const auto span = juce::jmax(minViewSeconds, viewEnd - viewStart);
    setView(newRangeStart, newRangeStart + span);
}

juce::Colour LegacyEasyWaveform104::regionColour(const juce::String& type) const
{
    const auto normalized = qqNormalizedRegionType(type);
    if (normalized == "Noise")
        return monitorNoize ? juce::Colour(0xffa855f7).withAlpha(0.44f)
                            : juce::Colour(0xff64748b).withAlpha(0.25f);
    if (normalized == "Sibilance")
        return monitorSibilance ? juce::Colour(0xfff59e0b).withAlpha(0.46f)
                               : juce::Colour(0xff64748b).withAlpha(0.25f);
    if (normalized == "Others")
        return monitorOthers ? juce::Colour(0xff38bdf8).withAlpha(0.42f)
                            : juce::Colour(0xff64748b).withAlpha(0.25f);
    return monitorBreath ? juce::Colour(0xff22c55e).withAlpha(0.40f)
                         : juce::Colour(0xff64748b).withAlpha(0.25f);
}

juce::int64 LegacyEasyWaveform104::regionStartSample(const QQDeBreathBridgeRegion& region) const
{
    if (sampleRate <= 0.0)
        return 0;

    return juce::jlimit<juce::int64>(0,
                                     monoWaveform.getNumSamples(),
                                     region.startSample > 0 ? region.startSample
                                                            : static_cast<juce::int64>(std::llround(region.startTime * sampleRate)));
}

juce::int64 LegacyEasyWaveform104::regionEndSample(const QQDeBreathBridgeRegion& region) const
{
    if (sampleRate <= 0.0)
        return 0;

    return juce::jlimit<juce::int64>(0,
                                     monoWaveform.getNumSamples(),
                                     region.endSample > 0 ? region.endSample
                                                          : static_cast<juce::int64>(std::llround(region.endTime * sampleRate)));
}

bool LegacyEasyWaveform104::hasAdjacentRegionBefore(int regionIndex, juce::int64 start, int fadeSamples) const
{
    const auto tolerance = juce::jmax<juce::int64>(1, fadeSamples);
    for (auto i = 0; i < regions.size(); ++i)
    {
        if (i == regionIndex)
            continue;

        const auto otherEnd = regionEndSample(regions.getReference(i));
        if (std::abs(otherEnd - start) <= tolerance)
            return true;
    }

    return false;
}

bool LegacyEasyWaveform104::hasAdjacentRegionAfter(int regionIndex, juce::int64 end, int fadeSamples) const
{
    const auto tolerance = juce::jmax<juce::int64>(1, fadeSamples);
    for (auto i = 0; i < regions.size(); ++i)
    {
        if (i == regionIndex)
            continue;

        const auto otherStart = regionStartSample(regions.getReference(i));
        if (std::abs(otherStart - end) <= tolerance)
            return true;
    }

    return false;
}

double LegacyEasyWaveform104::regionWeightAtSample(int regionIndex,
                                                      juce::int64 sampleIndex,
                                                      int fadeInSamples,
                                                      int fadeOutSamples) const
{
    const auto& region = regions.getReference(regionIndex);
    const auto start = regionStartSample(region);
    const auto end = regionEndSample(region);
    const auto adjacentBefore = fadeInSamples > 0 && hasAdjacentRegionBefore(regionIndex, start, fadeInSamples);
    const auto adjacentAfter = fadeOutSamples > 0 && hasAdjacentRegionAfter(regionIndex, end, fadeOutSamples);

    if (sampleIndex >= start && sampleIndex < end)
    {
        auto weight = 1.0;
        if (fadeInSamples > 0 && ! adjacentBefore)
            weight = juce::jmin(weight, static_cast<double>(sampleIndex - start) / juce::jmax(1, fadeInSamples - 1));

        if (fadeOutSamples > 0 && ! adjacentAfter)
            weight = juce::jmin(weight, static_cast<double>(end - 1 - sampleIndex) / juce::jmax(1, fadeOutSamples - 1));

        return juce::jlimit(0.0, 1.0, weight);
    }

    if (adjacentBefore && sampleIndex >= start - fadeInSamples && sampleIndex < start)
        return juce::jlimit(0.0, 1.0, static_cast<double>(sampleIndex - (start - fadeInSamples)) / juce::jmax(1, fadeInSamples));

    if (adjacentAfter && sampleIndex >= end && sampleIndex < end + fadeOutSamples)
        return juce::jlimit(0.0, 1.0, 1.0 - static_cast<double>(sampleIndex - end) / juce::jmax(1, fadeOutSamples));

    return 0.0;
}

double LegacyEasyWaveform104::breathNormGainForRegion(int regionIndex) const
{
    if (regionIndex >= 0 && regionIndex < breathNormGainCache.size())
        return breathNormGainCache.getReference(regionIndex);

    return 1.0;
}

double LegacyEasyWaveform104::displayRegionWeightAtSample(const DisplayRegion& region,
                                                             juce::int64 sampleIndex,
                                                             int fadeInSamples,
                                                             int fadeOutSamples) const
{
    const auto start = region.startSample;
    const auto end = region.endSample;

    if (sampleIndex >= start && sampleIndex < end)
    {
        auto weight = 1.0;
        if (fadeInSamples > 0 && ! region.adjacentBefore)
            weight = juce::jmin(weight, static_cast<double>(sampleIndex - start) / juce::jmax(1, fadeInSamples - 1));

        if (fadeOutSamples > 0 && ! region.adjacentAfter)
            weight = juce::jmin(weight, static_cast<double>(end - 1 - sampleIndex) / juce::jmax(1, fadeOutSamples - 1));

        return juce::jlimit(0.0, 1.0, weight);
    }

    if (region.adjacentBefore && sampleIndex >= start - fadeInSamples && sampleIndex < start)
        return juce::jlimit(0.0, 1.0, static_cast<double>(sampleIndex - (start - fadeInSamples)) / juce::jmax(1, fadeInSamples));

    if (region.adjacentAfter && sampleIndex >= end && sampleIndex < end + fadeOutSamples)
        return juce::jlimit(0.0, 1.0, 1.0 - static_cast<double>(sampleIndex - end) / juce::jmax(1, fadeOutSamples));

    return 0.0;
}

double LegacyEasyWaveform104::renderedDisplaySample(juce::int64 sampleIndex, bool activeComponents) const
{
    if (sampleIndex < 0 || sampleIndex >= monoWaveform.getNumSamples())
        return 0.0;

    const auto dry = static_cast<double>(monoWaveform.getSample(0, static_cast<int>(sampleIndex)));
    const auto fadeInSamples = processingParams.enableFade && sampleRate > 0.0
                             ? static_cast<int>(std::llround(processingParams.fadeInMs * sampleRate / 1000.0)) : 0;
    const auto fadeOutSamples = processingParams.enableFade && sampleRate > 0.0
                              ? static_cast<int>(std::llround(processingParams.fadeOutMs * sampleRate / 1000.0)) : 0;
    auto breathWeight = 0.0;
    auto noiseWeight = 0.0;
    auto sibilanceWeight = 0.0;
    auto othersWeight = 0.0;

    for (const auto& region : displayRegions)
    {
        if (sampleIndex < region.startSample - fadeInSamples)
            break;
        if (sampleIndex >= region.endSample + fadeOutSamples)
            continue;
        const auto weight = displayRegionWeightAtSample(region, sampleIndex, fadeInSamples, fadeOutSamples);
        const auto type = qqNormalizedRegionType(region.type);
        if (type == "Noise") noiseWeight = juce::jmax(noiseWeight, weight);
        else if (type == "Sibilance") sibilanceWeight = juce::jmax(sibilanceWeight, weight);
        else if (type == "Others") othersWeight = juce::jmax(othersWeight, weight);
        else breathWeight = juce::jmax(breathWeight, weight);
    }

    const auto regionWeightSum = breathWeight + noiseWeight + sibilanceWeight + othersWeight;
    const auto scale = regionWeightSum > 1.0 ? 1.0 / regionWeightSum : 1.0;
    breathWeight *= scale; noiseWeight *= scale; sibilanceWeight *= scale; othersWeight *= scale;
    const auto voiceWeight = juce::jlimit(0.0, 1.0, 1.0 - breathWeight - noiseWeight - sibilanceWeight - othersWeight);
    auto mixed = 0.0;
    if (monitorVoice == activeComponents) mixed += dry * voiceWeight;
    if (monitorNoize == activeComponents) mixed += dry * noiseWeight;
    if (monitorBreath == activeComponents)
        mixed += processedBreathDisplay.getNumSamples() == monoWaveform.getNumSamples()
               ? processedBreathDisplay.getSample(0, static_cast<int>(sampleIndex))
               : dry * breathWeight;
    if (monitorSibilance == activeComponents)
        mixed += processedSibilanceDisplay.getNumSamples() == monoWaveform.getNumSamples()
               ? processedSibilanceDisplay.getSample(0, static_cast<int>(sampleIndex))
               : dry * sibilanceWeight;
    if (monitorOthers == activeComponents)
        mixed += processedOthersDisplay.getNumSamples() == monoWaveform.getNumSamples()
               ? processedOthersDisplay.getSample(0, static_cast<int>(sampleIndex))
               : dry * othersWeight;
    return mixed;
}

void LegacyEasyWaveform104::drawFadeGuides(juce::Graphics& g, juce::Rectangle<int> area) const
{
    if (! processingParams.enableFade || sampleRate <= 0.0 || displayRegions.isEmpty())
        return;

    const auto fadeInSamples = static_cast<juce::int64>(std::llround(processingParams.fadeInMs * sampleRate / 1000.0));
    const auto fadeOutSamples = static_cast<juce::int64>(std::llround(processingParams.fadeOutMs * sampleRate / 1000.0));
    if (fadeInSamples <= 0 && fadeOutSamples <= 0)
        return;

    const auto top = static_cast<float>(area.getY() + 4);
    const auto bottom = static_cast<float>(area.getBottom() - 4);
    g.setColour(juce::Colour(0xffe0f2fe).withAlpha(0.50f));

    for (const auto& region : displayRegions)
    {
        const auto startSeconds = static_cast<double>(region.startSample) / sampleRate;
        const auto endSeconds = static_cast<double>(region.endSample) / sampleRate;

        if (fadeInSamples > 0 && ! region.adjacentBefore)
        {
            const auto x1 = static_cast<float>(timeToX(startSeconds));
            const auto x2 = static_cast<float>(timeToX(static_cast<double>(region.startSample + fadeInSamples) / sampleRate));
            if (x2 >= area.getX() && x1 <= area.getRight())
                g.drawLine(x1, bottom, x2, top, 1.0f);
        }

        if (region.adjacentAfter && fadeOutSamples > 0)
        {
            const auto x1 = static_cast<float>(timeToX(static_cast<double>(region.endSample - fadeOutSamples) / sampleRate));
            const auto x2 = static_cast<float>(timeToX(static_cast<double>(region.endSample + fadeInSamples) / sampleRate));
            if (x2 >= area.getX() && x1 <= area.getRight())
            {
                g.drawLine(x1, top, x2, bottom, 1.0f);
                g.drawLine(x1, bottom, x2, top, 1.0f);
            }
        }
        else if (fadeOutSamples > 0)
        {
            const auto x1 = static_cast<float>(timeToX(static_cast<double>(region.endSample - fadeOutSamples) / sampleRate));
            const auto x2 = static_cast<float>(timeToX(endSeconds));
            if (x2 >= area.getX() && x1 <= area.getRight())
                g.drawLine(x1, top, x2, bottom, 1.0f);
        }
    }
}

void LegacyEasyWaveform104::rebuildBreathNormGainCache()
{
    breathNormGainCache.clear();
    displayRegions.clear();

    for (auto regionIndex = 0; regionIndex < regions.size(); ++regionIndex)
    {
        const auto& region = regions.getReference(regionIndex);
        auto gain = 1.0;
        const auto normalized = qqNormalizedRegionType(region.type);
        if (sampleRate > 0.0 && monoWaveform.getNumSamples() > 0 && (normalized == "Breath" || normalized == "Sibilance"))
        {
            const auto start = regionStartSample(region);
            const auto end = regionEndSample(region);
            auto peak = 0.0f;
            const auto* data = monoWaveform.getReadPointer(0);

            for (auto sample = start; sample < end; ++sample)
                peak = juce::jmax(peak, std::abs(data[static_cast<int>(sample)]));

            if (peak > 1.0e-9f)
                gain = dbToGain(normalized == "Breath" ? processingParams.breathTargetDb : processingParams.sibilanceTargetDb) / static_cast<double>(peak);
        }

        breathNormGainCache.add(gain);

        DisplayRegion displayRegion;
        displayRegion.sourceIndex = regionIndex;
        displayRegion.type = qqNormalizedRegionType(region.type);
        displayRegion.startSample = regionStartSample(region);
        displayRegion.endSample = regionEndSample(region);
        displayRegion.normGain = gain;
        displayRegions.add(displayRegion);
    }

    for (auto i = 0; i < displayRegions.size(); ++i)
        for (auto j = i + 1; j < displayRegions.size(); ++j)
            if (displayRegions.getReference(j).startSample < displayRegions.getReference(i).startSample)
                displayRegions.swap(i, j);

    const auto fadeTolerance = processingParams.enableFade && sampleRate > 0.0
                             ? juce::jmax<juce::int64>(1, static_cast<juce::int64>(std::llround(juce::jmax(processingParams.fadeInMs, processingParams.fadeOutMs) * sampleRate / 1000.0)))
                             : 1;

    for (auto i = 0; i < displayRegions.size(); ++i)
    {
        auto& region = displayRegions.getReference(i);
        for (auto j = 0; j < displayRegions.size(); ++j)
        {
            if (i == j)
                continue;

            const auto& other = displayRegions.getReference(j);
            const auto beforeDistance = other.endSample > region.startSample ? other.endSample - region.startSample
                                                                             : region.startSample - other.endSample;
            const auto afterDistance = other.startSample > region.endSample ? other.startSample - region.endSample
                                                                            : region.endSample - other.startSample;
            if (beforeDistance <= fadeTolerance)
                region.adjacentBefore = true;
            if (afterDistance <= fadeTolerance)
                region.adjacentAfter = true;
        }
    }

    rebuildProcessedBreathDisplay();
}

double LegacyEasyWaveform104::dbToGain(double db)
{
    return std::pow(10.0, db / 20.0);
}

juce::String LegacyEasyWaveform104::buildProcessedBreathDisplayKey() const
{
    juce::String key;
    key << "samples=" << monoWaveform.getNumSamples()
        << "|sr=" << juce::String(sampleRate, 6)
        << "|fade=" << (processingParams.enableFade ? 1 : 0)
        << "|" << juce::String(processingParams.fadeInMs, 4)
        << "|" << juce::String(processingParams.fadeOutMs, 4)
        << "|bnorm=" << (processingParams.normalizeBreath ? 1 : 0)
        << "|btarget=" << juce::String(processingParams.breathTargetDb, 4)
        << "|snorm=" << (processingParams.normalizeSibilance ? 1 : 0)
        << "|starget=" << juce::String(processingParams.sibilanceTargetDb, 4)
        << "|bgain=" << juce::String(processingParams.breathGainDb, 4)
        << "|sgain=" << juce::String(processingParams.sibilanceGainDb, 4)
        << "|beq=" << serializeBreathEqState(processingParams.breathEqState)
        << "|seq=" << serializeBreathEqState(processingParams.sibilanceEqState)
;

    for (const auto& region : regions)
    {
        key << "|" << region.type
            << ":" << juce::String(region.startSample)
            << "-" << juce::String(region.endSample)
            << ":" << juce::String(region.startTime, 6)
            << "-" << juce::String(region.endTime, 6)
            << ":g" << juce::String(region.gainDb, 3)
            << ":eq" << serializeBreathEqState(region.eqState);
    }

    return key;
}

void LegacyEasyWaveform104::rebuildProcessedBreathDisplay()
{
    const auto key = buildProcessedBreathDisplayKey();
    if (key == processedBreathDisplayKey && processedBreathDisplay.getNumSamples() == monoWaveform.getNumSamples())
        return;

    processedBreathDisplayKey = key;
    auto needsBreathBuffer = processingParams.normalizeBreath
                          || std::abs(processingParams.breathGainDb) > 0.001
                          || processingParams.breathEqState.hasActiveProcessing();
    auto needsSibilanceBuffer = processingParams.normalizeSibilance
                              || std::abs(processingParams.sibilanceGainDb) > 0.001
                              || processingParams.sibilanceEqState.hasActiveProcessing();
    auto needsOthersBuffer = false;
    for (const auto& region : regions)
    {
        const auto hasLocalProcessing = std::abs(region.gainDb) > 0.001 || region.eqState.hasActiveProcessing();
        if (! hasLocalProcessing)
            continue;
        const auto type = qqNormalizedRegionType(region.type);
        if (type == "Breath") needsBreathBuffer = true;
        else if (type == "Sibilance") needsSibilanceBuffer = true;
        else if (type == "Others") needsOthersBuffer = true;
    }

    const auto prepareBuffer = [this] (juce::AudioBuffer<float>& buffer, bool needed)
    {
        if (! needed)
        {
            buffer.setSize(0, 0);
            return;
        }
        buffer.setSize(1, monoWaveform.getNumSamples(), false, false, true);
        buffer.clear();
    };
    prepareBuffer(processedBreathDisplay, needsBreathBuffer);
    prepareBuffer(processedSibilanceDisplay, needsSibilanceBuffer);
    prepareBuffer(processedOthersDisplay, needsOthersBuffer);

    if (monoWaveform.getNumSamples() <= 0 || sampleRate <= 0.0 || displayRegions.isEmpty()
        || (! needsBreathBuffer && ! needsSibilanceBuffer && ! needsOthersBuffer))
        return;

    const auto fadeInSamples = processingParams.enableFade ? static_cast<int>(std::llround(processingParams.fadeInMs * sampleRate / 1000.0)) : 0;
    const auto fadeOutSamples = processingParams.enableFade ? static_cast<int>(std::llround(processingParams.fadeOutMs * sampleRate / 1000.0)) : 0;

    for (auto regionIndex = 0; regionIndex < regions.size(); ++regionIndex)
    {
        const auto& region = regions.getReference(regionIndex);
        const auto type = qqNormalizedRegionType(region.type);
        if (type == "Noise")
            continue;
        if ((type == "Breath" && ! needsBreathBuffer)
            || (type == "Sibilance" && ! needsSibilanceBuffer)
            || (type == "Others" && ! needsOthersBuffer))
            continue;
        const auto start = juce::jmax<juce::int64>(0, regionStartSample(region) - fadeInSamples);
        const auto end = juce::jmin<juce::int64>(monoWaveform.getNumSamples(), regionEndSample(region) + fadeOutSamples);
        if (end <= start)
            continue;
        juce::AudioBuffer<float> regionBuffer(1, static_cast<int>(end - start));
        regionBuffer.clear();
        const auto globalGain = type == "Breath" ? dbToGain(processingParams.breathGainDb)
                              : type == "Sibilance" ? dbToGain(processingParams.sibilanceGainDb)
                              : 1.0;
        const auto normGain = ((type == "Breath" && processingParams.normalizeBreath) || (type == "Sibilance" && processingParams.normalizeSibilance)) ? breathNormGainForRegion(regionIndex) : 1.0;
        const auto localGain = dbToGain(juce::jlimit(-30.0, 30.0, region.gainDb));
        for (auto i = 0; i < regionBuffer.getNumSamples(); ++i)
        {
            const auto sourceSample = start + i;
            const auto weight = regionWeightAtSample(regionIndex, sourceSample, fadeInSamples, fadeOutSamples);
            const auto dry = monoWaveform.getSample(0, static_cast<int>(sourceSample));
            regionBuffer.setSample(0, i, static_cast<float>(dry * weight * normGain * globalGain * localGain));
        }
        const auto* globalEq = type == "Breath" ? &processingParams.breathEqState
                             : type == "Sibilance" ? &processingParams.sibilanceEqState
                             : nullptr;
        if (globalEq != nullptr && globalEq->hasActiveProcessing())
        {
            QQDeBreathEqProcessor processor; processor.prepare(sampleRate, 1, *globalEq); processor.process(regionBuffer);
        }
        if (region.eqState.hasActiveProcessing())
        {
            QQDeBreathEqProcessor processor; processor.prepare(sampleRate, 1, region.eqState); processor.process(regionBuffer);
        }
        auto* dest = type == "Breath" ? &processedBreathDisplay
                   : type == "Sibilance" ? &processedSibilanceDisplay
                   : &processedOthersDisplay;
        dest->addFrom(0, static_cast<int>(start), regionBuffer, 0, 0, regionBuffer.getNumSamples());
    }
}
