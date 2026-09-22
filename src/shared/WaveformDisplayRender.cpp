#include "WaveformDisplayRender.h"
#include <cmath>
namespace QQDeBreathWaveformDisplay
{
namespace
{

constexpr int blockSize = 4096;
double gain(double db) { return std::pow(10.0, db / 20.0); }
struct Region
{
    juce::int64 start = 0, end = 0, first = 0, last = 0;
    bool before = false, after = false;
    juce::String type;
    double peak = 0, localGain = 1, normGain = 1;
    double weight(juce::int64 sample, int fadeIn, int fadeOut) const
    {
        if (sample >= start && sample < end)
        {
            double w = 1.0;
            if (fadeIn > 0 && !before)
                w = juce::jmin(w, static_cast<double>(sample - start) / juce::jmax(1, fadeIn - 1));
            if (fadeOut > 0 && !after)
                w = juce::jmin(w, static_cast<double>(end - 1 - sample) / juce::jmax(1, fadeOut - 1));
            return juce::jlimit(0.0, 1.0, w);
        }
        if (before && sample >= start - fadeIn && sample < start)
            return juce::jlimit(0.0, 1.0, static_cast<double>(sample - (start - fadeIn)) / juce::jmax(1, fadeIn));
        if (after && sample >= end && sample < end + fadeOut)
            return juce::jlimit(0.0, 1.0, 1.0 - static_cast<double>(sample - end) / juce::jmax(1, fadeOut));
        return 0.0;
    }
};
bool applyEq(juce::AudioBuffer<float>& buffer, double rate, const QQDeBreathEqState& state, const Cancel& cancel)
{
    if (!state.hasActiveProcessing()) return !cancel();
    QQDeBreathEqProcessor eq;
    eq.prepare(rate, buffer.getNumChannels(), state);
    for (int offset = 0; offset < buffer.getNumSamples(); offset += blockSize)
    {
        if (cancel()) return false;
        juce::AudioBuffer<float> view(buffer.getArrayOfWritePointers(), buffer.getNumChannels(), offset,
                                      juce::jmin(blockSize, buffer.getNumSamples() - offset));
        eq.process(view);
    }
    return !cancel();
}
}

bool render(const Request& request, Result& result, const Cancel& cancel)
{
    if (!request.source || cancel()) return false;
    const auto& source = *request.source;
    const auto samples = source.getNumSamples();
    const auto rate = request.sampleRate;
    const auto& settings = request.settings;
    const auto& regions = request.regions;
    result.revision = request.revision;
    result.normalised = settings.normalizeBreath;
    result.sibilanceNormalised = settings.normalizeSibilance;
    result.sibilance.setSize(1, samples); result.sibilance.clear();
    result.sibilanceFixed.setSize(0, 0);
    const bool needsOthers = std::any_of(regions.begin(), regions.end(), [](const auto& region) {
        return qqNormalizedRegionType(region.type) == "Others" && (std::abs(region.gainDb) > 0.001 || region.eqState.hasActiveProcessing());
    });
    result.others.setSize(needsOthers ? 1 : 0, needsOthers ? samples : 0); result.others.clear();
    result.scalable.setSize(1, samples); result.scalable.clear();
    result.fixed.setSize(0, 0);
    if (samples == 0 || rate <= 0) return !cancel();
    const auto fadeIn = settings.enableFade ? static_cast<int>(std::llround(settings.fadeInMs * rate / 1000.0)) : 0;
    const auto fadeOut = settings.enableFade ? static_cast<int>(std::llround(settings.fadeOutMs * rate / 1000.0)) : 0;
    std::vector<Region> cached(static_cast<size_t>(regions.size()));
    for (int i=0; i<regions.size(); ++i)
    {
        const auto& r = regions.getReference(i); auto& c = cached[static_cast<size_t>(i)];
        c.start = juce::jlimit<juce::int64>(0, samples, r.startSample > 0 ? r.startSample : static_cast<juce::int64>(std::llround(r.startTime * rate)));
        c.end = juce::jlimit<juce::int64>(0, samples, r.endSample > 0 ? r.endSample : static_cast<juce::int64>(std::llround(r.endTime * rate)));
        c.type = qqNormalizedRegionType(r.type);
    }
    for (int i=0; i<regions.size(); ++i)
    {
        if (cancel()) return false;
        const auto& original = regions.getReference(i); auto& r = cached[static_cast<size_t>(i)];
        if (r.type == "Noise" || (r.type == "Others" && !needsOthers)) continue;
        const bool normalize = r.type == "Breath" ? settings.normalizeBreath : r.type == "Sibilance" && settings.normalizeSibilance;
        for (int j=0; j<regions.size(); ++j)
        {
            if (i==j) continue;
            if (fadeIn > 0 && std::abs(cached[static_cast<size_t>(j)].end-r.start)<=juce::jmax(1,fadeIn)) r.before=true;
            if (fadeOut > 0 && std::abs(cached[static_cast<size_t>(j)].start-r.end)<=juce::jmax(1,fadeOut)) r.after=true;
        }
        float peak=0;
        if (normalize)
            for (auto offset=r.start; offset<r.end; offset+=blockSize)
            {
                if (cancel()) return false;
                peak=juce::jmax(peak,source.getMagnitude(0,static_cast<int>(offset),static_cast<int>(juce::jmin<juce::int64>(blockSize,r.end-offset))));
            }
        const bool respondsToTarget = normalize && peak > 1.0e-9f;
        const auto norm = respondsToTarget ? 1.0 / static_cast<double>(peak) : 1.0;
        const auto local = gain(juce::jlimit(-30.0,30.0,original.gainDb));
        const auto start=juce::jmax<juce::int64>(0,r.start-fadeIn),end=juce::jmin<juce::int64>(samples,r.end+fadeOut);
        if (end<=start) continue;
        juce::AudioBuffer<float> contribution(1,static_cast<int>(end-start)); contribution.clear();
        for (auto offset=start; offset<end; offset+=blockSize)
        {
            if (cancel()) return false;
            for (auto sample=offset;sample<juce::jmin<juce::int64>(end,offset+blockSize);++sample)
            {
                const auto weight=r.weight(sample,fadeIn,fadeOut);
                if (weight>0) contribution.setSample(0,static_cast<int>(sample-start),static_cast<float>(static_cast<double>(source.getSample(0,static_cast<int>(sample)))*weight*norm*local));
            }
        }
        if (r.type == "Breath" && !applyEq(contribution,rate,settings.globalEq,cancel)) return false;
        if (r.type == "Sibilance" && !applyEq(contribution,rate,settings.sibilanceEq,cancel)) return false;
        if (!applyEq(contribution,rate,original.eqState,cancel)) return false;
        auto* dest = r.type == "Breath" ? &result.scalable : r.type == "Sibilance" ? &result.sibilance : &result.others;
        // Zero-peak cores can have nonzero adjacent fades. They must not follow Norm Target.
        if (normalize && !respondsToTarget)
        {
            auto& fixed = r.type == "Breath" ? result.fixed : result.sibilanceFixed;
            if (fixed.getNumSamples()!=samples) { fixed.setSize(1,samples); fixed.clear(); }
            dest=&fixed;
        }
        dest->addFrom(0,static_cast<int>(start),contribution,0,0,contribution.getNumSamples());
    }
    return !cancel();
}
Worker::Worker() : Thread("QQEasyTool Waveform Preview") {}
Worker::~Worker() { signalThreadShouldExit(); notify(); waitForThreadToExit(-1); }
uint64_t Worker::submit(Request request)
{
    std::lock_guard<std::mutex> lock(mutex);
    request.revision=revision.fetch_add(1,std::memory_order_acq_rel)+1;
    const auto next=request.revision;
    pending=std::make_unique<Request>(std::move(request)); ready.reset(); notify(); return next;
}
void Worker::cancel()
{
    std::lock_guard<std::mutex> lock(mutex);
    revision.fetch_add(1,std::memory_order_acq_rel);pending.reset();ready.reset();notify();
}
std::unique_ptr<Result> Worker::takeResult()
{
    std::lock_guard<std::mutex> lock(mutex);
    return std::move(ready);
}
void Worker::run()
{
    while (!threadShouldExit())
    {
        std::unique_ptr<Request> request;
        { std::lock_guard<std::mutex> lock(mutex);request=std::move(pending); }
        if (!request) { wait(100); continue; }
        const auto cancelled=[&] { return threadShouldExit() || revision.load(std::memory_order_acquire)!=request->revision; };
        try
        {
            auto result=std::make_unique<Result>();
            if (render(*request,*result,cancelled))
            {
                std::lock_guard<std::mutex> lock(mutex);
                if (!cancelled()) ready=std::move(result);
            }
        }
        catch (...) { /* Keep the previous display; audio playback is independent. */ }
    }
}
}
