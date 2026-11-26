#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <fstream>
#include <iomanip>
#include <cstdint>
#include "support/PacedTiming.hpp"

void require(bool ok,const char* message) {
    if(!ok){std::cerr<<message<<'\n';std::exit(1);}
}
juce::AudioProcessorParameter* find(juce::AudioPluginInstance& p,const juce::String& name) {
    for(auto* param:p.getParameters())if(param->getName(128).equalsIgnoreCase(name))return param;
    return nullptr;
}
void prepare(juce::AudioPluginInstance& p) {
    require(p.getTotalNumInputChannels()==2&&p.getTotalNumOutputChannels()==2,"stereo VST3 layout");
    p.setRateAndBufferSizeDetails(48000,127);p.prepareToPlay(48000,127);
    require(p.getLatencySamples()==10240,"VST3 reported latency");
}
void dryImpulse(juce::AudioPluginInstance& p) {
    juce::MidiBuffer midi;
    const int latency=p.getLatencySamples();
    for(int offset=0;offset<latency+255;) {
        const int n=std::min(127,latency+255-offset);
        juce::AudioBuffer<float> audio(2,n);audio.clear();
        if(offset==0){audio.setSample(0,0,.5f);audio.setSample(1,0,-.25f);}
        p.processBlock(audio,midi);
        for(int i=0;i<n;++i){
            require(audio.getSample(0,i)==(offset+i==latency?.5f:0),"VST3 dry impulse left");
            require(audio.getSample(1,i)==(offset+i==latency?-.25f:0),"VST3 dry impulse right");
        }
        offset+=n;
    }
}
void steadyGain(juce::AudioPluginInstance& p,double expected) {
    juce::MidiBuffer midi;
    for(int block=0;block<100;++block) {
        juce::AudioBuffer<float> audio(2,127);
        for(int i=0;i<127;++i){audio.setSample(0,i,.125f);audio.setSample(1,i,-.25f);}
        p.processBlock(audio,midi);
        if(block>=95)for(int i=0;i<127;++i){
            require(std::abs(audio.getSample(0,i)-.125*expected)<2e-5,"VST3 output gain left");
            require(std::abs(audio.getSample(1,i)+.25*expected)<2e-5,"VST3 output gain right");
        }
    }
}

template<class Create>
int runRealtime(Create create){
    using cyberator::test::TimingSample;
    using cyberator::test::pace;
    using cyberator::test::summarize;
    std::ofstream csv("CYBERATOR-rt-groups.csv"),summary("CYBERATOR-rt-summary.csv");
    require(bool(csv)&&bool(summary),"RT evidence files");
    csv<<"rate,block,instances,active,group,process_us,start_late_us,finish_late_us\n";
    summary<<"rate,block,instances,active,groups,budget_us,p50_us,p95_us,p99_us,max_us,duration_misses,completion_misses,max_start_late_us,max_finish_late_us,delta_energy,status\n";
    csv<<std::setprecision(10);summary<<std::setprecision(10);
    bool anyMiss=false;
    for(double sr:{48000.,96000.,192000.})for(int block:{64,256})for(int n:{1,4})for(bool active:{false,true}){
        std::vector<std::unique_ptr<juce::AudioPluginInstance>> plugins;
        std::vector<juce::AudioBuffer<float>> buffers;
        std::vector<juce::MidiBuffer> midi(static_cast<std::size_t>(n));
        plugins.reserve(static_cast<std::size_t>(n));buffers.reserve(static_cast<std::size_t>(n));
        constexpr std::pair<const char*,float> settings[]={
            {"granular",1},{"size",1},{"density",1},{"position",.15f},{"scatter",.4f},
            {"pitch",.75f},{"reverse",.5f},{"feedback",.5f},{"spread",.8f},
            {"formant",.75f},{"android",.8f},{"spectral",.8f},{"harmony",.8f},
            {"ghost",.8f},{"swell",.8f},{"delay",.8f},{"space",.8f},{"motion",.7f},
            {"mix",1},{"output",.5f}
        };
        for(int k=0;k<n;++k){
            auto p=create(sr,block);
            require(p->getTotalNumInputChannels()==2&&p->getTotalNumOutputChannels()==2,"RT stereo buses");
            p->setRateAndBufferSizeDetails(sr,block);p->prepareToPlay(sr,block);
            require(p->getLatencySamples()>0,"RT reported latency");
            if(active)for(const auto& [name,value]:settings){
                auto* param=find(*p,name);require(param!=nullptr,"RT effect parameter missing");
                param->setValueNotifyingHost(value);
                require(std::abs(param->getValue()-value)<1e-4f,"RT active value accepted");
            }
            plugins.push_back(std::move(p));buffers.emplace_back(2,block);
        }
        std::int64_t position=0;
        double delta=0,energy=0,maxDryError=0;
        std::vector<TimingSample> rows;
        auto source=[sr](std::int64_t i,int k){
            if(i<0)return 0.f;
            const double t=double(i)/sr;
            return float((.22*std::sin(1031*t)+.12*std::cos(2371*t))*(1-.05*k));
        };
        auto setup=[&](std::size_t){
            for(int k=0;k<n;++k)for(int i=0;i<block;++i){
                const float x=source(position+i,k);
                buffers[static_cast<std::size_t>(k)].setSample(0,i,x);
                buffers[static_cast<std::size_t>(k)].setSample(1,i,x*.7f);
            }
        };
        auto process=[&](std::size_t){
            for(int k=0;k<n;++k)plugins[static_cast<std::size_t>(k)]->processBlock(buffers[static_cast<std::size_t>(k)],midi[static_cast<std::size_t>(k)]);
        };
        auto observe=[&](std::size_t){
            for(int k=0;k<n;++k)for(int i=0;i<block;++i){
                const auto idx=static_cast<std::size_t>(k);
                const float x=source(position+i-plugins[idx]->getLatencySamples(),k);
                const double l=buffers[idx].getSample(0,i),r=buffers[idx].getSample(1,i);
                require(std::isfinite(l)&&std::isfinite(r),"RT finite audio");
                energy+=l*l+r*r;
                const double dl=l-x,dr=r-x*.7f;
                delta+=dl*dl+dr*dr;maxDryError=std::max({maxDryError,std::abs(dl),std::abs(dr)});
            }
            position+=block;
        };
        std::thread worker([&]{
            const auto warm=static_cast<std::size_t>(std::ceil(1.25*sr/block));
            for(std::size_t i=0;i<warm;++i){setup(i);process(i);position+=block;}
            rows=pace(sr,block,512,setup,process,observe);
        });
        worker.join();
        require(energy>1,"RT non-silent real module output");
        require(active?delta>1:maxDryError<1e-6,"RT active/neutral PCM witness");
        const double budget=1e6*block/sr;const auto s=summarize(rows,budget);
        const bool pass=s.durationMisses==0&&s.completionMisses==0;anyMiss|=!pass;
        for(std::size_t i=0;i<rows.size();++i)csv<<sr<<','<<block<<','<<n<<','<<active<<','<<i<<','<<rows[i].processUs<<','<<rows[i].startLateUs<<','<<rows[i].finishLateUs<<'\n';
        summary<<sr<<','<<block<<','<<n<<','<<active<<','<<rows.size()<<','<<budget<<','<<s.p50<<','<<s.p95<<','<<s.p99<<','<<s.maximum<<','<<s.durationMisses<<','<<s.completionMisses<<','<<s.maxStartLate<<','<<s.maxFinishLate<<','<<delta<<','<<(pass?"PASS":"FAIL")<<'\n';
        summary.flush();csv.flush();require(bool(summary)&&bool(csv),"RT evidence write");
        std::cout<<sr<<" Hz / "<<block<<" / "<<n<<" instances / active "<<active<<": p99 "<<s.p99<<" us, duration misses "<<s.durationMisses<<", completion misses "<<s.completionMisses<<'\n';
        for(auto& p:plugins)p->releaseResources();
    }
    std::cout<<"Paced Windows VM VST3 diagnostic: "<<(anyMiss?"RT_FAIL":"RT_PASS_IN_TESTED_MATRIX")<<"; physical ASIO NOT RUN\n";
    return anyMiss?3:0;
}

int main(int argc,char** argv) {
    require(argc==2||argc==3,"module path argument");
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File module(juce::String::fromUTF8(argv[1]));
    require(module.existsAsFile(),"exact built module exists");
    const auto bundle=module.getParentDirectory().getParentDirectory().getParentDirectory();
    require(bundle.isDirectory()&&bundle.hasFileExtension("vst3"),"VST3 bundle");
    juce::VST3PluginFormat format;
    juce::OwnedArray<juce::PluginDescription> types;
    format.findAllTypesForFile(types,bundle.getFullPathName());
    require(types.size()==1,"single scanned VST3 class");
    auto create=[&](double rate=48000,int frames=127){
        juce::String error;
        auto p=format.createInstanceFromDescription(*types[0],rate,frames,error);
        if(!p)std::cerr<<error.toStdString()<<'\n';
        require(p!=nullptr,"load actual VST3");
        return p;
    };
    if(argc==3&&std::string_view(argv[2])=="rt")return runRealtime(create);
    if(argc==3){
        require(std::string_view(argv[2])=="editor","editor mode argument");
        auto p=create();prepare(*p);
        require(p->hasEditor(),"VST3 advertises editor");
        juce::MemoryBlock before;p->getStateInformation(before);
        for(int repeat=0;repeat<2;++repeat){
            auto destroy=[&](juce::AudioProcessorEditor* editor){if(editor){editor->setVisible(false);editor->removeFromDesktop();p->editorBeingDeleted(editor);delete editor;}};
            std::unique_ptr<juce::AudioProcessorEditor,decltype(destroy)> ed(p->createEditorAndMakeActive(),destroy);
            require(ed&&ed->getWidth()>0&&ed->getHeight()>0,"VST3 creates actual editor");
            ed->addToDesktop(juce::ComponentPeer::windowHasTitleBar);
            ed->setVisible(true);
            require(ed->getPeer()!=nullptr,"native parent peer created");
            // Wrapper attaches IPlugView when showing; no claim of physical mouse/DPI verification.
            auto img=ed->createComponentSnapshot(ed->getLocalBounds(),true);
            require(img.isValid(),"VST3 editor snapshot allocation");
        }
        require(p->getActiveEditor()==nullptr,"VST3 host unregisters editor");
        dryImpulse(*p);
        p->releaseResources();
        std::cout<<"Actual VST3 editor create/peer/reopen and neutral audio smoke PASS\n";return 0;
    }
    auto a=create(),b=create();prepare(*a);prepare(*b);
    dryImpulse(*a);dryImpulse(*b);
    auto* output=find(*a,"output");require(output!=nullptr,"host output parameter");
    output->setValueNotifyingHost(30.f/36.f); // +6 dB in physical [-24,+12] range
    const double gain=std::pow(10.,6./20.);
    steadyGain(*a,gain);steadyGain(*b,1);
    juce::MemoryBlock saved;
    a->getStateInformation(saved);require(saved.getSize()>0,"VST3 state exported");
    b->releaseResources();b.reset();b=create();
    b->setStateInformation(saved.getData(),int(saved.getSize()));prepare(*b);
    steadyGain(*b,gain);
    auto* mix=find(*b,"mix");require(mix!=nullptr,"host mix parameter");
    mix->setValueNotifyingHost(0);
    steadyGain(*b,1);steadyGain(*a,gain);
    a->releaseResources();b->releaseResources();
    std::cout<<"Actual VST3 load, audio, automation and recall smoke PASS\n";
}
