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
    require(silent == 0 && audible == 44, "44-source offline render lost audio");
    std::cout << "REAL ARA 22 renderers / 44 sources: offline audible=" << audible << " silent=" << silent << "/44 (expected input amplitude=0.25)\n";
    // Offline processing must not depend on the host exposing a playing flag.
    pos.setIsPlaying(false); pos.setTimeInSamples(0);
    extensions[0]->getPlaybackRenderer<juce::ARAPlaybackRenderer>()->processBlock(buffer, juce::AudioProcessor::Realtime::no, pos);
    require(buffer.getMagnitude(0,128) == 0.25f, "stopped-flag offline render muted audio");
    extensions[0]->getPlaybackRenderer<juce::ARAPlaybackRenderer>()->processBlock(buffer, juce::AudioProcessor::Realtime::yes, pos);
    require(buffer.getMagnitude(0,128) == 0.0f, "stopped realtime render emitted audio");
    pos.setIsPlaying(true);
    // Independent ARA instances can render concurrently without host reads or shared mutable DSP.
    std::atomic<int> parallelFailures { 0 };
    std::vector<std::thread> workers;
    const auto readsBeforeRendering = audioHost.reads;
    for (int i=0; i<22; ++i) workers.emplace_back([&, i]
    {
        juce::AudioBuffer<float> out(1,128);
        juce::AudioPlayHead::PositionInfo at; at.setIsPlaying(true);
        for (int j=0; j<32; ++j)
        {
            at.setTimeInSamples((j%2)*48000);
            extensions[static_cast<size_t>(i)]->getPlaybackRenderer<juce::ARAPlaybackRenderer>()->processBlock(out, juce::AudioProcessor::Realtime::yes, at);
            for (int k=0; k<128; ++k) if (out.getSample(0,k) != 0.25f) ++parallelFailures;
        }
    });
    for (auto& worker : workers) worker.join();
    require(parallelFailures == 0 && audioHost.reads == readsBeforeRendering, "parallel render mismatch or host read");
    std::cout << "PASS: 22 concurrent renderers, zero host reads, offline/stopped flags\n";
    // One source per renderer fits below the cap: isolate DSP cost from cache misses.
    for (int i=0; i<22; ++i)
    {
        const int source = i*2;
        owner->setSourceAudioCache(fingerprint(source), 48000, audio);
        QQDeBreathARASourceInfo info; info.sourceFingerprint = fingerprint(source); info.persistentId = "render-source-" + juce::String(source); info.sampleRate = 48000; info.channelCount = 1; info.numSamples = 48000;
        QQDeBreathBridgeAnalysisResult result; result.hasResult = result.succeeded = true; result.sampleRate = 48000; result.channels = 1; result.numSamples = 48000;
        for (int j=0; j<64; ++j) { QQDeBreathBridgeRegion region; region.type = "Breath"; region.startSample = j*600; region.endSample = j*600+200; region.startTime = region.startSample/48000.0; region.endTime = region.endSample/48000.0; result.regions.add(region); }
        QQDeBreathARAPlaybackParams params;
        owner->updateRuntimePersistentState(info, result, params);
    }
    pos.setTimeInSamples(0);
    for (auto& ext : extensions) ext->getPlaybackRenderer<juce::ARAPlaybackRenderer>()->processBlock(buffer, juce::AudioProcessor::Realtime::yes, pos);
    std::vector<double> timings;
    for (int pass=0; pass<5; ++pass)
    {
        const auto start = std::chrono::steady_clock::now();
        for (auto& ext : extensions) ext->getPlaybackRenderer<juce::ARAPlaybackRenderer>()->processBlock(buffer, juce::AudioProcessor::Realtime::yes, pos);
        timings.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());
    }
    std::sort(timings.begin(),timings.end());
    std::cout << "REAL ARA 22 renderers sequential / 64 regions each / fade enabled / 128 samples @48kHz: median=" << timings[2] << " ms; block duration=" << 128000.0/48000 << " ms (synthetic, no DAW parallel scheduling)\n";
    // Compare the full ARA audio output with the frozen 1.05 renderer, including state revisions.
    auto* current = extensions[0]->getPlaybackRenderer<juce::ARAPlaybackRenderer>();
    LegacyEasyAra105::QQDeBreathARAPlaybackRenderer legacy(dc, *owner);
    legacy.addPlaybackRegion(regions[0]);
    double worst = 0.0;
    for (double rate : {44100.0, 48000.0, 96000.0}) for (int mode=0; mode<32; ++mode)
    {
        QQDeBreathARAPersistentState state;
        require(owner->getPersistentStateForSource(fingerprint(0), state), "missing reference state");
        auto& result = state.analysisResult;
        result.regions.clear(); state.regionPeakCache.clear();
        for (int j=0; j<12; ++j)
        {
            QQDeBreathBridgeRegion r; const char* types[] = { "Breath", "Noise", "Others", "Sibilance", "Noize", "Sibilant" }; r.type = types[j%6];
            r.startSample = j*240; r.endSample = r.startSample + 180; r.gainDb = j%5 - 3;
            if (j==2) { r.startSample=240; r.endSample=500; }
            if (j==3) { r.startSample=240; r.endSample=500; }
            if (j==4) { r.startSample=500; r.endSample=500; }
            if (j==5) { r.startSample=500; r.endSample=490; }
            if (j==6) { r.startSample=0; r.endSample=0; r.startTime=0.028; r.endTime=0.032; }
            if (mode>=8 && j%4==1) { r.eqState.enabled=true; r.eqState.highPassEnabled=true; r.eqState.bands[0].enabled=true; r.eqState.bands[0].gainDb=-4; }
            result.regions.add(r); state.regionPeakCache.add(j%4==0 ? 0.0 : 0.25);
        }
        auto& params = state.playbackParams;
        params.enableFade = mode%4 != 0; params.fadeInMs=mode%3==0 ? 0.0 : 1.3; params.fadeOutMs=mode%3==1 ? 0.0 : 2.1;
        params.normalizeBreath=mode%2!=0; params.breathTargetDb=-15; params.breathGainDb=-7;
        params.monitorVoice=(mode&1)!=0; params.monitorBreath=(mode&2)!=0; params.monitorNoize=(mode&4)!=0; params.monitorOthers=(mode&8)!=0; params.monitorSibilance=(mode&16)!=0; params.normalizeSibilance=mode%2==0; params.sibilanceTargetDb=-18; params.sibilanceGainDb=-5;
        params.breathEqState.enabled=mode>=8; params.breathEqState.highPassEnabled=true; params.breathEqState.bands[0].enabled=true; params.breathEqState.bands[0].gainDb=3;
        owner->updateRuntimePersistentState(state.sourceInfo, result, params, state.regionPeakCache);
        current->releaseResources(); legacy.releaseResources();
        current->prepareToPlay(rate,256,2,juce::AudioProcessor::singlePrecision);
        legacy.prepareToPlay(rate,256,2,juce::AudioProcessor::singlePrecision);
        juce::AudioBuffer<float> oldBlock(2,256), newBlock(2,256);
        for (auto start : {-64, 0, 128, 240, 490, 600, 1000, 1370, 2200, 4000})
        {
            pos.setTimeInSamples(static_cast<juce::int64>(std::llround(start*rate/48000.0)));
            legacy.processBlock(oldBlock,juce::AudioProcessor::Realtime::yes,pos);
            current->processBlock(newBlock,juce::AudioProcessor::Realtime::yes,pos);
            for (int ch=0;ch<2;++ch) for (int n=0;n<256;++n)
                worst=std::max(worst,static_cast<double>(std::abs(oldBlock.getSample(ch,n)-newBlock.getSample(ch,n))));
        }
    }
    require(worst <= 1.0e-7, "1.05 ARA DSP sample mismatch");
    legacy.releaseResources(); legacy.removePlaybackRegion(regions[0]);
    std::cout << "PASS: 96 legacy EasyTool ARA DSP cases / 10 blocks each / 44.1,48,96kHz / overlap, ties, fades, norm, monitors, EQ; max error=" << worst << "\n";
    // Partial archive restore and ID remapping must leave unrelated source edits and global settings intact.
    auto stateFor = [&](int i) { QQDeBreathARAPersistentState state; require(owner->getPersistentStateForSource(fingerprint(i), state), "missing archive state"); return state; };
    auto setGain = [&](int i, double gain) { auto st=stateFor(i); st.playbackParams.breathGainDb=gain; owner->updateRuntimePersistentState(st.sourceInfo,st.analysisResult,st.playbackParams,st.regionPeakCache); };
    setGain(0,-9); setGain(2,-3);
    auto global=owner->getPlaybackParams(); global.breathGainDb=6; owner->setPlaybackParams(global);
    ARA::ARAStoreObjectsFilter storeFilter {}; storeFilter.structSize=sizeof(storeFilter); storeFilter.audioSourceRefsCount=1; storeFilter.audioSourceRefs=&sources[0];
    auto writerRef=reinterpret_cast<ARA::ARAArchiveWriterHostRef>(&archiveHost);
    auto readerRef=reinterpret_cast<ARA::ARAArchiveReaderHostRef>(&archiveHost);
    archiveHost.bytes.clear(); require(dc->storeObjectsToArchive(writerRef,&storeFilter), "partial store");
    const auto saved=archiveHost.bytes;
    setGain(0,-20); setGain(2,-12); global.breathGainDb=3; owner->setPlaybackParams(global);
    const ARA::ARAPersistentID archiveId="render-source-0";
    ARA::ARARestoreObjectsFilter restoreFilter {}; restoreFilter.structSize=sizeof(restoreFilter); restoreFilter.audioSourceIDsCount=1; restoreFilter.audioSourceArchiveIDs=&archiveId;
    dc->beginEditing(); require(dc->restoreObjectsFromArchive(readerRef,&restoreFilter), "partial restore"); dc->endEditing();
    require(stateFor(0).playbackParams.breathGainDb==-9 && stateFor(2).playbackParams.breathGainDb==-12 && owner->getPlaybackParams().breathGainDb==3, "partial restore changed unrelated state");
    const ARA::ARAPersistentID currentId="render-source-4"; restoreFilter.audioSourceCurrentIDs=&currentId;
    setGain(0,-17);
    dc->beginEditing(); require(dc->restoreObjectsFromArchive(readerRef,&restoreFilter), "mapped restore"); dc->endEditing();
    require(stateFor(4).playbackParams.breathGainDb==-9 && stateFor(0).playbackParams.breathGainDb==-17, "archive ID remapping failed");
    archiveHost.bytes.resize(archiveHost.bytes.size()/2);
    dc->beginEditing(); require(!dc->restoreObjectsFromArchive(readerRef,&restoreFilter), "truncated archive accepted"); dc->endEditing();
    require(stateFor(4).playbackParams.breathGainDb==-9, "failed restore mutated state");
    archiveHost.bytes=saved;
    std::cout << "PASS: partial archive isolation, source ID remapping, malformed archive preservation\n";
    // Host archive splicing: document data may be stored alone and restored last.
    auto composite=stateFor(0);
    composite.sourceInfo.isComposite=true; composite.sourceInfo.sourceFingerprint="composite-probe";
    composite.sourceInfo.playbackMappings.clear();
    for (int source : {0,2})
    {
        QQDeBreathARASourceInfo::PlaybackMapping mapping;
        mapping.persistentId="render-source-"+juce::String(source); mapping.sourceFingerprint=fingerprint(source);
        mapping.sourceSampleRate=48000; mapping.sourceChannelCount=1; mapping.sourceNumSamples=48000;
        composite.sourceInfo.playbackMappings.add(mapping);
    }
    composite.playbackParams.breathGainDb=-8;
    owner->upsertPersistentState(composite.sourceInfo,composite.analysisResult,composite.playbackParams,composite.regionPeakCache);
    ARA::ARAStoreObjectsFilter docStore {}; docStore.structSize=sizeof(docStore); docStore.documentData=ARA::kARATrue;
    archiveHost.bytes.clear(); require(dc->storeObjectsToArchive(writerRef,&docStore),"document-only store");
    composite.playbackParams.breathGainDb=-2;
    owner->upsertPersistentState(composite.sourceInfo,composite.analysisResult,composite.playbackParams,composite.regionPeakCache);
    ARA::ARARestoreObjectsFilter docRestore {}; docRestore.structSize=sizeof(docRestore); docRestore.documentData=ARA::kARATrue;
    dc->beginEditing(); require(dc->restoreObjectsFromArchive(readerRef,&docRestore),"document-only restore"); dc->endEditing();
    QQDeBreathARAPersistentState compositeRestored;
    require(owner->getPersistentStateForSource("composite-probe",compositeRestored) && compositeRestored.playbackParams.breathGainDb==-8,"document-only composite snapshot lost");
    require(stateFor(2).playbackParams.breathGainDb==-12,"document-only restore replaced source edits");
    // A not-yet-analyzed source copied over an edited destination must remove the destination analysis.
    storeFilter.audioSourceRefs=&sources[1]; archiveHost.bytes.clear();
    require(dc->storeObjectsToArchive(writerRef,&storeFilter),"untouched source store");
    const ARA::ARAPersistentID untouchedId="render-source-1"; restoreFilter.audioSourceArchiveIDs=&untouchedId;
    restoreFilter.audioSourceCurrentIDs=&currentId;
    dc->beginEditing(); require(dc->restoreObjectsFromArchive(readerRef,&restoreFilter),"untouched mapped restore"); dc->endEditing();
    require(!stateFor(4).analysisResult.succeeded,"untouched remapped source retained old analysis");
    std::cout << "PASS: document-only archive splicing and untouched-source mapped restore\n";
    // Host lifecycle callbacks prepare audio before first render without opening an editor.
    const auto createdBefore=audioHost.creates;
    dc->beginEditing();
    ARA::ARAAudioSourceProperties coldProps {}; coldProps.structSize=sizeof(coldProps); coldProps.name="cold source";
    coldProps.persistentID="render-source-44"; coldProps.sampleCount=48000; coldProps.sampleRate=48000; coldProps.channelCount=1;
    auto cold=dc->createAudioSource(reinterpret_cast<ARA::ARAAudioSourceHostRef>(&audioHost.sources[44]),&coldProps);
    dc->endEditing();
    auto* coldSource=static_cast<juce::ARAAudioSource*>(ARA::PlugIn::fromRef(cold));
    auto cachedCold = [&]()
    {
        QQDeBreathARASourceAudioBuffer cached; double sr=0; bool found=false;
        require(owner->tryGetSourceAudioCache(LegacyEasyAra105::buildAraSourceFingerprint(*coldSource),cached,sr,found),"cache lock unexpectedly busy");
        return found ? cached : QQDeBreathARASourceAudioBuffer {};
    };
    require(cachedCold()==nullptr,"disabled cold source unexpectedly read");
    dc->enableAudioSourceSamplesAccess(cold,true);
    require(cachedCold()!=nullptr && cachedCold()->getSample(0,0)==0.25f,"cold source not ready when sample access returns");
    require(audioHost.creates==createdBefore+1,"cold source should load once");
    owner->requestSourceAudioCacheWarmup();
    require(audioHost.creates==createdBefore+1,"warmup reread valid cache");
    audioHost.sources[44].value=-0.125f;
    dc->beginEditing(); dc->updateAudioSourceContent(cold,nullptr,ARA::ContentUpdateScopes::samplesAreAffected()); dc->endEditing();
    require(cachedCold()!=nullptr && cachedCold()->getSample(0,0)==-0.125f,"sample content update kept stale audio");
    audioHost.sources[44].fail=true;
    dc->beginEditing(); dc->updateAudioSourceContent(cold,nullptr,ARA::ContentUpdateScopes::samplesAreAffected()); dc->endEditing();
    require(cachedCold()==nullptr,"failed host read published partial or stale audio");
    audioHost.sources[44].fail=false;
    dc->beginEditing(); dc->endEditing();
    require(cachedCold()!=nullptr,"failed read was not retried after host editing");
    dc->enableAudioSourceSamplesAccess(cold,false);
    dc->beginEditing(); dc->deactivateAudioSourceForUndoHistory(cold,true); dc->endEditing();
    require(cachedCold()==nullptr,"inactive undo source retained large audio");
    dc->beginEditing(); dc->deactivateAudioSourceForUndoHistory(cold,false); dc->endEditing();
    dc->enableAudioSourceSamplesAccess(cold,true);
    require(cachedCold()!=nullptr && cachedCold()->getSample(0,0)==-0.125f,"undo reactivation did not preload source");
    dc->enableAudioSourceSamplesAccess(cold,false);
    coldProps.sampleCount=24000; coldProps.sampleRate=24000;
    dc->beginEditing(); dc->updateAudioSourceProperties(cold,&coldProps); dc->endEditing();
    dc->enableAudioSourceSamplesAccess(cold,true);
    require(cachedCold()!=nullptr && cachedCold()->getNumSamples()==24000,"source geometry change kept old cache");
    const auto finalColdKey=LegacyEasyAra105::buildAraSourceFingerprint(*coldSource);
    dc->enableAudioSourceSamplesAccess(cold,false);
    dc->beginEditing(); dc->destroyAudioSource(cold); dc->endEditing();
    QQDeBreathARASourceAudioBuffer removed; double removedRate=0; bool removedFound=false;
    require(owner->tryGetSourceAudioCache(finalColdKey,removed,removedRate,removedFound) && !removedFound,"destroyed cold source cache survived");
    require(!audioHost.wrongThread && audioHost.creates==audioHost.destroys,"ARA reader lifecycle/thread violation");
    std::cout << "PASS: editor-free cold load, cache reuse, sample changes, failed-read retry, undo, source geometry, model-thread reader lifecycle\n";
    for (auto& ext : extensions)
    {
        auto* renderer = ext->getPlaybackRenderer<juce::ARAPlaybackRenderer>(); renderer->releaseResources();
        auto assigned = renderer->getPlaybackRegions();
        for (auto* region : assigned) renderer->removePlaybackRegion(ARA::PlugIn::toRef(region));
    }
    extensions.clear();
    dc->beginEditing();
    for (auto r : regions) dc->destroyPlaybackRegion(r);
    for (auto m : mods) dc->destroyAudioModification(m);
    for (auto s : sources) dc->destroyAudioSource(s);
    for (int i=0; i<44; ++i)
    {
        QQDeBreathARASourceAudioBuffer cached; double sr=0; bool found=false;
        require(owner->tryGetSourceAudioCache(fingerprint(i),cached,sr,found) && !found, "destroyed source retained audio cache");
    }
    dc->destroyRegionSequence(sequence); dc->destroyMusicalContext(context);
    dc->endEditing();
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
    require(countPresent(44) == 44, "cache evicted a live source");
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
    require(before == 22 && countPresent(22) == 22, "archive restore discarded valid audio");
    renderProbe(dc, owner, instance->documentControllerRef, audioHost, archiveHost);
    dc->destroyDocumentController();
    factory->uninitializeARA();
    require(stored && restored, "archive failed");
    std::cout << "PASS: ARA multi-instance regression\n";
    return 0;
}
