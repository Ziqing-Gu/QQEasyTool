#pragma once
#include "BridgeAnalysis.h"
#include <functional>
#include <memory>
#include <mutex>
namespace QQDeBreathWaveformDisplay
{
using Cancel = std::function<bool()>;
struct Settings
{
    bool enableFade = true, normalizeBreath = false, normalizeSibilance = false;
    double fadeInMs = 10, fadeOutMs = 10;
    QQDeBreathEqState globalEq, sibilanceEq;
};
struct Request
{
    std::shared_ptr<const juce::AudioBuffer<float>> source;
    double sampleRate = 0;
    juce::Array<QQDeBreathBridgeRegion> regions;
    Settings settings;
    uint64_t revision = 0;
};
struct Result
{
    juce::AudioBuffer<float> scalable, fixed, sibilance, sibilanceFixed, others;
    bool normalised = false, sibilanceNormalised = false;
    uint64_t revision = 0;
};
bool render(const Request&, Result&, const Cancel&);
class Worker final : public juce::Thread
{
public:
    Worker();
    ~Worker() override;
    uint64_t submit(Request);
    void cancel();
    std::unique_ptr<Result> takeResult();
    void run() override;
private:
    std::mutex mutex;
    std::unique_ptr<Request> pending;
    std::unique_ptr<Result> ready;
    std::atomic<uint64_t> revision { 0 };
};
}
