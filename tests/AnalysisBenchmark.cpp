#include "shared/NativeAnalysis.h"
#include <iostream>
#include <chrono>
class QQLegacyNativeAnalysis {
public:
 static QQDeBreathBridgeAnalysisResult run(const QQDeBreathBridgeAnalysisConfig&, const QQDeBreathBridgeAnalysis::ShouldCancel&);
};
int wmain(int argc,wchar_t* argv[]) {
 if(argc<3)return 2;
 QQDeBreathBridgeAnalysisConfig c; c.inputWav=juce::File(juce::String(argv[1]));c.sourceKey="benchmark";
 auto before=std::chrono::steady_clock::now();
 auto old=QQLegacyNativeAnalysis::run(c,[]{return false;});
 auto middle=std::chrono::steady_clock::now();
 auto now=QQDeBreathNativeAnalysis::run(c,[]{return false;});
 auto end=std::chrono::steady_clock::now();
 bool equal=old.succeeded && now.succeeded && old.regions.size()==now.regions.size();
 for(int i=0;equal && i<old.regions.size();++i){
  const auto& a=old.regions[i];const auto& b=now.regions[i];
  equal=a.type==b.type && a.startSample==b.startSample && a.endSample==b.endSample && a.startTime==b.startTime && a.endTime==b.endTime;
 }
 auto* o=new juce::DynamicObject;
 o->setProperty("input",c.inputWav.getFullPathName());o->setProperty("seconds",now.durationSeconds);o->setProperty("sampleRate",now.sampleRate);
 o->setProperty("oldSeconds",std::chrono::duration<double>(middle-before).count());o->setProperty("newSeconds",std::chrono::duration<double>(end-middle).count());
 o->setProperty("identicalRegions",equal);o->setProperty("oldRegions",old.regions.size());o->setProperty("newRegions",now.regions.size());
 juce::var v(o);const auto json=juce::JSON::toString(v);
 juce::File(juce::String(argv[2])).replaceWithText(json);
 std::cout<<json.toStdString()<<std::endl;return equal?0:1;
}
