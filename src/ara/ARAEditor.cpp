#include "ARAEditor.h"

#include "shared/NativeAnalysis.h"

#include <cstdint>

namespace
{
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

void setupLabel(juce::Label& label, float size, juce::Colour colour, juce::Justification justification = juce::Justification::centredLeft)
{
    label.setFont(juce::Font(size, juce::Font::plain));
    label.setColour(juce::Label::textColourId, colour);
    label.setJustificationType(justification);
}

void setupButton(juce::TextButton& button, juce::Colour colour)
{
    button.setColour(juce::TextButton::buttonColourId, colour);
    button.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
}
} // namespace

class QQDeBreathARAAudioProcessorEditor::AnalysisThread final : public juce::Thread
{
public:
    AnalysisThread(QQDeBreathARAAudioProcessorEditor& ownerIn, QQDeBreathBridgeAnalysisConfig configIn)
        : juce::Thread("QQDeBreath ARA Native Analysis"),
          owner(&ownerIn),
          config(std::move(configIn))
    {
    }

    ~AnalysisThread() override
    {
        signalThreadShouldExit();
        waitForThreadToExit(1500);
    }

    void run() override
    {
        auto result = QQDeBreathNativeAnalysis::run(config, [this] { return threadShouldExit(); });

        juce::MessageManager::callAsync([safeOwner = owner, result]
        {
            if (safeOwner != nullptr)
                safeOwner->handleAnalysisFinished(result);
        });
    }

private:
    juce::Component::SafePointer<QQDeBreathARAAudioProcessorEditor> owner;
    QQDeBreathBridgeAnalysisConfig config;
};

QQDeBreathARAAudioProcessorEditor::QQDeBreathARAAudioProcessorEditor(QQDeBreathARAAudioProcessor& processor)
    : AudioProcessorEditor(&processor),
      AudioProcessorEditorARAExtension(&processor),
      audioProcessor(processor)
{
    juce::ignoreUnused(audioProcessor);

    titleLabel.setText("QQDeBreath ARA", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(26.0f, juce::Font::bold));
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(titleLabel);

    phaseLabel.setText("ARA 1.02 Native C++ Analyzer", juce::dontSendNotification);
    setupLabel(phaseLabel, 15.0f, juce::Colour(0xff93c5fd));
    addAndMakeVisible(phaseLabel);

    reloadButton.onClick = [this] { reloadSource(); };
    setupButton(reloadButton, juce::Colour(0xff2563eb));
    addAndMakeVisible(reloadButton);

    analyzeButton.onClick = [this] { startAnalysis(); };
    setupButton(analyzeButton, juce::Colour(0xff7c3aed));
    addAndMakeVisible(analyzeButton);

    cancelAnalyzeButton.onClick = [this] { cancelAnalysis(); };
    setupButton(cancelAnalyzeButton, juce::Colour(0xffb45309));
    addAndMakeVisible(cancelAnalyzeButton);

    clearAnalysisButton.onClick = [this] { clearAnalysis(); };
    setupButton(clearAnalysisButton, juce::Colour(0xff475569));
    addAndMakeVisible(clearAnalysisButton);

    for (auto* label : { &sourceNameLabel, &sourceIdLabel, &sampleRateLabel, &channelsLabel,
                         &samplesLabel, &durationLabel, &exportPathLabel, &analysisStatusLabel,
                         &breathCountLabel, &noizeCountLabel, &analysisErrorLabel,
                         &sourceMismatchLabel, &statusLabel })
    {
        setupLabel(*label, 13.0f, juce::Colour(0xffcbd5e1));
        addAndMakeVisible(*label);
    }

    statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xfffacc15));
    analysisErrorLabel.setColour(juce::Label::textColourId, juce::Colour(0xfffca5a5));
    sourceMismatchLabel.setColour(juce::Label::textColourId, juce::Colour(0xfffacc15));
    monitorVoiceButton.setToggleState(true, juce::dontSendNotification);
    monitorBreathButton.setToggleState(true, juce::dontSendNotification);
    monitorNoizeButton.setToggleState(true, juce::dontSendNotification);
    for (auto* button : { &monitorVoiceButton, &monitorBreathButton, &monitorNoizeButton })
    {
        button->setColour(juce::ToggleButton::textColourId, juce::Colour(0xffcbd5e1));
        addAndMakeVisible(*button);
    }
    waveformEditor.onRegionsChanged = [this](const juce::Array<QQDeBreathBridgeRegion>& regions)
    {
        waveformRegionsChanged(regions);
    };
    waveformEditor.onSeekRequested = [this](double localSeconds)
    {
        waveformSeekRequested(localSeconds);
    };
    addAndMakeVisible(waveformEditor);
    waveformEditor.setAnalysisResult(audioProcessor.getAnalysisResult());
    updateLabels();
    startTimerHz(30);
    setSize(920, 720);
}

QQDeBreathARAAudioProcessorEditor::~QQDeBreathARAAudioProcessorEditor()
{
    cancelAnalysis();
}

void QQDeBreathARAAudioProcessorEditor::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    juce::ColourGradient gradient(juce::Colour(0xff111827), bounds.getTopLeft(),
                                  juce::Colour(0xff020617), bounds.getBottomRight(), false);
    g.setGradientFill(gradient);
    g.fillAll();

    g.setColour(juce::Colour(0xff334155));
    g.drawRoundedRectangle(bounds.reduced(12.0f), 8.0f, 1.0f);
}

void QQDeBreathARAAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(24);
    auto top = area.removeFromTop(58);
    reloadButton.setBounds(top.removeFromRight(150).reduced(0, 8));
    titleLabel.setBounds(top.removeFromTop(32));
    phaseLabel.setBounds(top);

    area.removeFromTop(14);
    auto analysisControls = area.removeFromTop(38);
    analyzeButton.setBounds(analysisControls.removeFromLeft(112).reduced(4, 3));
    cancelAnalyzeButton.setBounds(analysisControls.removeFromLeft(112).reduced(4, 3));
    clearAnalysisButton.setBounds(analysisControls.removeFromLeft(150).reduced(4, 3));
    analysisControls.removeFromLeft(12);
    monitorVoiceButton.setBounds(analysisControls.removeFromLeft(86).reduced(4, 3));
    monitorBreathButton.setBounds(analysisControls.removeFromLeft(94).reduced(4, 3));
    monitorNoizeButton.setBounds(analysisControls.removeFromLeft(92).reduced(4, 3));
    area.removeFromTop(8);

    waveformEditor.setBounds(area.removeFromTop(250));
    area.removeFromTop(10);

    const auto rowHeight = 27;
    sourceNameLabel.setBounds(area.removeFromTop(rowHeight));
    sourceIdLabel.setBounds(area.removeFromTop(rowHeight));
    sampleRateLabel.setBounds(area.removeFromTop(rowHeight));
    channelsLabel.setBounds(area.removeFromTop(rowHeight));
    samplesLabel.setBounds(area.removeFromTop(rowHeight));
    durationLabel.setBounds(area.removeFromTop(rowHeight));
    area.removeFromTop(10);
    exportPathLabel.setBounds(area.removeFromTop(rowHeight * 2));
    analysisStatusLabel.setBounds(area.removeFromTop(rowHeight));
    breathCountLabel.setBounds(area.removeFromTop(rowHeight));
    noizeCountLabel.setBounds(area.removeFromTop(rowHeight));
    sourceMismatchLabel.setBounds(area.removeFromTop(rowHeight));
    analysisErrorLabel.setBounds(area.removeFromTop(rowHeight * 2));
    statusLabel.setBounds(area.removeFromTop(rowHeight * 2));
}

void QQDeBreathARAAudioProcessorEditor::timerCallback()
{
    updatePlayheadFromHost();
    updateAnalysisLabels();
}

void QQDeBreathARAAudioProcessorEditor::updatePlayheadFromHost()
{
    auto* playHead = audioProcessor.getPlayHead();
    if (playHead == nullptr)
        return;

    const auto position = playHead->getPosition();
    if (! position.hasValue())
        return;

    const auto hostTime = position->getTimeInSeconds();
    if (! hostTime.hasValue())
        return;

    double localTime = *hostTime;

    if (auto* editorView = getARAEditorView())
    {
        const auto selectedRegions = editorView->getViewSelection().getEffectivePlaybackRegions<juce::ARAPlaybackRegion>();
        if (! selectedRegions.empty() && selectedRegions.front() != nullptr)
        {
            auto* playbackRegion = selectedRegions.front();
            const auto playbackRange = playbackRegion->getTimeRange();
            localTime = *hostTime - playbackRange.getStart() + playbackRegion->getStartInAudioModificationTime();
        }
    }

    waveformEditor.setPlayheadSeconds(localTime);
}

void QQDeBreathARAAudioProcessorEditor::reloadSource()
{
    sourceInfo = {};
    audioProcessor.clearAnalysisResult();

    auto* source = findCurrentAudioSource();
    if (source == nullptr)
    {
        sourceInfo.exportStatus = "No ARA audio source found. Apply this extension to an audio event, then reload.";
        updateLabels();
        return;
    }

    sourceInfo.name = makeSourceName(*source);
    sourceInfo.persistentId = juce::String::fromUTF8(source->getPersistentID().c_str());
    sourceInfo.sourceFingerprint = buildSourceFingerprint(*source);
    sourceInfo.sampleRate = source->getSampleRate();
    sourceInfo.channelCount = static_cast<int>(source->getChannelCount());
    sourceInfo.numSamples = static_cast<juce::int64>(source->getSampleCount());
    sourceInfo.durationSeconds = sourceInfo.sampleRate > 0.0 ? static_cast<double>(sourceInfo.numSamples) / sourceInfo.sampleRate : 0.0;

    if (exportSourceToWav(*source, sourceInfo))
    {
        sourceInfo.exportStatus = "Source reloaded and temp wav exported.";
        juce::String waveformStatus;
        waveformEditor.loadAudioFile(sourceInfo.exportedWav, waveformStatus);
    }

    updateLabels();
}

void QQDeBreathARAAudioProcessorEditor::startAnalysis()
{
    if (! sourceInfo.exportedWav.existsAsFile())
    {
        sourceInfo.exportStatus = "Reload Source first, then analyze.";
        updateLabels();
        return;
    }

    const auto sourceKey = sourceInfo.sourceFingerprint.isNotEmpty()
                         ? sourceInfo.sourceFingerprint
                         : sourceInfo.exportedWav.getFileNameWithoutExtension();
    const auto analysisDir = getAnalysisDirectory(sourceKey);

    QQDeBreathBridgeAnalysisConfig config;
    config.inputWav = sourceInfo.exportedWav;
    config.outputJson = analysisDir.getChildFile("result.json");
    config.outDir = analysisDir.getChildFile("stems");
    config.cancelFile = analysisDir.getChildFile("cancel.flag");
    config.sourceKey = sourceKey;

    QQDeBreathBridgeAnalysisResult pending;
    pending.status = "Analysis running...";
    pending.sourceKey = sourceKey;
    audioProcessor.setAnalysisResult(pending);

    analysisThread = std::make_unique<AnalysisThread>(*this, config);
    analysisThread->startThread();
    updateLabels();
}

void QQDeBreathARAAudioProcessorEditor::cancelAnalysis()
{
    if (analysisThread != nullptr)
    {
        analysisThread->signalThreadShouldExit();
        QQDeBreathBridgeAnalysisResult cancelling;
        cancelling.status = "Cancelling analysis...";
        audioProcessor.setAnalysisResult(cancelling);
    }

    updateAnalysisLabels();
}

void QQDeBreathARAAudioProcessorEditor::clearAnalysis()
{
    cancelAnalysis();
    audioProcessor.clearAnalysisResult();
    waveformEditor.setAnalysisResult({});
    updateAnalysisLabels();
}

void QQDeBreathARAAudioProcessorEditor::handleAnalysisFinished(const QQDeBreathBridgeAnalysisResult& result)
{
    audioProcessor.setAnalysisResult(result);
    waveformEditor.setAnalysisResult(result);
    analysisThread.reset();
    updateLabels();
}

void QQDeBreathARAAudioProcessorEditor::waveformRegionsChanged(const juce::Array<QQDeBreathBridgeRegion>& regions)
{
    auto result = audioProcessor.getAnalysisResult();
    result.hasResult = true;
    result.succeeded = true;
    result.status = "Analysis edited.";
    result.regions = regions;
    result.breathCount = 0;
    result.noizeCount = 0;

    for (const auto& region : regions)
    {
        if (region.type.equalsIgnoreCase("Breath"))
            ++result.breathCount;
        else if (region.type.equalsIgnoreCase("Noize"))
            ++result.noizeCount;
    }

    audioProcessor.setAnalysisResult(result);
    updateAnalysisLabels();
}

void QQDeBreathARAAudioProcessorEditor::waveformSeekRequested(double localSeconds)
{
    auto* editorView = getARAEditorView();
    if (editorView == nullptr)
    {
        statusLabel.setText("Status: ARA editor view is not available for seek.", juce::dontSendNotification);
        return;
    }

    auto* documentController = editorView->getDocumentController();
    if (documentController == nullptr || documentController->getHostPlaybackController() == nullptr)
    {
        statusLabel.setText("Status: Host playback controller is not available.", juce::dontSendNotification);
        return;
    }

    auto targetTime = localSeconds;
    const auto selectedRegions = editorView->getViewSelection().getEffectivePlaybackRegions<juce::ARAPlaybackRegion>();
    if (! selectedRegions.empty() && selectedRegions.front() != nullptr)
    {
        auto* playbackRegion = selectedRegions.front();
        targetTime = playbackRegion->getStartInPlaybackTime()
                   + juce::jmax(0.0, localSeconds - playbackRegion->getStartInAudioModificationTime());
    }

    documentController->getHostPlaybackController()->requestSetPlaybackPosition(targetTime);
    statusLabel.setText("Status: Requested DAW playhead seek to " + juce::String(targetTime, 3) + " s.", juce::dontSendNotification);
}

juce::ARAAudioSource* QQDeBreathARAAudioProcessorEditor::findCurrentAudioSource() const
{
    auto* editorView = getARAEditorView();
    if (editorView == nullptr)
        return nullptr;

    const auto selectedRegions = editorView->getViewSelection().getEffectivePlaybackRegions<juce::ARAPlaybackRegion>();
    if (! selectedRegions.empty())
        return selectedRegions.front()->getAudioModification()->getAudioSource();

    auto* documentController = editorView->getDocumentController();
    if (documentController == nullptr)
        return nullptr;

    auto* document = documentController->getDocument<juce::ARADocument>();
    if (document == nullptr)
        return nullptr;

    const auto& sources = document->getAudioSources<juce::ARAAudioSource>();
    return sources.empty() ? nullptr : sources.front();
}

bool QQDeBreathARAAudioProcessorEditor::exportSourceToWav(juce::ARAAudioSource& source, QQDeBreathARASourceInfo& info)
{
    if (! source.isSampleAccessEnabled())
    {
        info.exportStatus = "ARA sample access is not enabled by the host yet.";
        return false;
    }

    if (info.sampleRate <= 0.0 || info.channelCount <= 0 || info.numSamples <= 0)
    {
        info.exportStatus = "ARA source metadata is incomplete.";
        return false;
    }

    auto outDir = getExportDirectory();
    if (! outDir.createDirectory())
    {
        info.exportStatus = "Could not create temp export directory: " + outDir.getFullPathName();
        return false;
    }

    const auto safeName = info.name.fromLastOccurrenceOf("\\", false, false)
                              .fromLastOccurrenceOf("/", false, false)
                              .retainCharacters("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_ .")
                              .trim();
    const auto fileStem = safeName.isNotEmpty() ? safeName : "ara_source";
    const auto fileName = fileStem + "_" + info.sourceFingerprint + ".wav";
    auto outFile = outDir.getChildFile(fileName);
    outFile.deleteFile();

    juce::ARAAudioSourceReader reader(&source);
    if (! reader.isValid())
    {
        info.exportStatus = "Could not create ARA source reader.";
        return false;
    }

    juce::WavAudioFormat wavFormat;
    std::unique_ptr<juce::FileOutputStream> stream(outFile.createOutputStream());
    if (stream == nullptr)
    {
        info.exportStatus = "Could not open temp wav for writing: " + outFile.getFullPathName();
        return false;
    }

    std::unique_ptr<juce::AudioFormatWriter> writer(wavFormat.createWriterFor(stream.get(),
                                                                              info.sampleRate,
                                                                              static_cast<unsigned int>(info.channelCount),
                                                                              32,
                                                                              {},
                                                                              0));
    if (writer == nullptr)
    {
        info.exportStatus = "Could not create wav writer.";
        return false;
    }
    stream.release();

    constexpr int blockSize = 16384;
    juce::AudioBuffer<float> buffer(info.channelCount, blockSize);
    juce::int64 position = 0;

    while (position < info.numSamples)
    {
        const auto samplesThisBlock = static_cast<int>(std::min<juce::int64>(blockSize, info.numSamples - position));
        buffer.clear();

        if (! reader.read(&buffer, 0, samplesThisBlock, position, true, true))
        {
            info.exportStatus = "Failed while reading ARA source samples.";
            return false;
        }

        if (! writer->writeFromAudioSampleBuffer(buffer, 0, samplesThisBlock))
        {
            info.exportStatus = "Failed while writing temp wav.";
            return false;
        }

        position += samplesThisBlock;
    }

    writer.reset();
    info.exportedWav = outFile;
    return true;
}

void QQDeBreathARAAudioProcessorEditor::updateLabels()
{
    sourceNameLabel.setText("Source: " + (sourceInfo.name.isNotEmpty() ? sourceInfo.name : "(none)"), juce::dontSendNotification);
    sourceIdLabel.setText("Source ID/Fingerprint: " + (sourceInfo.sourceFingerprint.isNotEmpty() ? sourceInfo.sourceFingerprint : "(none)"), juce::dontSendNotification);
    sampleRateLabel.setText("Sample rate: " + juce::String(sourceInfo.sampleRate, 1) + " Hz", juce::dontSendNotification);
    channelsLabel.setText("Channels: " + juce::String(sourceInfo.channelCount), juce::dontSendNotification);
    samplesLabel.setText("Samples: " + juce::String(sourceInfo.numSamples), juce::dontSendNotification);
    durationLabel.setText("Duration: " + juce::String(sourceInfo.durationSeconds, 6) + " s", juce::dontSendNotification);
    exportPathLabel.setText("Temp wav: " + (sourceInfo.exportedWav.existsAsFile() ? sourceInfo.exportedWav.getFullPathName() : "(not exported)"), juce::dontSendNotification);
    statusLabel.setText("Status: " + sourceInfo.exportStatus, juce::dontSendNotification);
    updateAnalysisLabels();
}

void QQDeBreathARAAudioProcessorEditor::updateAnalysisLabels()
{
    const auto result = audioProcessor.getAnalysisResult();
    const auto analysisRunning = analysisThread != nullptr && analysisThread->isThreadRunning();

    analysisStatusLabel.setText("Analysis: " + (analysisRunning ? "Running..." : result.status), juce::dontSendNotification);
    breathCountLabel.setText("Breath regions: " + juce::String(result.breathCount), juce::dontSendNotification);
    noizeCountLabel.setText("Noize regions: " + juce::String(result.noizeCount), juce::dontSendNotification);
    analysisErrorLabel.setText("Error: " + (result.errorMessage.isNotEmpty() ? result.errorMessage : "(none)"), juce::dontSendNotification);

    const auto mismatch = result.hasResult
                       && sourceInfo.sourceFingerprint.isNotEmpty()
                       && result.sourceKey.isNotEmpty()
                       && result.sourceKey != sourceInfo.sourceFingerprint;
    sourceMismatchLabel.setText(mismatch ? "Source mismatch: ARA source changed after analysis. Re-analyze." : "Source mismatch: (none)",
                                juce::dontSendNotification);

    reloadButton.setEnabled(! analysisRunning);
    analyzeButton.setEnabled(! analysisRunning && sourceInfo.exportedWav.existsAsFile());
    cancelAnalyzeButton.setEnabled(analysisRunning);
    clearAnalysisButton.setEnabled(! analysisRunning && result.hasResult);
}

juce::String QQDeBreathARAAudioProcessorEditor::buildSourceFingerprint(const juce::ARAAudioSource& source)
{
    const auto id = juce::String::fromUTF8(source.getPersistentID().c_str());
    const auto seed = id + "|sr=" + juce::String(source.getSampleRate(), 8)
                    + "|ch=" + juce::String(static_cast<int>(source.getChannelCount()))
                    + "|samples=" + juce::String(static_cast<juce::int64>(source.getSampleCount()));

    return fnv1a64(seed);
}

juce::String QQDeBreathARAAudioProcessorEditor::makeSourceName(const juce::ARAAudioSource& source)
{
    if (auto name = source.getName())
        return juce::String::fromUTF8(name);

    return "ARA Audio Source";
}

juce::File QQDeBreathARAAudioProcessorEditor::getExportDirectory()
{
    return juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getChildFile("QQDeBreath")
        .getChildFile("ARA")
        .getChildFile("Phase6A");
}

juce::File QQDeBreathARAAudioProcessorEditor::getAnalysisDirectory(const juce::String& sourceKey)
{
    return juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getChildFile("QQDeBreath")
        .getChildFile("ARA")
        .getChildFile("Phase7B")
        .getChildFile(sourceKey);
}
