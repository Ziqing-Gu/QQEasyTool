#include "PluginProcessor.h"
#include "LegacyEasyAra105.h"
#include <stdexcept>
#include <thread>
#include <atomic>
#include <ARA_Library/Dispatch/ARAHostDispatch.h>
#include <ARA_Library/Dispatch/ARAPlugInDispatch.h>
#include <algorithm>
#include <cstring>
#include <iostream>
#include <vector>
#include <chrono>

const ARA::ARAFactory* JUCE_CALLTYPE createARAFactory();
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }

struct AudioHost final : ARA::Host::AudioAccessControllerInterface
{
    struct Source { float value = 0.25f; bool fail = false; };
    struct Reader { Source* source; bool wide; };
    std::array<Source, 64> sources;
    const std::thread::id modelThread = std::this_thread::get_id();
    int reads = 0, creates = 0, destroys = 0;
    bool wrongThread = false;
    ARA::ARAAudioReaderHostRef createAudioReaderForSource(ARA::ARAAudioSourceHostRef source, bool wide) noexcept override
    {
        wrongThread = wrongThread || std::this_thread::get_id() != modelThread; ++creates;
        return reinterpret_cast<ARA::ARAAudioReaderHostRef>(new Reader { reinterpret_cast<Source*>(source), wide });
    }
    bool readAudioSamples(ARA::ARAAudioReaderHostRef ref, ARA::ARASamplePosition, ARA::ARASampleCount count, void* const data[]) noexcept override
    {
        wrongThread = wrongThread || std::this_thread::get_id() != modelThread; ++reads;
        const auto& reader = *reinterpret_cast<Reader*>(ref);
        if (reader.source->fail) return false;
        if (reader.wide) std::fill_n(static_cast<double*>(data[0]), static_cast<size_t>(count), reader.source->value);
        else std::fill_n(static_cast<float*>(data[0]), static_cast<size_t>(count), reader.source->value);
        return true;
    }
    void destroyAudioReader(ARA::ARAAudioReaderHostRef ref) noexcept override
    {
        wrongThread = wrongThread || std::this_thread::get_id() != modelThread; ++destroys;
        delete reinterpret_cast<Reader*>(ref);
    }
};

struct ArchiveHost final : ARA::Host::ArchivingControllerInterface
{
    std::vector<ARA::ARAByte> bytes;
    const char* archiveId = nullptr;
    ARA::ARASize getArchiveSize(ARA::ARAArchiveReaderHostRef) noexcept override { return bytes.size(); }
    bool readBytesFromArchive(ARA::ARAArchiveReaderHostRef, ARA::ARASize p, ARA::ARASize n, ARA::ARAByte out[]) noexcept override
    {
        if (p + n > bytes.size()) return false;
        std::memcpy(out, bytes.data() + p, n); return true;
    }
    bool writeBytesToArchive(ARA::ARAArchiveWriterHostRef, ARA::ARASize p, ARA::ARASize n, const ARA::ARAByte in[]) noexcept override
    {
        bytes.resize(std::max(bytes.size(), static_cast<size_t>(p + n)));
        std::memcpy(bytes.data() + p, in, n); return true;
    }
    void notifyDocumentArchivingProgress(float) noexcept override {}
    void notifyDocumentUnarchivingProgress(float) noexcept override {}
    ARA::ARAPersistentID getDocumentArchiveID(ARA::ARAArchiveReaderHostRef) noexcept override { return archiveId; }
};

juce::String fingerprint(int i)
{
    auto text = "render-source-" + juce::String(i) + "|sr=48000.00000000|ch=1|samples=48000";
    std::uint64_t hash = 14695981039346656037ull;
    for (auto* p = reinterpret_cast<const unsigned char*>(text.toRawUTF8()); *p; ++p) { hash ^= *p; hash *= 1099511628211ull; }
    return juce::String::toHexString(static_cast<juce::int64>(hash)).paddedLeft('0', 16);
}

void renderProbe(ARA::PlugIn::DocumentController* dc, QQDeBreathARADocumentController* owner, ARA::ARADocumentControllerRef ref, AudioHost& audioHost, ArchiveHost& archiveHost)
{
    std::vector<ARA::ARAAudioSourceRef> sources;
    std::vector<ARA::ARAAudioModificationRef> mods;
    std::vector<ARA::ARAPlaybackRegionRef> regions;
    std::vector<std::unique_ptr<ARA::PlugIn::PlugInExtension>> extensions;
    auto audio = std::make_shared<juce::AudioBuffer<float>>(1, 48000);
    juce::FloatVectorOperations::fill(audio->getWritePointer(0), 0.25f, 48000);
    dc->beginEditing();
    ARA::ARAMusicalContextProperties cp {}; cp.structSize = sizeof(cp); cp.name = "test context";
    auto context = dc->createMusicalContext(reinterpret_cast<ARA::ARAMusicalContextHostRef>(&sources), &cp);
    ARA::ARARegionSequenceProperties sp {}; sp.structSize = sizeof(sp); sp.name = "test sequence"; sp.musicalContextRef = context;
    auto sequence = dc->createRegionSequence(reinterpret_cast<ARA::ARARegionSequenceHostRef>(&mods), &sp);
    for (int i=0; i<44; ++i)
    {
        auto id = "render-source-" + juce::String(i);
        ARA::ARAAudioSourceProperties ap {}; ap.structSize = sizeof(ap); ap.name = id.toRawUTF8(); ap.persistentID = id.toRawUTF8(); ap.sampleCount = 48000; ap.sampleRate = 48000; ap.channelCount = 1;
        sources.push_back(dc->createAudioSource(reinterpret_cast<ARA::ARAAudioSourceHostRef>(&audioHost.sources[static_cast<size_t>(i)]), &ap));
        ARA::ARAAudioModificationProperties mp {}; mp.structSize = sizeof(mp); mp.name = id.toRawUTF8(); mp.persistentID = id.toRawUTF8();
        mods.push_back(dc->createAudioModification(sources.back(), reinterpret_cast<ARA::ARAAudioModificationHostRef>(static_cast<uintptr_t>(i+1)), &mp));
        ARA::ARAPlaybackRegionProperties rp {}; rp.structSize = sizeof(rp); rp.durationInModificationTime = 1.0; rp.durationInPlaybackTime = 1.0; rp.startInPlaybackTime = static_cast<double>(i%2); rp.regionSequenceRef = sequence;
        regions.push_back(dc->createPlaybackRegion(mods.back(), reinterpret_cast<ARA::ARAPlaybackRegionHostRef>(static_cast<uintptr_t>(i+1)), &rp));
        owner->setSourceAudioCache(fingerprint(i), 48000, audio);
    }
    dc->endEditing();
    for (int i=0; i<22; ++i)
    {
        auto ext = std::make_unique<ARA::PlugIn::PlugInExtension>();
        ext->bindToARA(ref, ARA::kARAPlaybackRendererRole, ARA::kARAPlaybackRendererRole);
        auto* renderer = ext->getPlaybackRenderer<juce::ARAPlaybackRenderer>();
        renderer->addPlaybackRegion(regions[2*i]); renderer->addPlaybackRegion(regions[2*i+1]);
        renderer->prepareToPlay(48000, 128, 1, juce::AudioProcessor::singlePrecision);
        extensions.push_back(std::move(ext));
    }
    juce::AudioBuffer<float> buffer(1, 128);
    juce::AudioPlayHead::PositionInfo pos; pos.setIsPlaying(true);
    int silent = 0; int audible = 0;
    for (int i=0; i<44; ++i)
    {
        pos.setTimeInSamples((i%2)*48000);
        auto* renderer = extensions[i/2]->getPlaybackRenderer<juce::ARAPlaybackRenderer>();
        renderer->processBlock(buffer, juce::AudioProcessor::Realtime::no, pos);
        if (buffer.getMagnitude(0,128) == 0) ++silent; else ++audible;
    }
    require(silent == 12 && audible == 32, "baseline did not reproduce 12 silent sources");
    std::cout << "REPRODUCED 1.05: 22 renderers / 44 sources: audible=" << audible << " silent=" << silent << "\n";
    for (auto& ext : extensions) {
        auto* renderer=ext->getPlaybackRenderer<juce::ARAPlaybackRenderer>(); renderer->releaseResources();
        auto assigned=renderer->getPlaybackRegions(); for(auto* region:assigned) renderer->removePlaybackRegion(ARA::PlugIn::toRef(region));
    }
    extensions.clear(); dc->beginEditing();
    for(auto region:regions) dc->destroyPlaybackRegion(region);
    for(auto mod:mods) dc->destroyAudioModification(mod);
    for(auto source:sources) dc->destroyAudioSource(source);
    dc->destroyRegionSequence(sequence); dc->destroyMusicalContext(context); dc->endEditing();
}
int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    auto* factory = createARAFactory();
    const ARA::SizedStruct<ARA_STRUCT_MEMBER(ARAInterfaceConfiguration, assertFunctionAddress)> config { ARA::kARAAPIGeneration_2_0_Final, nullptr };
    factory->initializeARAWithConfiguration(&config);
    AudioHost audioHost;
    ArchiveHost archiveHost;
    archiveHost.archiveId = factory->documentArchiveID;
    ARA::Host::DocumentControllerHostInstance host(&audioHost, &archiveHost);
    const ARA::SizedStruct<ARA_STRUCT_MEMBER(ARADocumentProperties, name)> props { "QQEasyTool cache diagnosis" };
    auto* instance = factory->createDocumentControllerWithDocument(&host, &props);
    auto* dc = static_cast<ARA::PlugIn::DocumentController*>(ARA::PlugIn::fromRef(instance->documentControllerRef));
    auto* owner = juce::ARADocumentControllerSpecialisation::getSpecialisedDocumentController<QQDeBreathARADocumentController>(dc);
    auto sample = std::make_shared<juce::AudioBuffer<float>>(1, 128);
    juce::FloatVectorOperations::fill(sample->getWritePointer(0), 0.25f, 128);
    auto key = [](int i) { return "probe-source-" + juce::String(i); };
    auto load = [&](int count) { for (int i=0; i<count; ++i) owner->setSourceAudioCache(key(i), 48000.0, sample); };
    auto countPresent = [&](int count)
    {
        int total = 0;
        for (int i=0; i<count; ++i)
        {
            QQDeBreathARASourceAudioBuffer buffer;
            double sr = 0; bool found = false;
            if (owner->tryGetSourceAudioCache(key(i), buffer, sr, found) && found) ++total;
        }
        return total;
    };
    load(22);
    std::cout << "22 unique sources present=" << countPresent(22) << "/22\n";
    load(44);
    require(countPresent(44) == 32, "baseline cache cap did not reproduce");
    std::cout << "44 unique sources present=" << countPresent(44) << "/44; earliest 12 present=" << countPresent(12) << "/12\n";
    for (int pass=0; pass<3; ++pass)
    {
        load(44);
        std::cout << "warmup-like reload pass=" << pass+1 << " present=" << countPresent(44) << "/44; earliest 12 present=" << countPresent(12) << "/12\n";
    }
    // Real ARA archive API roundtrip. Empty edit state intentionally isolates raw-audio cache lifetime.
    load(22);
    const auto before = countPresent(22);
    auto writerRef = reinterpret_cast<ARA::ARAArchiveWriterHostRef>(&archiveHost);
    auto readerRef = reinterpret_cast<ARA::ARAArchiveReaderHostRef>(&archiveHost);
    const bool stored = dc->storeObjectsToArchive(writerRef, nullptr);
    dc->beginEditing();
    const bool restored = dc->restoreObjectsFromArchive(readerRef, nullptr);
    dc->endEditing();
    std::cout << "archive store=" << stored << " restore=" << restored << "; cached sources before=" << before << " after=" << countPresent(22) << "/22\n";
    require(before == 22 && countPresent(22) == 0, "baseline archive clear did not reproduce");
    renderProbe(dc, owner, instance->documentControllerRef, audioHost, archiveHost);
    dc->destroyDocumentController();
    factory->uninitializeARA();
    require(stored && restored, "archive failed");
    std::cout << "PASS: 1.05 fault reproduction\n";
    return 0;
}
