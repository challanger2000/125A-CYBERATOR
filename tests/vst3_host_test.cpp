#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <fstream>
#include <iomanip>
#include <cstdint>
#include <barrier>
#include "support/PacedTiming.hpp"
#include "support/RenderWitness.hpp"
#include "support/GridWitness.hpp"
#include "control/FactoryBank.hpp"

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
int runRealtime(Create create,bool parallel=false,bool local=false,bool full=false){
    const std::size_t groups=local?2048:512;
    const double warmSeconds=local?4.:1.25;
    using cyberator::test::TimingSample;
    using cyberator::test::pace;
    using cyberator::test::summarize;
    const std::string prefix=parallel?"CYBERATOR-rt-parallel-":"CYBERATOR-rt-";
    std::ofstream csv(prefix+"groups.csv"),summary(prefix+"summary.csv");
    std::ofstream tracks;if(parallel)tracks.open(prefix+"tracks.csv");
    if(parallel){require(bool(tracks),"parallel track evidence");tracks<<"rate,block,active,track,group,process_us,start_late_us,finish_late_us\n";tracks<<std::setprecision(10);}
    std::cout<<"Host reported logical processors "<<std::thread::hardware_concurrency()<<", parallel diagnostic "<<parallel<<", groups per case "<<groups<<", warmup seconds "<<warmSeconds<<'\n';
    require(bool(csv)&&bool(summary),"RT evidence files");
    csv<<"rate,block,instances,active,group,process_us,start_late_us,finish_late_us\n";
    summary<<"rate,block,instances,active,groups,budget_us,p50_us,p95_us,p99_us,max_us,duration_misses,completion_misses,max_start_late_us,max_finish_late_us,delta_energy,status\n";
    csv<<std::setprecision(10);summary<<std::setprecision(10);
    bool anyMiss=false;
    for(double sr:{44100.,48000.,96000.,192000.})for(int block:{64,256})for(int n:{1,4})for(bool active:{false,true}){
        if((!local&&sr==44100.)||(local&&!full&&sr>48000.))continue;
        if(parallel&&n!=4)continue;
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
        if(!parallel){
        std::thread worker([&]{
            const auto warm=static_cast<std::size_t>(std::ceil(warmSeconds*sr/block));
            for(std::size_t i=0;i<warm;++i){setup(i);process(i);position+=block;}
            rows=pace(sr,block,groups,setup,process,observe);
        });
        worker.join();
        }else{
            struct Track {std::int64_t position{};double energy{},delta{},maxDryError{};std::vector<TimingSample> rows;};
            std::vector<Track> perTrack(static_cast<std::size_t>(n));
            using Clock=std::chrono::steady_clock;
            Clock::time_point release;
            std::barrier ready(static_cast<std::ptrdiff_t>(n),[&]()noexcept{release=Clock::now()+std::chrono::milliseconds(20);});
            std::vector<std::thread> workers;workers.reserve(static_cast<std::size_t>(n));
            const auto warm=static_cast<std::size_t>(std::ceil(warmSeconds*sr/block));
            for(int k=0;k<n;++k)workers.emplace_back([&,k]{
                const auto idx=static_cast<std::size_t>(k);auto& track=perTrack[idx];
                auto fill=[&](std::size_t){for(int i=0;i<block;++i){const float x=source(track.position+i,k);buffers[idx].setSample(0,i,x);buffers[idx].setSample(1,i,x*.7f);}};
                auto run=[&](std::size_t){plugins[idx]->processBlock(buffers[idx],midi[idx]);};
                const auto latency=plugins[idx]->getLatencySamples();
                auto check=[&](std::size_t){
                    for(int i=0;i<block;++i){
                        const float x=source(track.position+i-latency,k);
                        const double l=buffers[idx].getSample(0,i),r=buffers[idx].getSample(1,i);
                        require(std::isfinite(l)&&std::isfinite(r),"parallel RT finite audio");
                        const double dl=l-x,dr=r-x*.7f;
                        track.energy+=l*l+r*r;track.delta+=dl*dl+dr*dr;track.maxDryError=std::max({track.maxDryError,std::abs(dl),std::abs(dr)});
                    }
                    track.position+=block;
                };
                for(std::size_t i=0;i<warm;++i){fill(i);run(i);track.position+=block;}
                ready.arrive_and_wait();
                track.rows=pace(sr,block,groups,fill,run,check,release);
            });
            for(auto& thread:workers)thread.join();
            rows.resize(groups);
            for(std::size_t k=0;k<perTrack.size();++k){
                const auto& track=perTrack[k];
                require(track.rows.size()==rows.size()&&track.energy>1,"parallel per-track output and callback count");
                require(active?track.delta>1:track.maxDryError<1e-6,"parallel per-track PCM witness");
                energy+=track.energy;delta+=track.delta;maxDryError=std::max(maxDryError,track.maxDryError);
                for(std::size_t i=0;i<rows.size();++i){
                    const auto& r=track.rows[i];auto& combined=rows[i];
                    if(k==0)combined=r;
                    else{combined.processUs=std::max(combined.processUs,r.processUs);combined.startLateUs=std::max(combined.startLateUs,r.startLateUs);combined.finishLateUs=std::max(combined.finishLateUs,r.finishLateUs);}
                    tracks<<sr<<','<<block<<','<<active<<','<<k<<','<<i<<','<<r.processUs<<','<<r.startLateUs<<','<<r.finishLateUs<<'\n';
                }
            }
            tracks.flush();require(bool(tracks),"parallel raw evidence write");
        }
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
    std::cout<<(parallel?"Parallel independent-track diagnostic: ":"Paced serial Windows VST3 diagnostic: ")<<(anyMiss?"RT_FAIL":"RT_PASS_IN_TESTED_MATRIX")<<"; physical ASIO NOT RUN\n";
    return anyMiss?3:0;
}


// Native host parameter and opaque project-state tests; no direct access to product processor.
// The bank is an expected-value manifest only; every render passes through the actual VST3.
namespace productCases {
using namespace cyberator;
using cyberator::test::compare;
using cyberator::test::audibleChange;
using cyberator::test::recallMatches;
constexpr int rate=48000,frames=96000;
struct Controls {
    std::array<juce::AudioProcessorParameter*,Parameters::count> p{};
    explicit Controls(juce::AudioPluginInstance& plugin){
        for(std::size_t i=0;i<p.size();++i){
            p[i]=find(plugin,juce::String::fromUTF8(specs[i].id.data(),int(specs[i].id.size())));
            require(p[i]!=nullptr,"every host parameter available");
        }
    }
    void set(Param id,float physical)const{
        const auto i=std::size_t(id);const auto& s=specs[i];
        const float normalized=(physical-s.low)/(s.high-s.low);
        p[i]->beginChangeGesture();p[i]->setValueNotifyingHost(normalized);p[i]->endChangeGesture();
        require(std::abs(p[i]->getValue()-normalized)<1e-4f,"host value accepted");
    }
    void apply(const Parameters& values)const{for(std::size_t i=0;i<p.size();++i)set(static_cast<Param>(i),values.get(static_cast<Param>(i)));}
    void verify(const Parameters& values)const{
        for(std::size_t i=0;i<p.size();++i){
            const auto& s=specs[i];const auto expected=(values.get(static_cast<Param>(i))-s.low)/(s.high-s.low);
            require(std::abs(p[i]->getValue()-expected)<1e-4f,"restored host value");
        }
    }
};
std::vector<float> fixture(){
    std::vector<float> src(2*frames);
    for(int i=0;i<rate;++i){
        const double t=double(i)/rate,env=.15+.85*std::pow(std::sin(2*std::numbers::pi*3.1*t),2);
        double x=0,y=0;
        for(int k=1;k<=12;++k){const auto phase=2*std::numbers::pi*(137.3*k)*t+std::sin(2*std::numbers::pi*4.3*t);x+=.09/k*std::sin(phase);y+=.07/k*std::sin(phase+.17*k);}
        src[2*std::size_t(i)]=float(env*x);src[2*std::size_t(i)+1]=float(env*y);
    }
    return src;
}
std::vector<float> aligned(const std::vector<float>& src,int latency){
    require(latency>=0&&2*std::size_t(latency)<src.size(),"valid measured latency");
    std::vector<float> out(src.size());std::copy_n(src.begin(),src.size()-2*std::size_t(latency),out.begin()+2*std::size_t(latency));return out;
}
template<class Edit>
std::vector<float> render(juce::AudioPluginInstance& p,const std::vector<float>& source,int partition,bool offline,Edit edit){
    p.setNonRealtime(offline);p.setRateAndBufferSizeDetails(rate,512);p.prepareToPlay(rate,512);
    juce::MidiBuffer midi;std::vector<float> output(source.size());juce::AudioBuffer<float> audio(2,512);
    for(int offset=0;offset<int(source.size()/2);){
        // Keep the 0.5s control boundary exact for every block partition.
        const int boundary=offset<24000?24000:int(source.size()/2);
        const int n=std::min({partition,boundary-offset,int(source.size()/2)-offset});
        audio.setSize(2,n,false,false,true);
        edit(offset);
        for(int i=0;i<n;++i){audio.setSample(0,i,source[2*std::size_t(offset+i)]);audio.setSample(1,i,source[2*std::size_t(offset+i)+1]);}
        p.processBlock(audio,midi);
        for(int i=0;i<n;++i){output[2*std::size_t(offset+i)]=audio.getSample(0,i);output[2*std::size_t(offset+i)+1]=audio.getSample(1,i);}
        offset+=n;
    }
    p.releaseResources();return output;
}
template<class Create>
int run(Create create){
    auto a=create(rate,512),b=create(rate,512);
    Controls ca(*a),cb(*b);const auto src=fixture();
    auto noop=[](int){};
    ca.apply(Parameters{});const auto neutral=render(*a,src,127,false,noop);
    // Independently calculated latency from fixed 48k studio design: FFT 2048+512, swell 7680.
    const auto dry=aligned(src,10240);require(compare(neutral,dry).maxError==0,"actual loaded module exact dry oracle");
    std::ofstream csv("CYBERATOR-product-cases.csv");require(bool(csv),"case report");csv<<std::setprecision(10);
    csv<<"kind,index,name,peak,energy,shape,recall_max_error,recall_relative_error,status\n";
    std::vector<std::vector<float>> prior;
    for(std::size_t i=0;i<factoryBank.size();++i){
        const auto& preset=factoryBank[i];const auto values=preset.parameters();
        ca.apply(values);juce::MemoryBlock state;a->getStateInformation(state);require(state.getSize()>0,"nonempty actual host state");
        const auto x=render(*a,src,127,false,noop);
        b->setStateInformation(state.getData(),int(state.getSize()));cb.verify(values);
        const auto y=render(*b,src,512,true,noop);
        const auto effect=compare(x,dry),recall=compare(y,x);
        const bool pass=audibleChange(effect)&&effect.maxPeak<1&&recallMatches(recall);
        csv<<"preset,"<<i<<','<<preset.name<<','<<effect.maxPeak<<','<<effect.energy<<','<<effect.shapeError<<','<<recall.maxError<<','<<recall.relativeError<<','<<(pass?"PASS":"FAIL")<<'\n';csv.flush();
        if(!pass)std::cerr<<"Preset "<<preset.name<<": shape "<<effect.shapeError<<", peak "<<effect.maxPeak<<", recall "<<recall.maxError<<" / "<<recall.relativeError<<'\n';
        require(pass,"preset actual PCM, gain-independent effect, state recall, partition and offline equivalence");
        // Compare every preset to all earlier presets without retaining 120 full two-second stereo renders.
        std::vector<float> sampled;sampled.reserve(x.size()/16);
        for(std::size_t j=0;j<x.size();j+=32){sampled.push_back(x[j]);sampled.push_back(x[j+1]);}
        for(const auto& old:prior)require(audibleChange(compare(sampled,old)),"level-independent preset distinction");
        prior.push_back(std::move(sampled));
        if(i%20==19)std::cout<<"Actual VST3 presets "<<i+1<<" / "<<factoryBank.size()<<'\n';
    }
    // Context for every individual control. No global preset change between A and B.
    Parameters base;
    auto set=[&](Param id,float x){require(base.set(id,x),"base fixture");};
    set(Param::granular,.7f);set(Param::size,35);set(Param::density,.7f);set(Param::position,.2f);set(Param::scatter,.2f);set(Param::pitch,3);set(Param::reverse,.25f);set(Param::feedback,.3f);set(Param::spread,.3f);
    set(Param::formant,3);set(Param::android,.3f);set(Param::spectral,.3f);set(Param::harmony,.3f);set(Param::ghost,.3f);set(Param::swell,.3f);set(Param::delay,.3f);set(Param::space,.3f);set(Param::motion,.2f);set(Param::mix,.7f);set(Param::output,-6);
    ca.apply(base);const auto baseline=render(*a,src,127,false,noop);
    for(std::size_t i=0;i<specs.size();++i){
        const auto id=static_cast<Param>(i);const auto& s=specs[i];
        const float target=id==Param::mode?9.f:s.high;
        ca.apply(base);
        const auto x=render(*a,src,127,false,[&](int offset){if(offset==24000)ca.set(id,target);});
        const auto m=compare(x,baseline);
        // Output is intentionally a gain control; other controls may make a small
        // change in this combined context, so use normalized waveform error here.
        const bool pass=m.energy>1e-6&&m.maxError>1e-5&&m.relativeError>1e-4;
        csv<<"control,"<<i<<','<<s.id<<','<<m.maxPeak<<','<<m.energy<<','<<m.shapeError<<','<<m.maxError<<','<<m.relativeError<<','<<(pass?"PASS":"FAIL")<<'\n';csv.flush();
        if(!pass)std::cerr<<"Control "<<s.id<<" delta "<<m.maxError<<" / "<<m.relativeError<<'\n';
        require(pass,"each host parameter changes actual processed PCM in active context");
    }
    std::cout<<"120 actual VST3 preset renders and recalls, 23 individual control changes PASS\n";
    return 0;
}
} // namespace productCases


namespace transportCases {
struct Clock final:juce::AudioPlayHead{
    PositionInfo position;
    juce::Optional<PositionInfo> getPosition()const override{return position;}
};
template<class Create>
int run(Create create){
    using namespace cyberator;
    for(double rate:{48000.,96000.})for(double bpm:{120.,137.})for(double origin:{-.371,.031}){
        Clock clock;auto p=create(rate,256);p->setPlayHead(&clock);
        productCases::Controls controls(*p);Parameters initial;
        require(initial.set(Param::size,10)&&initial.set(Param::sync,1),"transport fixture");
        controls.apply(initial);
        p->setRateAndBufferSizeDetails(rate,256);p->prepareToPlay(rate,256);
        const int latency=p->getLatencySamples();
        require(latency==(rate==48000?10240:20480),"actual transport latency");
        juce::MidiBuffer midi;juce::AudioBuffer<float> audio(2,256);
        auto position=[&](int sample,bool playing,double ppq){
            clock.position.setTimeInSamples(static_cast<int64_t>(sample));
            clock.position.setTimeInSeconds(double(sample)/rate);
            clock.position.setBpm(bpm);clock.position.setPpqPosition(ppq);clock.position.setIsPlaying(playing);
        };
        // Fill a constant stereo source with granular amount zero: no prior grains.
        const int prefill=int(rate*.5);
        for(int offset=0;offset<prefill;){
            const int n=std::min(113,prefill-offset);audio.setSize(2,n,false,false,true);
            position(offset-prefill,false,origin);
            for(int i=0;i<n;++i){audio.setSample(0,i,.25f);audio.setSample(1,i,-.125f);}
            p->processBlock(audio,midi);offset+=n;
        }
        require(std::abs(p->getTailLengthSeconds()-30)<1e-4,"initial actual VST3 tail");
        controls.set(Param::freeze,1);controls.set(Param::granular,1);
        const int total=int(rate),grainLength=int(std::round(.010*rate));
        const auto expected=test::frozenGrid(rate,bpm,origin,total,grainLength,.25f,int(.010*rate));
        std::vector<float> left(total),right(total);
        double energy=0;const int partition=origin<0?113:256;
        for(int offset=0;offset<total+latency;){
            const int n=std::min(partition,total+latency-offset);
            audio.setSize(2,n,false,false,true);audio.clear();
            position(offset,true,origin+double(offset)*bpm/(60*rate));
            p->processBlock(audio,midi);
            if(offset==0)require(std::isinf(p->getTailLengthSeconds()),"actual VST3 freeze infinite tail");
            for(int i=0;i<n;++i){
                const int at=offset+i-latency;if(at<0||at>=total)continue;
                left[std::size_t(at)]=audio.getSample(0,i);right[std::size_t(at)]=audio.getSample(1,i);
                energy+=double(left[std::size_t(at)])*left[std::size_t(at)];
            }
            offset+=n;
        }
        double maxError=0,stereoError=0,wrongClockError=0;
        const auto wrong=test::frozenGrid(rate,bpm,origin+.03125,total,grainLength,.25f,int(.010*rate));
        for(std::size_t i=0;i<left.size();++i){
            maxError=std::max(maxError,std::abs(double(left[i])-expected[i]));
            stereoError=std::max(stereoError,std::abs(double(right[i])+.5*left[i]));
            wrongClockError=std::max(wrongClockError,std::abs(double(left[i])-wrong[i]));
        }
        std::cout<<rate<<" Hz / "<<bpm<<" BPM / PPQ "<<origin<<": PCM max error "<<maxError<<", stereo "<<stereoError<<", deliberately shifted clock error "<<wrongClockError<<'\n';
        require(energy>1&&maxError<2e-6&&stereoError<2e-6,"actual loaded VST3 aligns frozen grains to host PPQ");
        require(wrongClockError>.05,"actual render rejects deliberately shifted timing oracle");
        controls.set(Param::freeze,0);controls.set(Param::feedback,.1f);
        audio.clear();position(total+latency,false,origin);p->processBlock(audio,midi);
        require(std::abs(p->getTailLengthSeconds()-65.75)<1e-3,"actual VST3 finite feedback tail");
        controls.set(Param::feedback,1);audio.clear();p->processBlock(audio,midi);
        require(std::isinf(p->getTailLengthSeconds()),"actual VST3 conservative feedback tail");
        p->setPlayHead(nullptr);p->releaseResources();
    }
    std::cout<<"Actual VST3 transport grid PCM and dynamic tail queries PASS\n";return 0;
}
}

#if defined(CYBERATOR_ASIO_PROBE)
#include "support/AsioProbe.hpp"
#endif

int main(int argc,char** argv) {
    require(argc==2||argc==3,"module path argument");
    juce::ScopedJuceInitialiser_GUI init;
    const auto module=juce::File::getCurrentWorkingDirectory().getChildFile(juce::String::fromUTF8(argv[1]));
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
#if defined(CYBERATOR_ASIO_PROBE)
    if(argc==3&&std::string_view(argv[2])=="asio")return asioProbe::run(create,false);
    if(argc==3&&std::string_view(argv[2])=="asio-selftest")return asioProbe::run(create,true);
#endif
    if(argc==3&&std::string_view(argv[2])=="transport")return transportCases::run(create);
    if(argc==3&&std::string_view(argv[2])=="product")return productCases::run(create);
    if(argc==3&&std::string_view(argv[2])=="rt")return runRealtime(create);
    if(argc==3&&std::string_view(argv[2])=="rt-parallel")return runRealtime(create,true);
    if(argc==3&&std::string_view(argv[2])=="rt-local")return runRealtime(create,false,true,false);
    if(argc==3&&std::string_view(argv[2])=="rt-local-parallel")return runRealtime(create,true,true,false);
    if(argc==3&&std::string_view(argv[2])=="rt-local-full")return runRealtime(create,false,true,true);
    if(argc==3&&std::string_view(argv[2])=="rt-local-full-parallel")return runRealtime(create,true,true,true);
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
