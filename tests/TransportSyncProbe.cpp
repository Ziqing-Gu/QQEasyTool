#include "PluginProcessor.h"
#include <cmath>
#include <iostream>

class QQDeBreathTransportSyncProbe
{
public:
    static float valueAt(juce::int64 sample, double sr)
    {
        return static_cast<float>(0.1 + 0.8 * static_cast<double>(sample) / (8.0 * sr));
    }
    static void configure(QQDeBreathAudioProcessor& p, double sr)
    {
        const auto length = static_cast<int>(sr * 8.0);
        p.recordedBuffer.setSize(2, length);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < length; ++i)
                p.recordedBuffer.setSample(ch, i, valueAt(i, sr));
        p.recordedLengthSamples = length;
        p.recordedSampleRate = sr;
        p.recordingStartTimelineSeconds = 10.0;
        p.recordedPreviewReady.store(true);
        QQDeBreathBridgeAnalysisResult result;
        result.hasResult = result.succeeded = true;
        result.sampleRate = static_cast<int>(sr);
        result.channels = 2;
        result.numSamples = length;
        result.durationSeconds = 8.0;
        p.setAnalysisResult(result);
    }
};

namespace
{
class TestPlayHead final : public juce::AudioPlayHead
{
public:
    double seconds = 10.0, sampleRate = 48000.0;
    bool playing = true, reportSamples = true;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo info;
        info.setIsPlaying(playing);
        info.setTimeInSeconds(seconds);
        if (reportSamples)
            info.setTimeInSamples(static_cast<juce::int64>(std::llround(seconds * sampleRate)));
        return info;
    }
};

struct Fixture
{
    QQDeBreathAudioProcessor processor;
    TestPlayHead playHead;
    int failures = 0;
    Fixture(double sr, bool sampleClock)
    {
        playHead.sampleRate = sr;
        playHead.reportSamples = sampleClock;
        processor.setPlayHead(&playHead);
        processor.setRateAndBufferSizeDetails(sr, 512);
        processor.prepareToPlay(sr, 512);
        QQDeBreathTransportSyncProbe::configure(processor, sr);
    }
    ~Fixture() { processor.setPlayHead(nullptr); }
    void check(bool ok, const char* name)
    {
        if (! ok)
        {
            ++failures;
            std::cerr << "FAIL: " << name << " sr=" << playHead.sampleRate
                      << " sampleClock=" << playHead.reportSamples << '\n';
        }
    }
    void render(double host, bool playing, int size, double local, const char* name)
    {
        playHead.seconds = host;
        playHead.playing = playing;
        juce::AudioBuffer<float> buffer(2, size);
        buffer.clear();
        juce::MidiBuffer midi;
        processor.processBlock(buffer, midi);
        double cachedHost = -1.0;
        bool cachedPlaying = false;
        check(processor.getCachedHostPosition(cachedHost, cachedPlaying)
              && std::abs(cachedHost - host) <= 1.1 / playHead.sampleRate
              && cachedPlaying == playing, "audio-thread transport snapshot");
        check(std::abs(processor.getInternalPreviewPosition(host) - juce::jmax(0.0, local)) <= 1.1 / playHead.sampleRate, name);
        if (playing)
        {
            const auto start = static_cast<juce::int64>(std::llround(local * playHead.sampleRate));
            bool ok = true;
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < size; ++i)
                {
                    const auto index = start + i;
                    const auto expected = index < 0 || index >= 8.0 * playHead.sampleRate
                        ? 0.0f : QQDeBreathTransportSyncProbe::valueAt(index, playHead.sampleRate);
                    ok &= std::abs(buffer.getSample(ch, i) - expected) < 2.0e-6f;
                }
            check(ok, name);
        }
    }
};

int run(double sr, bool samples)
{
    Fixture f(sr, samples);
    const auto b = 128.0 / sr;
    f.render(10.0, true, 128, 0.0, "initial alignment with nonzero recording origin");
    f.processor.setInternalPreviewPosition(3.0, 10.0);
    f.render(10.0 + b, true, 256, 3.0 + b, "waveform click survives ordinary playback");
    f.render(10.0 + 3.0 * b, true, 64, 3.0 + 3.0 * b, "variable block sizes retain preview");
    f.render(12.0, true, 128, 2.0, "forward seek clears offset");
    f.processor.setInternalPreviewPosition(5.0, 12.0);
    f.render(11.0, true, 128, 1.0, "backward seek clears offset");
    f.processor.setInternalPreviewPosition(4.0, 11.0);
    f.render(11.0 + b, false, 128, 4.0 + b, "pause preserves selected waveform position");
    f.processor.setInternalPreviewPosition(6.0, 11.0 + b);
    f.render(11.0 + b, false, 128, 6.0, "click while stopped remains selected");
    f.render(11.0 + b, true, 128, 6.0, "start at same stopped position retains selection");
    f.render(11.0 + 2.0 * b, true, 128, 6.0 + b, "selection advances after start");
    f.render(13.0, false, 128, 3.0, "stop and relocate");
    f.processor.setInternalPreviewPosition(6.0, 13.0);
    f.render(14.0, false, 128, 4.0, "stopped relocation clears offset");
    f.render(14.0, true, 128, 4.0, "restart after relocation stays aligned");
    f.processor.setInternalPreviewLoopRange(2.0, 2.01, 14.0);
    f.render(14.0 + b, true, 128, 2.0 + b, "internal loop survives continuous playback");
    f.render(10.5, true, 128, 0.5, "DAW cycle wrap cancels internal loop");
    f.render(15.0, true, 128, 5.0, "cancelled loop cannot reappear");
    f.processor.clearInternalPreviewLoop();
    f.processor.setInternalPreviewPosition(6.0, 15.0);
    f.processor.prepareToPlay(sr, 512);
    f.render(12.0, true, 128, 2.0, "DAW relocation across engine restart clears offset");
    f.render(12.0, false, 128, 2.0, "stop before suspended audio callbacks");
    f.processor.setInternalPreviewLoopRange(5.0, 5.1, 12.0);
    f.processor.syncStoppedPreviewWithHost(12.0);
    f.check(f.processor.isInternalPreviewLoopEnabled(), "stopped UI polling retains same-position preview");
    f.processor.syncStoppedPreviewWithHost(13.0);
    f.check(std::abs(f.processor.getInternalPreviewPosition(13.0) - 3.0) < 1.0e-9,
            "stopped UI relocation works without an audio callback");
    f.check(! f.processor.isInternalPreviewLoopEnabled(), "stopped UI relocation clears loop");
    f.render(13.0, true, 128, 3.0, "resume after suspended host relocation stays aligned");
    f.render(13.0 + b, false, 128, 3.0 + b, "stop before a new suspended-host selection");
    f.processor.syncStoppedPreviewWithHost(14.0);
    f.processor.setInternalPreviewPosition(6.0, 14.0);
    f.render(14.0, true, 128, 6.0, "new preview selection wins after suspended-host seek");
    f.render(9.0, true, 128, -1.0, "DAW seek before recording start renders silence");
    f.render(10.0 - 64.0 / sr, true, 128, -64.0 / sr, "block crossing recording start is sample aligned");
    f.render(18.0, true, 128, 8.0, "DAW seek after recording end renders silence");
    return f.failures;
}

int runPreservation(double sr, bool samples)
{
    const auto b = 128.0 / sr;
    int failures = 0;
    {
        Fixture f(sr, samples);
        f.render(10.0, true, 128, 0.0, "preserve: initial playback");
        f.processor.setInternalPreviewPosition(3.0, 10.0);
        f.render(10.0 + b, true, 128, 3.0 + b, "preserve: waveform click");
        f.render(10.0 + 2.0 * b, false, 128, 3.0 + 2.0 * b, "pause preserves waveform offset");
        f.processor.syncStoppedPreviewWithHost(10.0 + 2.0 * b);
        f.check(std::abs(f.processor.getInternalPreviewPosition(10.0 + 2.0 * b)
                         - (3.0 + 2.0 * b)) < 1.1 / sr, "stopped editor polling preserves waveform offset");
        f.render(10.0 + 2.0 * b, true, 128, 3.0 + 2.0 * b, "resume in place preserves waveform offset");
        f.processor.prepareToPlay(sr, 512);
        f.render(10.0 + 3.0 * b, true, 128, 3.0 + 3.0 * b, "engine prepare alone preserves waveform offset");
        f.render(12.0, true, 128, 2.0, "DAW seek after resume clears waveform offset");
        failures += f.failures;
    }
    {
        Fixture f(sr, samples);
        f.render(10.0, true, 128, 0.0, "suspended: initial playback");
        f.processor.setInternalPreviewLoopRange(3.0, 3.5, 10.0);
        f.render(10.0 + b, true, 128, 3.0 + b, "suspended: loop audition advances");
        // No stopped audio callback: the first UI poll must use the last block,
        // not the original waveform-click timestamp.
        f.processor.syncStoppedPreviewWithHost(10.0 + 2.0 * b);
        f.check(f.processor.isInternalPreviewLoopEnabled(), "suspended pause retains internal loop");
        f.check(std::abs(f.processor.getInternalPreviewPosition(10.0 + 2.0 * b)
                         - (3.0 + 2.0 * b)) < 1.1 / sr, "suspended pause retains cursor offset");
        f.processor.syncStoppedPreviewWithHost(10.0 + 2.0 * b);
        f.check(f.processor.isInternalPreviewLoopEnabled(), "repeated stopped polling retains internal loop");
        f.processor.syncStoppedPreviewWithHost(10.0 + 3.0 * b);
        f.check(!f.processor.isInternalPreviewLoopEnabled(), "one-block stopped DAW seek cancels internal loop");
        f.check(std::abs(f.processor.getInternalPreviewPosition(10.0 + 3.0 * b)
                         - 3.0 * b) < 1.1 / sr, "one-block stopped DAW seek aligns cursor");
        f.processor.setInternalPreviewPosition(6.0, 12.0);
        f.processor.syncStoppedPreviewWithHost(12.0);
        f.check(std::abs(f.processor.getInternalPreviewPosition(12.0) - 6.0) < 1.1 / sr,
                "fresh click at new stopped position wins over stale UI history");
        f.render(12.0, true, 128, 6.0, "fresh stopped selection resumes at clicked sample");
        failures += f.failures;
    }
    {
        Fixture f(sr, samples);
        f.render(10.0, true, 128, 0.0, "block-start stop: initial playback");
        f.processor.setInternalPreviewPosition(3.0, 10.0);
        f.render(10.0 + b, true, 128, 3.0 + b, "block-start stop: audition");
        f.processor.syncStoppedPreviewWithHost(10.0 + b);
        f.render(10.0 + b, true, 128, 3.0 + b, "resume without stopped callback preserves block-start preview");
        f.render(10.0 + 2.0 * b, false, 128, 3.0 + 2.0 * b, "host stop callback preserves preview");
        f.render(10.0 + 2.0 * b, true, 128, 3.0 + 2.0 * b, "resume after stop callback preserves preview");
        failures += f.failures;
    }
    return failures;
}

int runStoppedSelectionStart(double sr, bool samples)
{
    const auto b = 128.0 / sr;
    int failures = 0;
    for (const auto initialCallback : { false, true })
        for (const auto repeatStoppedCallbacks : { false, true })
        {
            Fixture f(sr, samples);
            if (initialCallback)
                f.render(10.0, false, 128, 0.0, "pending: stopped baseline");
            f.processor.setInternalPreviewPosition(6.0, 10.0);
            if (repeatStoppedCallbacks)
            {
                f.render(10.0, false, 128, 6.0, "pending: stopped callbacks retain selection");
                f.processor.syncStoppedPreviewWithHost(10.0);
                f.render(10.0, false, 128, 6.0, "pending: repeated stopped callbacks retain selection");
            }
            f.processor.prepareToPlay(sr, 512);
            f.render(10.5, true, 0, 6.0, "zero-length startup block does not consume pending selection");
            f.render(11.0, true, 128, 6.0, "Play binds selected sample to first actual host block");
            f.render(11.0 + b, true, 128, 6.0 + b, "selected audio advances after Play");
            f.render(12.0, true, 128, 2.0, "later DAW seek returns to original timeline");
            failures += f.failures;
        }
    {
        Fixture f(sr, samples);
        f.render(10.0, false, 128, 0.0, "cancel pending: stopped baseline");
        f.processor.setInternalPreviewPosition(6.0, 10.0);
        f.render(12.0, false, 128, 2.0, "DAW seek while stopped cancels pending selection");
        f.render(12.0, true, 128, 2.0, "Play after stopped DAW seek follows DAW");
        failures += f.failures;
    }
    {
        Fixture f(sr, samples);
        f.render(10.0, false, 128, 0.0, "pending loop: stopped baseline");
        f.processor.setInternalPreviewLoopRange(3.0, 3.5, 10.0);
        f.render(11.0, true, 128, 3.0, "Play starts selected loop at first audio block");
        f.check(f.processor.isInternalPreviewLoopEnabled(), "Play retains pending internal loop");
        f.render(12.0, true, 128, 2.0, "DAW seek cancels loop after Play");
        failures += f.failures;
    }
    return failures;
}

}

#if JUCE_WINDOWS
int wmain()
#else
int main()
#endif
{
    int failures = 0;
    for (const auto sr : { 44100.0, 48000.0, 96000.0 })
        for (const auto samples : { true, false })
            failures += run(sr, samples) + runPreservation(sr, samples) + runStoppedSelectionStart(sr, samples);
    if (failures != 0)
    {
        std::cerr << failures << " transport/audio checks failed\n";
        return 1;
    }
    std::cout << "PASS: stopped waveform selection survives Play startup; subsequent DAW seeks align audio; "
                 "and rendered sample alignment at 44.1/48/96 kHz with sample and seconds clocks\n";
    return 0;
}
