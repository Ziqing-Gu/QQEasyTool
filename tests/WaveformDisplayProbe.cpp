#include "shared/WaveformEditorComponent.h"
#include "LegacyEasyWaveform104.h"
#include <iostream>
#include <chrono>
#include <stdexcept>
class QQDeBreathWaveformDisplayProbe
{
public:
    static void check(bool yes,const char* message){if(!yes)throw std::runtime_error(message);}
    static void settle(QQDeBreathWaveformEditor& editor)
    {
        const auto deadline=juce::Time::getMillisecondCounter()+15000;
        while(editor.displayAppliedRevision!=editor.displayRequestedRevision && juce::Time::getMillisecondCounter()<deadline)
        {editor.timerCallback();juce::Thread::sleep(1);}
        check(editor.displayAppliedRevision==editor.displayRequestedRevision,"latest waveform timeout");
    }
    static double compare(QQDeBreathWaveformEditor& current,LegacyEasyWaveform104& old,int samples)
    {
        settle(current);double error=0;
        for(int i=0;i<samples;++i)for(bool active:{false,true}){
            auto a=current.renderedDisplaySample(i,active),b=old.renderedDisplaySample(i,active);
            auto relative=std::abs(a-b)/juce::jmax(1.0,std::abs(b));error=juce::jmax(error,relative);
        }check(error<5e-6,"waveform reference mismatch");return error;
    }
    static juce::AudioBuffer<float> audio(int samples)
    {
        juce::AudioBuffer<float> data(1,samples);
        for(int i=0;i<samples;++i)data.setSample(0,i,static_cast<float>(0.2*std::sin(i*0.031)+0.05*std::cos(i*0.083)));
        return data;
    }
    static QQDeBreathBridgeAnalysisResult analysis(int samples,int count)
    {
        QQDeBreathBridgeAnalysisResult r;r.succeeded=true;
        for(int i=0;i<count;++i){QQDeBreathBridgeRegion region;const char* types[] = {"Noise", "Breath", "Others", "Sibilance", "Breath", "Breath"}; region.type=types[i%6];
            region.startSample=i*samples/count;region.endSample=juce::jmin<juce::int64>(samples,region.startSample+samples/count/2);
            region.startTime=region.startSample/48000.0;region.endTime=region.endSample/48000.0;
            region.gainDb=(i%3-1)*3;r.regions.add(region);}
        return r;
    }
    static void run()
    {
        double worst=0;
        for(double rate:{44100.0,48000.0,96000.0})for(int mode=0;mode<10;++mode){
            auto data=audio(8000);auto result=analysis(8000,12);
            // A silent core with nonzero adjoining fades exercises Norm Target fallback.
            auto& zero=result.regions.getReference(1);zero.startSample=1000;zero.endSample=1200;data.clear(0,1000,200);
            auto& adjacent=result.regions.getReference(2);adjacent.startSample=1200;adjacent.endSample=1600;
            auto& overlap=result.regions.getReference(3);overlap.startSample=1400;overlap.endSample=1900;
            QQDeBreathWaveformEditor current;LegacyEasyWaveform104 old;
            current.setAudioBuffer(data,rate);old.setAudioBuffer(data,rate);
            QQDeBreathWaveformEditor::DisplayProcessingParams p;
            p.normalizeBreath=mode%2==1;p.enableFade=mode!=0;p.fadeInMs=3;p.fadeOutMs=6;
            if(mode>=2){p.breathEqState.enabled=true;p.breathEqState.highPassEnabled=true;p.breathEqState.bands[0].enabled=true;p.breathEqState.bands[0].gainDb=5;
                result.regions.getReference(2).eqState=p.breathEqState;}
            if(mode>=8) { for(auto& r : result.regions) { r.gainDb=0; r.eqState={}; } p.breathEqState={}; p.normalizeBreath=false; }
            p.normalizeSibilance=mode%2==1; p.sibilanceGainDb=-9; p.sibilanceTargetDb=-18; p.sibilanceEqState=p.breathEqState;
            current.setAnalysisResult(result);old.setAnalysisResult(result);
            const auto oldParams=[&]{LegacyEasyWaveform104::DisplayProcessingParams q;q.enableFade=p.enableFade;q.normalizeBreath=p.normalizeBreath;q.fadeInMs=p.fadeInMs;q.fadeOutMs=p.fadeOutMs;q.breathTargetDb=p.breathTargetDb;q.breathGainDb=p.breathGainDb;q.breathEqState=p.breathEqState;q.normalizeSibilance=p.normalizeSibilance;q.sibilanceGainDb=p.sibilanceGainDb;q.sibilanceTargetDb=p.sibilanceTargetDb;q.sibilanceEqState=p.sibilanceEqState;return q;};
            current.setProcessingParams(p);old.setProcessingParams(oldParams());settle(current);
            const auto revision=current.displayRequestedRevision;
            for(int step=0;step<6;++step){const double gains[] = {-10, 0, 0.0005, -0.0005, 0.002, 4};p.breathGainDb=gains[step];p.breathTargetDb=-30+step*6;
                current.setProcessingParams(p);old.setProcessingParams(oldParams());check(current.displayRequestedRevision==revision,"scalar drag queued full render");
                current.setMonitorState(mode%3!=0,mode%3!=1,mode%3!=2,mode%4!=2,mode%4!=3);old.setMonitorState(mode%3!=0,mode%3!=1,mode%3!=2,mode%4!=2,mode%4!=3);
                worst=juce::jmax(worst,compare(current,old,8000));}
            p.breathEqState.bands[0].gainDb=-7;current.setProcessingParams(p);old.setProcessingParams(oldParams());
            worst=juce::jmax(worst,compare(current,old,8000));
            current.setRegionProcessing(2,-9,p.breathEqState,false,false);old.setRegionProcessing(2,-9,p.breathEqState,false,false);
            current.refreshProcessedDisplay();old.refreshProcessedDisplay();worst=juce::jmax(worst,compare(current,old,8000));
        }
        std::cout<<"30 waveform cases (6 scalar settings plus global/region EQ each); max relative display error="<<worst<<std::endl;
        {
            auto data=audio(48000*120);auto result=analysis(data.getNumSamples(),500);
            QQDeBreathWaveformEditor current;current.setSize(1180,500);current.setAudioBuffer(data,48000);current.setAnalysisResult(result);settle(current);
            QQDeBreathWaveformEditor::DisplayProcessingParams p;p.normalizeBreath=true;p.breathEqState.enabled=true;p.breathEqState.bands[0].enabled=true;
            current.setProcessingParams(p);settle(current);
            auto revision=current.displayRequestedRevision;
            auto begin=std::chrono::steady_clock::now();
            for(int i=0;i<1000;++i){p.breathGainDb=-30+i%60;p.breathTargetDb=-30+i%30;current.setProcessingParams(p);}
            const auto scalarMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();
            check(current.displayRequestedRevision==revision,"scalar update invalidated cache");
            begin=std::chrono::steady_clock::now();
            for(int i=0;i<500;++i){p.breathEqState.bands[0].frequencyHz=100+i*17;p.breathEqState.bands[0].gainDb=i%20-10;current.setProcessingParams(p);}
            const auto eqMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();
            settle(current);check(current.displayAppliedRevision==current.displayRequestedRevision,"stale EQ result applied");
            juce::Image image(juce::Image::RGB,1180,500,true);juce::Graphics graphics(image);begin=std::chrono::steady_clock::now();current.paint(graphics);
            std::cout<<"120 sec / 500 regions: 1000 Gain/Target updates="<<scalarMs<<"ms, 500 EQ submissions="<<eqMs<<"ms, paint="<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count()<<"ms"<<std::endl;
            auto altered=audio(data.getNumSamples());altered.applyGain(0.1f);current.setAudioBuffer(altered,48000);settle(current);
            current.clearAudio();juce::Thread::sleep(10);current.timerCallback();check(current.processedBreathDisplay.getNumSamples()==0,"stale source result after clear");
        }
        {
            auto current=std::make_unique<QQDeBreathWaveformEditor>();auto data=audio(48000*30);current->setAudioBuffer(data,48000);current->setAnalysisResult(analysis(data.getNumSamples(),500));current.reset();
        }
        std::cout<<"PASS: scalar cache reuse, legacy waveform agreement, latest EQ wins, source invalidation, background close"<<std::endl;
    }
};
int main(){juce::ScopedJuceInitialiser_GUI init;try{QQDeBreathWaveformDisplayProbe::run();return 0;}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<std::endl;return 1;}}
