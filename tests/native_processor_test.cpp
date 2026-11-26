#include <juce_audio_processors/juce_audio_processors.h>
#include "control/SessionState.hpp"
#include "control/TailPolicy.hpp"
#include "control/FactoryBank.hpp"
#include "gui/PanelLayout.hpp"
#include <cstdlib>
#include <iostream>
#include <memory>
#include <new>
#include <thread>
#include <atomic>
#include "support/AllocationProbe.hpp"

void require(bool ok,const char* msg){if(!ok){std::cerr<<msg<<'\n';std::exit(1);}}

int allocationCases(juce::AudioProcessor& p){
    using namespace cyberator;
    using cyberator::test::AllocationProbe;
    Parameters controls;
    const std::pair<Param,float> active[]={
        {Param::granular,.8f},{Param::size,80},{Param::density,.85f},{Param::position,.2f},{Param::scatter,.4f},
        {Param::pitch,5},{Param::reverse,.4f},{Param::feedback,.5f},{Param::spread,.7f},
        {Param::formant,4},{Param::android,.6f},{Param::spectral,.7f},{Param::harmony,.6f},
        {Param::ghost,.6f},{Param::swell,.6f},{Param::delay,.5f},{Param::space,.5f},{Param::motion,.5f},{Param::output,-6}
    };
    for(const auto& [id,v]:active)require(controls.set(id,v),"active allocation fixture");
    auto find=[&](Param id){
        const auto key=specs[std::size_t(id)].id;
        for(auto* param:p.getParameters())if(param->getName(128)==juce::String::fromUTF8(key.data(),int(key.size())))return param;
        return static_cast<juce::AudioProcessorParameter*>(nullptr);
    };
    auto* pitch=find(Param::pitch);auto* freeze=find(Param::freeze);auto* mode=find(Param::mode);
    require(pitch&&freeze&&mode,"automation fixture parameters");
    // Confirm interposition in this linked executable before trusting zero counts.
    void* (*volatile allocate)(std::size_t)=::operator new;
    void (*volatile deallocate)(void*)noexcept=::operator delete;
    {AllocationProbe probe;auto* ptr=allocate(64);deallocate(ptr);const auto n=probe.finish();
        require(n.allocations==1&&n.frees==1,"native test deliberate allocation detected");}
    std::size_t callbacks=0;
    for(double rate:{48000.,96000.,192000.})for(int channels:{1,2})for(bool editorOpen:{false,true}){
        auto layout=p.getBusesLayout();auto set=channels==1?juce::AudioChannelSet::mono():juce::AudioChannelSet::stereo();
        layout.inputBuses.set(0,set);layout.outputBuses.set(0,set);require(p.setBusesLayout(layout),"allocation buses");
        const auto bytes=controls.save();p.setStateInformation(bytes.data(),int(bytes.size()));
        p.setRateAndBufferSizeDetails(rate,256);p.prepareToPlay(rate,256);
        auto destroy=[&](juce::AudioProcessorEditor* editor){if(editor){p.editorBeingDeleted(editor);delete editor;}};
        std::unique_ptr<juce::AudioProcessorEditor,decltype(destroy)> editor(editorOpen?p.createEditorAndMakeActive():nullptr,destroy);
        require(!editorOpen||editor!=nullptr,"allocation editor open");
        juce::AudioBuffer<float> audio(channels,256);juce::MidiBuffer midi;
        double energy=0;int position=0;std::size_t allocs=0,frees=0;
        for(int block=0;position<int(rate*.75);++block){
            const int n=block%2?256:64;
            audio.setSize(channels,n,false,false,true);
            if(block%17==0)pitch->setValueNotifyingHost(block%34?.4f:.65f);
            if(block==20)freeze->setValueNotifyingHost(1);
            if(block==80)freeze->setValueNotifyingHost(0);
            if(block%29==0)mode->setValueNotifyingHost(block%58?0.f:1.f);
            for(int i=0;i<n;++i){const float x=float(.2*std::sin(.041*(position+i))+.1*std::cos(.113*(position+i)));
                audio.setSample(0,i,x);if(channels==2)audio.setSample(1,i,-.7f*x);}
            AllocationProbe probe;
            if(block%5==0)p.processBlockBypassed(audio,midi);else p.processBlock(audio,midi);
            const auto counts=probe.finish();allocs+=counts.allocations;frees+=counts.frees;++callbacks;
            for(int c=0;c<channels;++c)for(int i=0;i<n;++i){const double x=audio.getSample(c,i);require(std::isfinite(x),"allocation finite PCM");energy+=x*x;}
            position+=n;
        }
        AllocationProbe probe;p.reset();const auto resetCounts=probe.finish();
        allocs+=resetCounts.allocations;frees+=resetCounts.frees;
        std::cout<<rate<<" Hz / "<<channels<<" channels / editor "<<editorOpen<<": new "<<allocs<<", delete "<<frees<<", PCM energy "<<energy<<'\n';
        require(energy>1,"allocation test processes nonzero audio");
        require(allocs==0&&frees==0,"no intercepted C++ allocations or frees in processor callbacks/reset");
        editor.reset();p.releaseResources();
    }
    std::cout<<callbacks<<" processor callbacks with C++ allocation interposition PASS\n";return 0;
}


bool matchesSession(const juce::MemoryBlock& block,const cyberator::SessionState& expected){
    cyberator::SessionState s;
    if(!s.restore(std::span(static_cast<const std::uint8_t*>(block.getData()),block.getSize())))return false;
    if(std::abs(s.scale-expected.scale)>1e-6f)return false;
    for(std::size_t i=0;i<cyberator::Parameters::count;++i){
        const auto id=static_cast<cyberator::Param>(i);
        if(std::abs(s.parameters.get(id)-expected.parameters.get(id))>1e-5f*(cyberator::specs[i].high-cyberator::specs[i].low))return false;
    }
    return true;
}
int stateTransactionCases(juce::AudioProcessor& p){
    using namespace cyberator;
    SessionState a,b;a.scale=.7f;b.scale=1.3f;
    // Keep an exact dry PCM oracle while every other parameter changes.
    for(std::size_t i=0;i<specs.size();++i){
        const auto id=static_cast<Param>(i);
        require(a.parameters.set(id,specs[i].low)&&b.parameters.set(id,specs[i].high),"session fixtures");
    }
    require(a.parameters.set(Param::mix,0)&&b.parameters.set(Param::mix,0),"aligned dry oracle");
    require(a.parameters.set(Param::freeze,0)&&b.parameters.set(Param::freeze,0),"fresh source fixture");
    const auto aa=a.save(),bb=b.save();
    p.setStateInformation(aa.data(),int(aa.size()));
    struct Listener final:juce::AudioProcessorParameter::Listener{
        juce::AudioProcessor& p;const SessionState& a;const SessionState& b;unsigned calls{};
        Listener(juce::AudioProcessor& processor,const SessionState& first,const SessionState& second):p(processor),a(first),b(second){}
        void parameterValueChanged(int,float)override{
            juce::MemoryBlock saved;p.getStateInformation(saved);
            require(matchesSession(saved,a)||matchesSession(saved,b),"synchronous parameter listener save is a complete session");
            ++calls;
        }
        void parameterGestureChanged(int,bool)override{}
    } listener(p,a,b);
    auto* out=static_cast<juce::AudioProcessorParameter*>(nullptr);
    for(auto* param:p.getParameters())if(param->getName(128)=="output")out=param;
    require(out!=nullptr,"output listener fixture");out->addListener(&listener);
    p.setStateInformation(bb.data(),int(bb.size()));p.setStateInformation(aa.data(),int(aa.size()));
    require(listener.calls>=2,"reentrant host callback exercised");
    out->removeListener(&listener);
    p.setRateAndBufferSizeDetails(48000,128);p.prepareToPlay(48000,128);
    const int latency=p.getLatencySamples();
    std::atomic<bool> done{false};std::atomic<unsigned> callbacks{0},saves{0};
    std::thread audio([&]{
        juce::AudioBuffer<float> buffer(2,128);juce::MidiBuffer midi;int position=0;
        do{
            for(int i=0;i<128;++i){buffer.setSample(0,i,.125f);buffer.setSample(1,i,-.25f);}
            p.processBlock(buffer,midi);
            for(int i=0;i<128;++i){
                require(buffer.getSample(0,i)==(position+i>=latency?.125f:0),"concurrent restore dry PCM left");
                require(buffer.getSample(1,i)==(position+i>=latency?-.25f:0),"concurrent restore dry PCM right");
            }
            position+=128;++callbacks;
        }while(!done.load()||position<latency+256);
    });
    std::thread saver([&]{
        do{juce::MemoryBlock saved;p.getStateInformation(saved);
            require(matchesSession(saved,a)||matchesSession(saved,b),"parallel save never emits mixed scale/parameters");
            ++saves;
        }while(!done.load()||saves.load()<100);
    });
    while(!callbacks.load()||!saves.load())std::this_thread::yield();
    for(int i=0;i<200;++i){
        const auto& bytes=i%2?aa:bb;p.setStateInformation(bytes.data(),int(bytes.size()));
    }
    done.store(true);saver.join();audio.join();
    juce::MemoryBlock last;p.getStateInformation(last);require(matchesSession(last,a),"last complete restore");
    require(callbacks.load()>0&&saves.load()>=100,"concurrent paths executed");
    p.releaseResources();
    std::cout<<"Reentrant saves "<<listener.calls<<", concurrent saves "<<saves.load()<<", audio callbacks "<<callbacks.load()<<" PASS\n";
    return 0;
}

int main(int argc,char** argv){
    juce::ScopedJuceInitialiser_GUI init;
    std::unique_ptr<juce::AudioProcessor> p(createPluginFilter());
    require(p!=nullptr,"factory");
    if(argc==2&&std::string_view(argv[1])=="state-transaction")return stateTransactionCases(*p);
    if(argc==2&&std::string_view(argv[1])=="allocation")return allocationCases(*p);
    require(p->getNumPrograms()==1&&p->getProgramName(0)=="Current Settings","named current host program");
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
    require(original.getSize()==cyberator::SessionState::bytes,"native state size");
    cyberator::SessionState initial;
    require(initial.restore(std::span(static_cast<const std::uint8_t*>(original.getData()),original.getSize())),"native state decodes");
    require(p->getTailLengthSeconds()==30,"default finite tail");
    for(float feedback:{.1f,.3f,1.f}){
        auto v=initial.parameters;require(v.set(cyberator::Param::feedback,feedback),"tail feedback fixture");
        auto bytes=v.save();p->setStateInformation(bytes.data(),int(bytes.size()));
        const auto tail=p->getTailLengthSeconds(),expected=cyberator::tailSeconds(false,feedback);
        require(tail==expected,"actual processor feedback tail follows restored state");
    }
    auto frozen=initial.parameters;require(frozen.set(cyberator::Param::freeze,1),"freeze tail fixture");
    auto frozenBytes=frozen.save();p->setStateInformation(frozenBytes.data(),int(frozenBytes.size()));
    require(std::isinf(p->getTailLengthSeconds()),"frozen capture infinite tail");
    p->setStateInformation(original.getData(),int(original.getSize()));
    require(p->getTailLengthSeconds()==30,"default tail restored");
    auto values=initial.parameters;
    require(values.set(cyberator::Param::mix,0)&&values.set(cyberator::Param::ghost,1)&&values.set(cyberator::Param::output,12),"fixture");
    auto expected=values.save();
    p->setStateInformation(expected.data(),int(expected.size()));
    juce::MemoryBlock recalled;
    p->getStateInformation(recalled);
    require(recalled.getSize()==cyberator::SessionState::bytes&&std::memcmp(recalled.getData(),expected.data(),expected.size())==0,"native legacy recall");
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
        require(latency==10240,"reported latency");
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
