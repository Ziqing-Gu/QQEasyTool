#pragma once
#include "NativeAnalysis.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <map>
#include <memory>
#include <limits>
#include <vector>
#include <atomic>

// Owned by an ARA document, never by an editor window. Workers own immutable
// audio/config snapshots only; all host/model updates stay on the model thread.
template<class SourceInfo>
class QQAnalysisJobs
{
    friend class QQEasyToolAraSelectionProbe;
public:
    struct Completion
    {
        SourceInfo source;
        QQDeBreathBridgeAnalysisResult result;
        juce::Array<double> peaks;
    };

    ~QQAnalysisJobs() { cancelAll(); }

    bool start(const SourceInfo& source, const QQDeBreathBridgeAnalysisConfig& config,
               std::shared_ptr<const juce::AudioBuffer<float>> audio, double sampleRate)
    {
        if (source.sourceFingerprint.isEmpty() || busy(source.sourceFingerprint)) return false;
        auto job = std::make_shared<Job>();
        job->completion.source = source;
        jobs.emplace(source.sourceFingerprint, job);
        pool->threads.addJob([job, config, audio = std::move(audio), sampleRate]() mutable
        {
            const auto cancelled = [&] { return job->cancelled.load(std::memory_order_acquire); };
            job->started.store(true, std::memory_order_release);
            try
            {
                if (!cancelled() && audio == nullptr)
                {
                    juce::AudioFormatManager formats; formats.registerBasicFormats();
                    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(config.inputWav));
                    if (reader != nullptr && reader->lengthInSamples > 0 && reader->lengthInSamples <= std::numeric_limits<int>::max())
                    {
                        auto loaded = std::make_shared<juce::AudioBuffer<float>>(static_cast<int>(reader->numChannels), static_cast<int>(reader->lengthInSamples));
                        if (reader->read(loaded.get(), 0, loaded->getNumSamples(), 0, true, true))
                        { audio = std::move(loaded); sampleRate = reader->sampleRate; }
                    }
                }
                job->completion.result = audio != nullptr
                    ? QQDeBreathNativeAnalysis::runWithAudio(config, *audio, sampleRate, cancelled)
                    : QQDeBreathNativeAnalysis::run(config, cancelled);
                if (audio != nullptr && !cancelled())
                    for (const auto& region : job->completion.result.regions)
                    {
                        const auto start = static_cast<int>(juce::jlimit<juce::int64>(0, audio->getNumSamples(), region.startSample));
                        const auto end = static_cast<int>(juce::jlimit<juce::int64>(start, audio->getNumSamples(), region.endSample));
                        job->completion.peaks.add(end > start ? static_cast<double>(audio->getMagnitude(start, end - start)) : 0.0);
                    }
            }
            catch (const std::exception& e)
            { job->completion.result.errorMessage = e.what(); job->completion.result.status = "Analysis failed."; }
            catch (...)
            { job->completion.result.errorMessage = "Analysis worker failed."; job->completion.result.status = "Analysis failed."; }
            job->done.store(true, std::memory_order_release);
        });
        return true;
    }

    bool busy(const juce::String& key) const { return jobs.find(key) != jobs.end(); }
    bool queued(const juce::String& key) const
    { const auto i = jobs.find(key); return i != jobs.end() && !i->second->started.load(std::memory_order_acquire); }
    juce::uint32 startedAt(const juce::String& key) const
    { const auto i = jobs.find(key); return i != jobs.end() ? i->second->submittedAt : 0; }
    bool empty() const { return jobs.empty(); }

    void cancelForSource(const juce::String& key)
    {
        for (auto i = jobs.begin(); i != jobs.end();)
        {
            bool matches = i->first == key;
            for (const auto& mapping : i->second->completion.source.playbackMappings)
                matches = matches || mapping.sourceFingerprint == key;
            if (matches) { i->second->cancelled.store(true, std::memory_order_release); i = jobs.erase(i); }
            else ++i;
        }
    }
    void cancelAll()
    { for (auto& item : jobs) item.second->cancelled.store(true, std::memory_order_release); jobs.clear(); }

    std::vector<Completion> takeCompleted()
    {
        std::vector<Completion> out;
        for (auto i = jobs.begin(); i != jobs.end();)
            if (i->second->done.load(std::memory_order_acquire))
            {
                if (!i->second->cancelled.load(std::memory_order_acquire)) out.push_back(std::move(i->second->completion));
                i = jobs.erase(i);
            }
            else ++i;
        return out;
    }

private:
    struct Job
    {
        Completion completion;
        std::atomic<bool> cancelled{false}, done{false}, started{false};
        juce::uint32 submittedAt = juce::Time::getMillisecondCounter();
    };
    struct Pool
    {
        // Shared by all instances of this plug-in; 22 tracks do not spawn 22 workers.
        ~Pool() { threads.removeAllJobs(true, -1); }
        juce::ThreadPool threads{2, juce::Thread::osDefaultStackSize, juce::Thread::Priority::low};
    };
    juce::SharedResourcePointer<Pool> pool;
    std::map<juce::String, std::shared_ptr<Job>> jobs;
};
