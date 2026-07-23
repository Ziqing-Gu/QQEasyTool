#include "ARAProcessor.h"

#include "ARAEditor.h"

#include <cmath>

namespace
{
constexpr auto araStateMagic = "QQDeBreathARAState";
constexpr int araStateVersion = 1;

float getBufferMagnitude(const juce::AudioBuffer<float>& buffer)
{
    float magnitude = 0.0f;
    for (auto channel = 0; channel < buffer.getNumChannels(); ++channel)
        magnitude = juce::jmax(magnitude, buffer.getMagnitude(channel, 0, buffer.getNumSamples()));

    return magnitude;
}

class QQDeBreathARAPlaybackRenderer final : public juce::ARAPlaybackRenderer
{
public:
    using juce::ARAPlaybackRenderer::ARAPlaybackRenderer;

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
    }

    bool processBlock(juce::AudioBuffer<float>& buffer,
                      juce::AudioProcessor::Realtime /*realtime*/,
                      const juce::AudioPlayHead::PositionInfo& positionInfo) noexcept override
    {
        buffer.clear();

        if (! positionInfo.getIsPlaying() || sampleRate <= 0.0)
            return true;

        const auto timeInSeconds = positionInfo.getTimeInSeconds();
        if (! timeInSeconds.hasValue())
            return true;

        const auto blockStart = static_cast<juce::int64>(std::llround(*timeInSeconds * sampleRate));
        const auto blockEnd = blockStart + buffer.getNumSamples();

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

            if (! juce::approximatelyEqual(audioSource->getSampleRate(), sampleRate))
                continue;

            const auto regionStart = playbackRegion->getStartInPlaybackSamples(sampleRate);
            const auto regionEnd = playbackRegion->getEndInPlaybackSamples(sampleRate);
            if (blockEnd <= regionStart || regionEnd <= blockStart)
                continue;

            auto copyStartInSong = juce::jmax<juce::int64>(blockStart, regionStart);
            auto copyEndInSong = juce::jmin<juce::int64>(blockEnd, regionEnd);

            const auto offsetToSource = playbackRegion->getStartInAudioModificationSamples() - regionStart;
            const auto sourceStartLimit = juce::jmax<juce::int64>(0, playbackRegion->getStartInAudioModificationSamples());
            const auto sourceEndLimit = juce::jmin<juce::int64>(audioSource->getSampleCount(), playbackRegion->getEndInAudioModificationSamples());

            copyStartInSong = juce::jmax<juce::int64>(copyStartInSong, sourceStartLimit - offsetToSource);
            copyEndInSong = juce::jmin<juce::int64>(copyEndInSong, sourceEndLimit - offsetToSource);
            if (copyEndInSong <= copyStartInSong)
                continue;

            const auto destOffset = static_cast<int>(copyStartInSong - blockStart);
            const auto numSamples = static_cast<int>(copyEndInSong - copyStartInSong);
            const auto sourceStart = copyStartInSong + offsetToSource;

            tempBuffer.clear();
            juce::ARAAudioSourceReader reader(audioSource);
            if (! reader.read(&tempBuffer, 0, numSamples, sourceStart, true, true))
                continue;

            for (auto channel = 0; channel < buffer.getNumChannels(); ++channel)
                buffer.addFrom(channel, destOffset, tempBuffer, juce::jmin(channel, tempBuffer.getNumChannels() - 1), 0, numSamples);
        }

        return true;
    }

private:
    double sampleRate = 0.0;
    int numChannels = 0;
    juce::AudioBuffer<float> tempBuffer;
};
} // namespace

QQDeBreathARAAudioProcessor::QQDeBreathARAAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
}

void QQDeBreathARAAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    prepareToPlayForARA(sampleRate, samplesPerBlock, getMainBusNumOutputChannels(), getProcessingPrecision());
    araDryBuffer.setSize(getMainBusNumOutputChannels(), samplesPerBlock);
}

void QQDeBreathARAAudioProcessor::releaseResources()
{
    releaseResourcesForARA();
}

bool QQDeBreathARAAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& input = layouts.getMainInputChannelSet();
    const auto& output = layouts.getMainOutputChannelSet();

    if (input.isDisabled() || output.isDisabled())
        return false;

    if (input != output)
        return false;

    return input == juce::AudioChannelSet::mono() || input == juce::AudioChannelSet::stereo();
}

void QQDeBreathARAAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused(midiMessages);

    if (araDryBuffer.getNumChannels() != buffer.getNumChannels() || araDryBuffer.getNumSamples() < buffer.getNumSamples())
        araDryBuffer.setSize(buffer.getNumChannels(), buffer.getNumSamples(), false, false, true);

    const auto inputMagnitude = getBufferMagnitude(buffer);
    if (inputMagnitude > 0.000001f)
        araDryBuffer.makeCopyOf(buffer, true);

    auto* playHead = getPlayHead();
    if (processBlockForARA(buffer, isRealtime(), playHead))
    {
        if (inputMagnitude > 0.000001f && getBufferMagnitude(buffer) <= 0.000001f)
        {
            for (auto channel = 0; channel < buffer.getNumChannels(); ++channel)
                buffer.copyFrom(channel, 0, araDryBuffer, channel, 0, buffer.getNumSamples());
        }

        return;
    }

    for (auto channel = getTotalNumInputChannels(); channel < getTotalNumOutputChannels(); ++channel)
        buffer.clear(channel, 0, buffer.getNumSamples());
}

juce::AudioProcessorEditor* QQDeBreathARAAudioProcessor::createEditor()
{
    return new QQDeBreathARAAudioProcessorEditor(*this);
}

double QQDeBreathARAAudioProcessor::getTailLengthSeconds() const
{
    double tail = 0.0;
    if (getTailLengthSecondsForARA(tail))
        return tail;

    return 0.0;
}

void QQDeBreathARAAudioProcessor::setCurrentProgram(int /*index*/)
{
}

const juce::String QQDeBreathARAAudioProcessor::getProgramName(int /*index*/)
{
    return {};
}

void QQDeBreathARAAudioProcessor::changeProgramName(int /*index*/, const juce::String& /*newName*/)
{
}

void QQDeBreathARAAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    juce::MemoryOutputStream stream(destData, false);
    stream.writeString(araStateMagic);
    stream.writeInt(araStateVersion);

    const juce::ScopedLock lock(analysisLock);
    stream.writeString(QQDeBreathBridgeAnalysis::serializeResult(analysisResult));
}

void QQDeBreathARAAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    juce::MemoryInputStream stream(data, static_cast<size_t>(sizeInBytes), false);
    const auto magic = stream.readString();

    if (magic != araStateMagic)
        return;

    const auto version = stream.readInt();
    if (version < 1 || stream.isExhausted())
        return;

    QQDeBreathBridgeAnalysisResult restored;
    if (QQDeBreathBridgeAnalysis::deserializeResult(stream.readString(), restored))
        setAnalysisResult(restored);
}

void QQDeBreathARAAudioProcessor::setAnalysisResult(const QQDeBreathBridgeAnalysisResult& result)
{
    const juce::ScopedLock lock(analysisLock);
    analysisResult = result;
}

QQDeBreathBridgeAnalysisResult QQDeBreathARAAudioProcessor::getAnalysisResult() const
{
    const juce::ScopedLock lock(analysisLock);
    return analysisResult;
}

void QQDeBreathARAAudioProcessor::clearAnalysisResult()
{
    const juce::ScopedLock lock(analysisLock);
    analysisResult = {};
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new QQDeBreathARAAudioProcessor();
}

const ARA::ARAFactory* JUCE_CALLTYPE createARAFactory()
{
    return juce::ARADocumentControllerSpecialisation::createARAFactory<QQDeBreathARADocumentController>();
}

bool QQDeBreathARADocumentController::doRestoreObjectsFromStream(juce::ARAInputStream& input,
                                                                 const juce::ARARestoreObjectsFilter* filter) noexcept
{
    juce::ignoreUnused(input, filter);
    return true;
}

juce::ARAPlaybackRenderer* QQDeBreathARADocumentController::doCreatePlaybackRenderer()
{
    return new QQDeBreathARAPlaybackRenderer(getDocumentController());
}

bool QQDeBreathARADocumentController::doStoreObjectsToStream(juce::ARAOutputStream& output,
                                                             const juce::ARAStoreObjectsFilter* filter) noexcept
{
    juce::ignoreUnused(output, filter);
    return true;
}
