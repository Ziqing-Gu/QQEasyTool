#include "PluginProcessor.h"
#include "PluginEditor.h"
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
#if JUCE_WINDOWS
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

// The standalone host must dispatch messages just as the DAW does. In Windows,
// JUCE workers attach their input queues to the message thread during startup.
void pumpHostMessages()
{
#if JUCE_WINDOWS
    MSG message;
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
    { TranslateMessage(&message); DispatchMessageW(&message); }
#endif
}

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


struct UpdateHost final : ARA::Host::ModelUpdateControllerInterface
{
 int sources=0, modifications=0, regions=0, documents=0;
 void notifyAudioSourceAnalysisProgress(ARA::ARAAudioSourceHostRef,ARA::ARAAnalysisProgressState,float) noexcept override {}
 void notifyAudioSourceContentChanged(ARA::ARAAudioSourceHostRef,const ARA::ARAContentTimeRange*,ARA::ContentUpdateScopes) noexcept override {++sources;}
 void notifyAudioModificationContentChanged(ARA::ARAAudioModificationHostRef,const ARA::ARAContentTimeRange*,ARA::ContentUpdateScopes) noexcept override {++modifications;}
 void notifyPlaybackRegionContentChanged(ARA::ARAPlaybackRegionHostRef,const ARA::ARAContentTimeRange*,ARA::ContentUpdateScopes) noexcept override {++regions;}
 void notifyDocumentDataChanged() noexcept override {++documents;}
 int count() const {return sources+modifications+regions+documents;}
};

struct NativeUpdateListener final : juce::AudioProcessorListener
{
    int dirty = 0;
    void audioProcessorParameterChanged(juce::AudioProcessor*, int, float) override {}
    void audioProcessorChanged(juce::AudioProcessor*, const ChangeDetails& details) override { if (details.nonParameterStateChanged) ++dirty; }
};

class QQEasyToolAraSelectionProbe
{
public:
 static void ordinary()
 {
    QQDeBreathAudioProcessor processor;
    NativeUpdateListener listener; processor.addListener(&listener);
    processor.recordedBuffer.setSize(1,48000); processor.recordedBuffer.clear();
    processor.recordedLengthSamples=48000; processor.recordedSampleRate=48000;
    processor.claimInitialGlobalDefaultsApplication();
    auto editor=std::make_unique<QQDeBreathAudioProcessorEditor>(processor);
    editor->timerCallback();
    QQDeBreathBridgeRegion r; r.type="Breath"; r.startTime=0.1; r.endTime=0.2; r.startSample=4800; r.endSample=9600; r.gainDb=-7;
    editor->waveformRegionsChanged({r});
    require(listener.dirty>0,"native custom edit did not notify host");
    const auto dirtyBeforeIdle=listener.dirty; editor->timerCallback();
    require(listener.dirty==dirtyBeforeIdle,"idle editor generated phantom edits");
    editor->performUndoRedo(false); require(processor.getAnalysisResult().regions.isEmpty(),"ordinary region Undo");
    editor->performUndoRedo(true); require(processor.getAnalysisResult().regions.size()==1,"ordinary region Redo");
    editor->breathGainSlider.setValue(-12,juce::sendNotificationSync); editor->timerCallback();
    editor->breathNormButton.setToggleState(true,juce::sendNotificationSync); editor->timerCallback();
    auto eq=processor.getBreathEqState(); eq.enabled=true; eq.bands[0].enabled=true; eq.bands[0].gainDb=5;
    editor->applyBreathEqStateFromUi(eq); editor->timerCallback();
    editor->performUndoRedo(false); require(!processor.getBreathEqState().enabled,"ordinary EQ Undo");
    editor->performUndoRedo(false); require(!editor->breathNormButton.getToggleState(),"ordinary Norm Undo");
    editor->performUndoRedo(false); require(editor->breathGainSlider.getValue()==0,"ordinary Gain Undo");
    editor->performUndoRedo(true); editor->performUndoRedo(true); editor->performUndoRedo(true);
    require(editor->breathGainSlider.getValue()==-12 && editor->breathNormButton.getToggleState() && processor.getBreathEqState().enabled,"ordinary full Redo");
    editor.reset(); editor=std::make_unique<QQDeBreathAudioProcessorEditor>(processor);
    std::cout<<"ordinary reopen before Undo gain="<<editor->breathGainSlider.getValue()<<" norm="<<editor->breathNormButton.getToggleState()<<" eq="<<processor.getBreathEqState().enabled<<" canUndo="<<editor->canUndoRedo(false)<<"\n";
    editor->performUndoRedo(false); require(!processor.getBreathEqState().enabled,"ordinary reopen history");
    juce::MemoryBlock saved; processor.getStateInformation(saved);
    editor->breathGainSlider.setValue(-22,juce::sendNotificationSync); editor->timerCallback();
    processor.setStateInformation(saved.getData(),static_cast<int>(saved.getSize())); editor->timerCallback();
    require(processor.getAnalysisResult().regions[0].gainDb==-7,"ordinary host archive region restore");
    require(processor.parameters.getRawParameterValue(QQDeBreath::ParamIDs::breathGainDb)->load()==-12,"ordinary host archive parameter restore");
    processor.removeListener(&listener);
    std::cout<<"PASS: ordinary VST/AU shared editor region/Gain/Norm/EQ Undo/Redo, reopen and host state restore\n";
 }
 static void run(ARA::PlugIn::DocumentController* dc, QQDeBreathARADocumentController* owner,
                 ARA::ARADocumentControllerRef ref, AudioHost& host, ArchiveHost& archive, UpdateHost& updates)
 {
    std::array<ARA::ARAAudioSourceRef,3> sources;
    std::array<ARA::ARAAudioModificationRef,3> mods;
    std::array<ARA::ARAPlaybackRegionRef,3> regions;
    std::array<ARA::ARARegionSequenceRef,3> sequences;
    dc->beginEditing();
    ARA::ARAMusicalContextProperties cp {}; cp.structSize=sizeof(cp); cp.name="selection test";
    auto context=dc->createMusicalContext(reinterpret_cast<ARA::ARAMusicalContextHostRef>(&sources),&cp);
    for(int i=0;i<3;++i) {
      auto name="selection-track-"+juce::String(i);
      ARA::ARARegionSequenceProperties sp {}; sp.structSize=sizeof(sp); sp.name=name.toRawUTF8(); sp.musicalContextRef=context; sp.orderIndex=i;
      sequences[i]=dc->createRegionSequence(reinterpret_cast<ARA::ARARegionSequenceHostRef>(&host.sources[i]),&sp);
      host.sources[i].value=0.125f*(i+1);
      ARA::ARAAudioSourceProperties ap {}; ap.structSize=sizeof(ap); ap.name=name.toRawUTF8(); ap.persistentID=name.toRawUTF8(); ap.sampleCount=48000; ap.sampleRate=48000; ap.channelCount=1;
      sources[i]=dc->createAudioSource(reinterpret_cast<ARA::ARAAudioSourceHostRef>(&host.sources[i]),&ap);
      ARA::ARAAudioModificationProperties mp {}; mp.structSize=sizeof(mp); mp.name=name.toRawUTF8(); mp.persistentID=name.toRawUTF8();
      mods[i]=dc->createAudioModification(sources[i],reinterpret_cast<ARA::ARAAudioModificationHostRef>(&host.sources[i]),&mp);
      ARA::ARAPlaybackRegionProperties rp {}; rp.structSize=sizeof(rp); rp.durationInModificationTime=1; rp.durationInPlaybackTime=1; rp.startInPlaybackTime=i*2; rp.regionSequenceRef=sequences[i];
      regions[i]=dc->createPlaybackRegion(mods[i],reinterpret_cast<ARA::ARAPlaybackRegionHostRef>(&host.sources[i]),&rp);
      dc->enableAudioSourceSamplesAccess(sources[i],true);
    }
    dc->endEditing();
    {
      QQDeBreathAudioProcessor processor;
      auto roles=ARA::kARAPlaybackRendererRole|ARA::kARAEditorViewRole;
      processor.bindToARA(ref,roles,roles);
      processor.getPlaybackRenderer()->addPlaybackRegion(regions[0]);
      auto* view=processor.getEditorView();
      auto select=[&](int i) { ARA::ARAViewSelection s {}; s.structSize=sizeof(s); s.playbackRegionRefsCount=1; s.playbackRegionRefs=&regions[i]; view->notifySelection(&s); };
      select(0);
      auto editor=std::make_unique<QQDeBreathAudioProcessorEditor>(processor);
      editor->reloadAraSource();
      auto verify=[&](int i) {
        if (editor->araSourceInfo.persistentId!="selection-track-"+juce::String(i))
          throw std::runtime_error("editor stayed bound to previous track: expected="+std::to_string(i)+" actual="+editor->araSourceInfo.persistentId.toStdString()+" status="+editor->araSourceInfo.exportStatus.toStdString());
        juce::AudioFormatManager fm; fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader(fm.createReaderFor(editor->araSourceInfo.exportedWav));
        require(reader!=nullptr,"waveform file missing"); juce::AudioBuffer<float> audio(1,64);
        require(reader->read(&audio,0,64,0,true,false),"waveform read");
        require(std::abs(audio.getSample(0,0)-host.sources[i].value)<1.0e-5f,"wrong track waveform");
      };
      verify(0);
      QQDeBreathBridgeRegion a; a.type="Breath"; a.startSample=4800; a.endSample=9600; a.startTime=0.1; a.endTime=0.2; a.gainDb=-3;
      editor->waveformRegionsChanged({a});
      editor->breathGainSlider.setValue(-4,juce::sendNotificationSync);
      editor->syncAraPlaybackParams();
      const auto keyA=editor->araSourceInfo.sourceFingerprint;
      select(1); editor->timerCallback(); verify(1);
      require(processor.getAnalysisResult().regions.isEmpty(),"fresh track inherited edits");
      auto b=a; b.type="Noise"; b.gainDb=-9;
      editor->waveformRegionsChanged({b});
      editor->breathGainSlider.setValue(-11,juce::sendNotificationSync);
      editor->syncAraPlaybackParams();
      select(0); editor->timerCallback(); verify(0);
      require(processor.getAnalysisResult().regions.size()==1 && processor.getAnalysisResult().regions[0].gainDb==-3,"track A edits lost");
      require(editor->breathGainSlider.getValue()==-4,"track A parameters lost");
      select(1); editor->timerCallback(); verify(1);
      require(processor.getAnalysisResult().regions[0].gainDb==-9,"track B edits lost");
      require(editor->breathGainSlider.getValue()==-11,"track B parameters lost");
      QQDeBreathARAPersistentState state;
      require(owner->getPersistentStateForSource(keyA,state) && state.analysisResult.regions[0].gainDb==-3,"switch overwrote other track");
      std::cout<<"PASS: actual ARA EditorView A/B switch, distinct waveforms and independent edits\n";
      const auto keyB=editor->araSourceInfo.sourceFingerprint;
      for(int i=0;i<20;++i) {select(i%2); editor->timerCallback(); verify(i%2);}
      std::cout<<"PASS: rapid A/B selection loop\n";
      require(processor.getAnalysisResult().regions[0].gainDb==-9,"rapid switch lost edits");
      QQDeBreathBridgeAnalysisResult late; late.hasResult=late.succeeded=true; late.sourceKey=keyA; late.regions.add(a);
      editor->handleAnalysisFinished(late);
      require(processor.getAnalysisResult().sourceKey==keyB && processor.getAnalysisResult().regions[0].gainDb==-9,"late analysis crossed tracks");
      editor->reloadAraSource(); verify(1);
      std::cout<<"PASS: Load retains selected source\n";
      require(processor.getAnalysisResult().regions[0].gainDb==-9,"Load erased saved edits");
      editor.reset(); select(1); editor=std::make_unique<QQDeBreathAudioProcessorEditor>(processor); verify(1);
      require(editor->breathGainSlider.getValue()==-11 && processor.getAnalysisResult().regions[0].gainDb==-9,"reopen lost current track");
      std::cout<<"PASS: rapid switching, late result rejected, Load and editor reopen preserve edits/parameters\n";

      // Sequence-only selection is how a host can express track selection without event refs.
      ARA::ARAViewSelection sequenceSelection {}; sequenceSelection.structSize=sizeof(sequenceSelection);
      sequenceSelection.regionSequenceRefsCount=1; sequenceSelection.regionSequenceRefs=&sequences[0];
      view->notifySelection(&sequenceSelection); editor->timerCallback(); verify(0);
      require(editor->breathGainSlider.getValue()==-4,"sequence selection parameters");
      require(editor->araSourceInfo.playbackMappings.size()==1 && editor->araSourceInfo.playbackMappings[0].playbackStartSeconds==0,"wrong selected event mapping");

      // Roll back unapplied selected-region preview before binding a different source.
      editor->waveformEditor.setAnalysisResult(processor.getAnalysisResult());
      editor->selectedRegionIndex=0; editor->showBreathDetailPage();
      editor->breathDetailAutoApplyButton.setToggleState(false,juce::dontSendNotification);
      editor->breathDetailGainSlider.setValue(-19,juce::sendNotificationSync);
      const auto readsBeforeSwitch=host.reads;
      select(1); editor->handleUpdateNowIfNeeded(); verify(1);
      require(host.reads==readsBeforeSwitch,"selection reread host audio instead of cache");
      select(0); editor->timerCallback(); verify(0);
      require(processor.getAnalysisResult().regions[0].gainDb==-3,"unapplied preview committed on switch");

      // Auto-applied changes still pending their throttled persistence must be flushed to A.
      editor->selectedRegionIndex=0; editor->showBreathDetailPage();
      editor->breathDetailAutoApplyButton.setToggleState(true,juce::dontSendNotification);
      editor->breathDetailGainSlider.setValue(-6,juce::sendNotificationSync);
      select(1); editor->timerCallback(); verify(1);
      select(0); editor->timerCallback(); verify(0);
      require(processor.getAnalysisResult().regions[0].gainDb==-6,"pending applied edit lost");
      std::cout<<"PASS: sequence-only selection, unapplied preview rollback, pending applied edit flush\n";

      ARA::ARAViewSelection group {}; group.structSize=sizeof(group); group.playbackRegionRefsCount=2; group.playbackRegionRefs=regions.data();
      view->notifySelection(&group); editor->timerCallback();
      require(editor->araSourceInfo.isComposite && processor.getAnalysisResult().regions.size()==2,"group lost source edits");
      require(owner->getPersistentStateForSource(keyB,state) && state.playbackParams.breathGainDb==-11,"group load changed track B parameters");
      const auto groupKey=editor->araSourceInfo.sourceFingerprint;
      select(1); editor->timerCallback(); verify(1);
      view->notifySelection(&group); editor->timerCallback();
      require(editor->araSourceInfo.sourceFingerprint==groupKey && processor.getAnalysisResult().regions.size()==2,"group restore lost edits");
      select(1); editor->timerCallback();
      std::cout<<"PASS: multi-event selection and return preserve existing edits without writing group defaults to tracks\n";

      // New source temporarily unavailable: clear previous waveform, then retry correctly.
      dc->enableAudioSourceSamplesAccess(sources[2],false);
      select(2); editor->timerCallback();
      require(!editor->araPlaybackCacheReady && processor.getAnalysisResult().regions.isEmpty() && !editor->lastExportedFile.existsAsFile(),"failed load kept old track visible");
      dc->enableAudioSourceSamplesAccess(sources[2],true);
      editor->reloadAraSource(); verify(2);
      require(processor.getAnalysisResult().regions.isEmpty(),"fresh source inherited regions");
      select(1); editor->timerCallback(); verify(1);

      // A second instance/view with independent host selection cannot steal the first view.
      {
        QQDeBreathAudioProcessor other; other.bindToARA(ref,roles,roles);
        other.getPlaybackRenderer()->addPlaybackRegion(regions[0]);
        ARA::ARAViewSelection s {}; s.structSize=sizeof(s); s.playbackRegionRefsCount=1; s.playbackRegionRefs=&regions[0];
        other.getEditorView()->notifySelection(&s);
        auto otherEditor=std::make_unique<QQDeBreathAudioProcessorEditor>(other);
        otherEditor->timerCallback(); editor->timerCallback(); verify(1);
        require(otherEditor->araSourceInfo.persistentId=="selection-track-0" && other.getAnalysisResult().regions[0].gainDb==-6,"views contaminated each other");
        otherEditor.reset(); other.getPlaybackRenderer()->removePlaybackRegion(regions[0]);
      }
      std::cout<<"PASS: unavailable-source retry and two independent editor views\n";
      // Internal Undo/Redo includes full parameters and survives Load and editor recreation.
      editor->breathGainSlider.setValue(-17,juce::sendNotificationSync); editor->timerCallback();
      require(editor->canUndoRedo(false),"no internal parameter Undo");
      editor->reloadAraSource(); editor->performUndoRedo(false);
      require(editor->breathGainSlider.getValue()==-11,"Reload lost parameter Undo");
      editor->reloadAraSource(); editor->performUndoRedo(true);
      require(editor->breathGainSlider.getValue()==-17,"Reload lost parameter Redo");
      editor.reset(); select(1); editor=std::make_unique<QQDeBreathAudioProcessorEditor>(processor);
      editor->performUndoRedo(false);
      require(editor->breathGainSlider.getValue()==-11,"reopen lost parameter Undo");
      editor->performUndoRedo(true);
      dc->notifyModelUpdates();
      require(updates.sources>0 && updates.modifications>0,"ARA source/modification notifications missing");

      // Emulate the host's source-filtered Undo/Redo archives through the public ARA API.
      ARA::ARAStoreObjectsFilter store {}; store.structSize=sizeof(store); store.audioSourceRefsCount=1; store.audioSourceRefs=&sources[1];
      auto writer=reinterpret_cast<ARA::ARAArchiveWriterHostRef>(&archive);
      auto reader=reinterpret_cast<ARA::ARAArchiveReaderHostRef>(&archive);
      archive.bytes.clear(); require(dc->storeObjectsToArchive(writer,&store),"host Undo snapshot");
      const auto before=archive.bytes;
      editor->breathGainSlider.setValue(-23,juce::sendNotificationSync); editor->timerCallback();
      archive.bytes.clear(); require(dc->storeObjectsToArchive(writer,&store),"host Redo snapshot");
      const auto after=archive.bytes;
      dc->notifyModelUpdates(); const int notifications=updates.count();
      const ARA::ARAPersistentID id="selection-track-1";
      ARA::ARARestoreObjectsFilter restore {}; restore.structSize=sizeof(restore); restore.audioSourceIDsCount=1; restore.audioSourceArchiveIDs=&id;
      archive.bytes=before; dc->beginEditing(); require(dc->restoreObjectsFromArchive(reader,&restore),"host Undo restore"); dc->endEditing();
      editor->timerCallback(); verify(1);
      require(editor->breathGainSlider.getValue()==-17,"open editor ignored host Undo");
      dc->notifyModelUpdates(); require(updates.count()==notifications,"host Undo echoed as new edit");
      archive.bytes=after; dc->beginEditing(); require(dc->restoreObjectsFromArchive(reader,&restore),"host Redo restore"); dc->endEditing();
      editor->timerCallback(); require(editor->breathGainSlider.getValue()==-23,"open editor ignored host Redo");
      dc->notifyModelUpdates(); require(updates.count()==notifications,"host Redo echoed as new edit");
      std::cout<<"PASS: full parameter Undo/Redo across Reload/reopen; source-scoped host notifications; host archive Undo/Redo refresh without feedback\n";

      // Analysis belongs to each source, not to this disposable editor view.
      struct Gate { std::atomic<bool> open{false}; std::atomic<int> entered{0}; };
      auto gate = std::make_shared<Gate>();
      struct ReleaseGate { std::shared_ptr<Gate> gate; ~ReleaseGate() { gate->open.store(true); } } releaseGate{gate};
      for (int i=0;i<2;++i)
          owner->analysisJobs.pool->threads.addJob([gate] {
              ++gate->entered;
              while (!gate->open.load()) std::this_thread::sleep_for(std::chrono::milliseconds(1));
          });
      const auto gateDeadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
      while (gate->entered.load()!=2) {
          pumpHostMessages();
          require(std::chrono::steady_clock::now()<gateDeadline,"analysis worker pool did not start");
          std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
      select(0); editor->timerCallback();
      const auto beforeA = owner->getSourceAnalysisRevision(keyA);
      const auto beforeB = owner->getSourceAnalysisRevision(keyB);
      const auto readsBeforeBackground = host.reads;
      editor->startAnalysis();
      require(owner->isSourceAnalysisRunning(keyA), "track A background analysis not registered");
      select(1); editor->timerCallback();
      require(editor->analyzeButton.isEnabled(), "track A job disabled Analyze on track B");
      editor->startAnalysis();
      require(owner->isSourceAnalysisRunning(keyB), "track B background analysis not registered");
      select(2); editor->timerCallback(); verify(2);
      editor.reset();
      require(owner->isSourceAnalysisRunning(keyA) && owner->isSourceAnalysisRunning(keyB),
              "switching or closing editor cancelled queued jobs");
      gate->open.store(true);
      const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
      while (owner->isSourceAnalysisRunning(keyA) || owner->isSourceAnalysisRunning(keyB))
      {
          pumpHostMessages(); // Document timer commits even with no editor.
          require(std::chrono::steady_clock::now() < deadline, "background analysis did not finish after editor close");
          std::this_thread::sleep_for(std::chrono::milliseconds(5));
      }
      require(owner->getSourceAnalysisRevision(keyA) > beforeA && owner->getSourceAnalysisRevision(keyB) > beforeB,
              "switching/closing cancelled a source analysis");
      require(owner->getPersistentStateForSource(keyA,state) && state.analysisResult.succeeded && state.analysisResult.sourceKey==keyA,
              "track A background result missing/misrouted");
      require(state.playbackParams.breathGainDb==-4,"track A background result overwrote parameters");
      require(owner->getPersistentStateForSource(keyB,state) && state.analysisResult.succeeded && state.analysisResult.sourceKey==keyB,
              "track B background result missing/misrouted");
      require(state.playbackParams.breathGainDb==-23,"track B background result overwrote parameters");
      require(host.reads==readsBeforeBackground,"background analysis read host audio instead of immutable cache");
      view->notifySelection(&group);
      editor=std::make_unique<QQDeBreathAudioProcessorEditor>(processor); editor->timerCallback();
      require(editor->araSourceInfo.isComposite && processor.getAnalysisResult().regions.isEmpty(),
              "cached group snapshot hid completed source analyses");
      select(0); editor->timerCallback(); verify(0);
      require(processor.getAnalysisResult().succeeded && processor.getAnalysisResult().regions.isEmpty(),"completed A analysis not restored on return");
      editor->performUndoRedo(false);
      require(processor.getAnalysisResult().regions.size()==1 && processor.getAnalysisResult().regions[0].gainDb==-6,
              "background completion lost pre-analysis Undo");
      editor->startAnalysis(); editor->cancelAnalysis();
      require(!owner->isSourceAnalysisRunning(keyA),"explicit Cancel did not cancel selected source");
      require(processor.getAnalysisResult().regions.size()==1,"Cancel erased previous edits");
      select(1); editor->timerCallback(); editor->startAnalysis();
      archive.bytes=before;
      dc->beginEditing(); require(dc->restoreObjectsFromArchive(reader,&restore),"archive while analyzing"); dc->endEditing();
      require(!owner->isSourceAnalysisRunning(keyB),"host Undo left a stale analysis job alive");
      owner->pollSourceAnalyses(); editor->timerCallback();
      require(editor->breathGainSlider.getValue()==-17,"late analysis overwrote host Undo");
      std::cout<<"PASS: independent A/B background jobs survive switching/closed editor; timer completion; cached input/group refresh; pre-analysis Undo; explicit Cancel and host Undo invalidate stale jobs\n";
      editor.reset();
      processor.getPlaybackRenderer()->removePlaybackRegion(regions[0]);
    }
    dc->beginEditing();
    for(int i=0;i<3;++i) { dc->destroyPlaybackRegion(regions[i]); dc->destroyAudioModification(mods[i]); dc->enableAudioSourceSamplesAccess(sources[i],false); dc->destroyAudioSource(sources[i]); dc->destroyRegionSequence(sequences[i]); }
    dc->destroyMusicalContext(context); dc->endEditing();
 }
};
int main() {
 try {
    juce::ScopedJuceInitialiser_GUI init;
    QQEasyToolAraSelectionProbe::ordinary();
    auto* factory=createARAFactory();
    const ARA::SizedStruct<ARA_STRUCT_MEMBER(ARAInterfaceConfiguration, assertFunctionAddress)> config {ARA::kARAAPIGeneration_2_0_Final,nullptr};
    factory->initializeARAWithConfiguration(&config);
    AudioHost audio; ArchiveHost archive; UpdateHost updates; archive.archiveId=factory->documentArchiveID;
    ARA::Host::DocumentControllerHostInstance host(&audio,&archive,nullptr,&updates);
    const ARA::SizedStruct<ARA_STRUCT_MEMBER(ARADocumentProperties,name)> props {"QQEasyTool selection regression"};
    auto* instance=factory->createDocumentControllerWithDocument(&host,&props);
    auto* dc=static_cast<ARA::PlugIn::DocumentController*>(ARA::PlugIn::fromRef(instance->documentControllerRef));
    auto* owner=juce::ARADocumentControllerSpecialisation::getSpecialisedDocumentController<QQDeBreathARADocumentController>(dc);
    QQEasyToolAraSelectionProbe::run(dc,owner,instance->documentControllerRef,audio,archive,updates);
    dc->destroyDocumentController(); factory->uninitializeARA();
    require(!audio.wrongThread,"editor read host audio off model thread");
    return 0;
 } catch(const std::exception& e) {std::cerr<<"FAIL: "<<e.what()<<"\n"; return 1;}
}
