#include <juce_audio_processors/juce_audio_processors.h>
#include "control/Parameters.hpp"
#include <cstdlib>
#include <iostream>
#include <memory>

void require(bool ok,const char* msg){if(!ok){std::cerr<<msg<<'\n';std::exit(1);}}
int main(){
    juce::ScopedJuceInitialiser_GUI init;
    std::unique_ptr<juce::AudioProcessor> p(createPluginFilter());
    require(p!=nullptr,"factory");
    require(p->getParameters().size()==int(cyberator::Parameters::count),"parameter count");
    for(const auto& s:cyberator::specs){
        bool found=false;
        for(auto* param:p->getParameters()){
            auto* withId=dynamic_cast<juce::AudioProcessorParameterWithID*>(param);
            if(withId&&withId->paramID==juce::String::fromUTF8(s.id.data(),int(s.id.size())))found=true;
        }
        require(found,"stable parameter id");
    }
    juce::MemoryBlock original;
    p->getStateInformation(original);
    require(original.getSize()==cyberator::Parameters::bytes,"native state size");
    cyberator::Parameters values;
    require(values.restore(std::span(static_cast<const std::uint8_t*>(original.getData()),original.getSize())),"native state decodes");
    require(values.set(cyberator::Param::mix,0)&&values.set(cyberator::Param::ghost,1)&&values.set(cyberator::Param::output,12),"fixture");
    auto expected=values.save();
    p->setStateInformation(expected.data(),int(expected.size()));
    juce::MemoryBlock recalled;
    p->getStateInformation(recalled);
    require(recalled.getSize()==expected.size()&&std::memcmp(recalled.getData(),expected.data(),expected.size())==0,"native recall");
    auto broken=expected;broken[9]^=1;
    p->setStateInformation(broken.data(),int(broken.size()));
    juce::MemoryBlock after;p->getStateInformation(after);
    require(after==recalled,"corrupt state rollback");
    for(int channels:{1,2}){
        auto layout=p->getBusesLayout();
        auto set=channels==1?juce::AudioChannelSet::mono():juce::AudioChannelSet::stereo();
        layout.inputBuses.set(0,set);layout.outputBuses.set(0,set);
        require(p->setBusesLayout(layout),"supported layout");
        p->setRateAndBufferSizeDetails(48000,127);
        p->prepareToPlay(48000,127);
        const int latency=p->getLatencySamples();
        require(latency==9728,"reported latency");
        for(bool bypass:{false,true}){
            p->reset();
            juce::MidiBuffer midi;
            for(int offset=0;offset<latency+200;){
                const int n=std::min(127,latency+200-offset);
                juce::AudioBuffer<float> audio(channels,n);audio.clear();
                if(offset==0){audio.setSample(0,0,.5f);if(channels==2)audio.setSample(1,0,-.25f);}
                if(bypass)p->processBlockBypassed(audio,midi);else p->processBlock(audio,midi);
                for(int i=0;i<n;++i){
                    require(audio.getSample(0,i)==(offset+i==latency?.5f:0),"native aligned dry");
                    if(channels==2)require(audio.getSample(1,i)==(offset+i==latency?-.25f:0),"native stereo dry");
                }
                offset+=n;
            }
        }
        p->releaseResources();
    }
    std::cout<<"Native processor smoke PASS\n";
}
