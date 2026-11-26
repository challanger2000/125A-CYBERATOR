#pragma once
// Private engineering executable only. No ASIO code linked into the product.
#include <juce_audio_devices/juce_audio_devices.h>
#include <atomic>
#include <array>
#include <limits>
#include <windows.h>
namespace asioProbe {
using Clock=std::chrono::steady_clock;
using Us=std::chrono::duration<double,std::micro>;
struct Row {
    double wall{},plugin{},interval{},energy{},delta{},dryError{};
    unsigned long long cycles{};
    bool cyclesValid{},finite{true};
};
class Case final : public juce::AudioIODeviceCallback {
public:
    const int rate,block,count;const bool active;
    const std::size_t warm,groups;
    std::vector<Row> rows;
    std::atomic<std::size_t> done{0};
    std::atomic<unsigned> starts{0},stops{0},errors{0};
    std::atomic<bool> badBlock{false};
    std::atomic_flag busy=ATOMIC_FLAG_INIT;
    std::atomic<unsigned> reentrant{0};
    std::vector<std::unique_ptr<juce::AudioPluginInstance>> plugins;
    std::vector<juce::AudioBuffer<float>> buffers;
    std::vector<juce::MidiBuffer> midi;
    std::vector<float> signal;
    std::vector<int> latencies;
    Clock::time_point previous{};
    template<class Create>
    Case(Create create,int sr,int n,int instances,bool enabled,bool smoke)
      :rate(sr),block(n),count(instances),active(enabled),
       warm(std::size_t((2*sr+n-1)/n)),groups(std::size_t(((smoke?1:10)*sr+n-1)/n)),
       rows(warm+groups),midi(std::size_t(instances)),signal((warm+groups)*std::size_t(n)){
        for(std::size_t i=0;i<signal.size();++i){
            const auto t=double(i)/rate;
            signal[i]=float(.22*std::sin(1031*t)+.12*std::cos(2371*t));
        }
        constexpr std::pair<const char*,float> values[]={
            {"granular",1.f},{"size",1.f},{"density",1.f},{"position",.15f},{"scatter",.4f},
            {"pitch",.75f},{"reverse",.5f},{"feedback",.5f},{"spread",.8f},
            {"formant",.75f},{"android",.8f},{"spectral",.8f},{"harmony",.8f},
            {"ghost",.8f},{"swell",.8f},{"delay",.8f},{"space",.8f},{"motion",.7f},
            {"mix",1.f},{"output",.5f}};
        for(int k=0;k<count;++k){
            auto p=create(sr,n);
            require(p->getTotalNumInputChannels()==2&&p->getTotalNumOutputChannels()==2,"ASIO stereo plugin");
            p->setRateAndBufferSizeDetails(sr,n);p->prepareToPlay(sr,n);
            if(active)for(auto [name,value]:values){
                auto* param=find(*p,name);require(param!=nullptr,"ASIO parameter");
                param->setValueNotifyingHost(value);
                require(std::abs(param->getValue()-value)<1e-4,"ASIO parameter accepted");
            }
            latencies.push_back(p->getLatencySamples());require(latencies.back()>0,"ASIO latency");
            plugins.push_back(std::move(p));buffers.emplace_back(2,n);
        }
    }
    ~Case()override{for(auto& p:plugins)p->releaseResources();}
    void audioDeviceAboutToStart(juce::AudioIODevice* device)override{
        ++starts;
        if(device&&(device->getCurrentBufferSizeSamples()!=block||std::abs(device->getCurrentSampleRate()-rate)>.01))badBlock.store(true);
    }
    void audioDeviceStopped()override{++stops;}
    void audioDeviceError(const juce::String&)override{++errors;}
    void audioDeviceIOCallbackWithContext(const float* const*,int,float* const* output,int outputs,int samples,
                                          const juce::AudioIODeviceCallbackContext&)override{
        const auto start=Clock::now();
        for(int c=0;c<outputs;++c)if(output[c])std::fill_n(output[c],samples,0.f);
        if(busy.test_and_set(std::memory_order_acquire)){++reentrant;return;}
        struct Unlock{std::atomic_flag& flag;~Unlock(){flag.clear(std::memory_order_release);}} unlock{busy};
        if(samples!=block||outputs!=2){badBlock.store(true);return;}
        const auto g=done.load(std::memory_order_relaxed);
        if(g>=rows.size())return;
        Row row;
        row.interval=g?Us(start-previous).count():0.;previous=start;
        const auto pos=g*std::size_t(block);
        for(int k=0;k<count;++k){
            const auto scale=1.f-.05f*float(k);
            for(int i=0;i<block;++i){
                const auto x=signal[pos+std::size_t(i)]*scale;
                buffers[std::size_t(k)].setSample(0,i,x);buffers[std::size_t(k)].setSample(1,i,x*.7f);
            }
        }
        unsigned long long before=0,after=0;
        const bool got=QueryThreadCycleTime(GetCurrentThread(),&before)!=0;
        const auto begin=Clock::now();
        for(int k=0;k<count;++k)plugins[std::size_t(k)]->processBlock(buffers[std::size_t(k)],midi[std::size_t(k)]);
        const auto end=Clock::now();
        row.cyclesValid=got&&QueryThreadCycleTime(GetCurrentThread(),&after)!=0&&after>=before;
        if(row.cyclesValid)row.cycles=after-before;
        row.plugin=Us(end-begin).count();
        for(int k=0;k<count;++k){
            const auto scale=1.f-.05f*float(k);
            for(int i=0;i<block;++i){
                const auto at=static_cast<std::int64_t>(pos)+i-latencies[std::size_t(k)];
                const auto x=at<0?0.f:signal[std::size_t(at)]*scale;
                const auto l=double(buffers[std::size_t(k)].getSample(0,i)),r=double(buffers[std::size_t(k)].getSample(1,i));
                const auto dl=l-x,dr=r-x*.7f;
                row.finite=row.finite&&std::isfinite(l)&&std::isfinite(r);
                row.energy+=l*l+r*r;row.delta+=dl*dl+dr*dr;
                row.dryError=std::max({row.dryError,std::abs(dl),std::abs(dr)});
            }
        }
        row.wall=Us(Clock::now()-start).count();
        rows[g]=row;done.store(g+1,std::memory_order_release);
    }
    int report(std::ofstream& raw,std::ofstream& summary,int index,int xruns,bool smoke){
        const auto measured=done.load(std::memory_order_acquire);
        const double budget=1.e6*block/rate;
        std::size_t misses=0,pluginMisses=0,longIntervals=0,invalid=0,cyclesValid=0;
        double energy=0,delta=0,dryError=0;
        std::vector<double> wall,plugin;
        for(std::size_t i=warm;i<measured;++i){
            const auto& v=rows[i];misses+=v.wall>budget;pluginMisses+=v.plugin>budget;
            longIntervals+=v.interval>1.5*budget;invalid+=!v.finite;cyclesValid+=v.cyclesValid;
            energy+=v.energy;delta+=v.delta;dryError=std::max(dryError,v.dryError);
            wall.push_back(v.wall);plugin.push_back(v.plugin);
            raw<<index<<','<<i-warm<<','<<v.wall<<','<<v.plugin<<','<<v.interval<<','<<v.cycles<<','<<v.cyclesValid<<','<<v.finite<<','<<v.energy<<','<<v.delta<<','<<v.dryError<<'\n';
        }
        std::sort(wall.begin(),wall.end());std::sort(plugin.begin(),plugin.end());
        auto q=[](const auto& a,double p){return a.empty()?0.:a[std::size_t(std::ceil(p*a.size()))-1];};
        const bool pcm=count==0?energy==0:energy>1&&(active?delta>1:dryError<1e-6);
        const bool complete=measured==rows.size()&&invalid==0&&pcm&&!badBlock.load()&&!errors.load()&&!reentrant.load()&&starts.load()==1&&stops.load()==1;
        const bool failed=misses||pluginMisses||longIntervals||xruns>0;
        const char* status=!complete?"INCOMPLETE":smoke?"SELFTEST":failed?"FAIL":"PASS_IN_OBSERVED_METRICS";
        summary<<index<<','<<rate<<','<<block<<','<<count<<','<<active<<','<<wall.size()<<','<<groups<<','<<budget<<','<<q(wall,.99)<<','<<q(wall,1)<<','<<q(plugin,.99)<<','<<q(plugin,1)<<','<<misses<<','<<pluginMisses<<','<<longIntervals<<','<<xruns<<','<<cyclesValid<<','<<starts.load()<<','<<stops.load()<<','<<errors.load()<<','<<reentrant.load()<<','<<badBlock.load()<<','<<energy<<','<<delta<<','<<dryError<<','<<status<<'\n';
        raw.flush();summary.flush();
        std::cout<<"Case "<<index<<": "<<count<<" instances, active "<<active<<", "<<status<<", "<<wall.size()<<"/"<<groups<<", plugin p99 "<<q(plugin,.99)<<" us, callback p99 "<<q(wall,.99)<<" us, budget "<<budget<<", misses "<<misses<<", interval gaps "<<longIntervals<<", driver XRuns "<<xruns<<std::endl;
        if(!complete||!raw||!summary)return 1;
        return !smoke&&failed?3:0;
    }
};
template<class Create> int run(Create create,bool smoke){
    std::ofstream raw("CYBERATOR-asio-groups.csv"),summary("CYBERATOR-asio-summary.csv");
    require(bool(raw)&&bool(summary),"ASIO report open");
    raw<<std::setprecision(12)<<"case,group,wall_us,plugin_us,interval_us,thread_cycles,cycles_valid,finite,energy,delta,dry_error\n";
    summary<<std::setprecision(12)<<"case,rate,block,instances,active,groups,expected,budget_us,wall_p99_us,wall_max_us,plugin_p99_us,plugin_max_us,wall_misses,plugin_misses,long_intervals,xruns,cycles_valid,starts,stops,errors,reentrant,bad_block,energy,delta,dry_error,status\n";
    std::unique_ptr<juce::AudioIODeviceType> type;juce::String name;int rate=48000,block=64;
    if(!smoke){
        type.reset(juce::AudioIODeviceType::createAudioIODeviceType_ASIO());
        require(type!=nullptr,"ASIO support unavailable");type->scanForDevices();
        const auto names=type->getDeviceNames();require(!names.isEmpty(),"No native ASIO drivers found");
        int selected=0;
        std::cout<<"Native ASIO drivers (UR22: Yamaha Steinberg USB ASIO):\n";
        for(int i=0;i<names.size();++i)std::cout<<i<<": "<<names[i].toStdString()<<'\n';
        if(names.size()>1){std::cout<<"Driver number: "<<std::flush;std::cin>>selected;}
        require(bool(std::cin)&&selected>=0&&selected<names.size(),"Invalid driver selection");
        name=names[selected];
        std::unique_ptr<juce::AudioIODevice> d(type->createDevice(name,name));require(d!=nullptr,"ASIO device creation");
        rate=int(std::lround(d->getCurrentSampleRate()));block=d->getDefaultBufferSize();
        require((rate==44100||rate==48000)&&(block==64||block==128||block==256),"Set 44.1/48 kHz and 64/128/256 samples in ASIO control panel before starting");
        std::cout<<"Driver "<<name.toStdString()<<", current rate "<<rate<<", preferred buffer "<<block<<"; no requested change.\n";
    }
    int overall=0,index=0;
    for(const auto [count,active]:std::array<std::pair<int,bool>,6>{{{0,false},{1,false},{1,true},{4,false},{4,true},{0,false}}}){
        Case c(create,rate,block,count,active,smoke);
        int xruns=-1;
        if(smoke){
            c.audioDeviceAboutToStart(nullptr);
            std::vector<float> l(std::size_t(block),1.f),r(std::size_t(block),1.f);
            float* out[]{l.data(),r.data()};
            for(std::size_t i=0;i<c.rows.size();++i){
                std::fill(l.begin(),l.end(),1.f);std::fill(r.begin(),r.end(),1.f);
                c.audioDeviceIOCallbackWithContext(nullptr,0,out,2,block,{});
                require(std::all_of(l.begin(),l.end(),[](float v){return v==0;})&&std::all_of(r.begin(),r.end(),[](float v){return v==0;}),"selftest silent output");
            }
            c.audioDeviceStopped();
        }else{
            std::cout<<"Starting hardware case "<<index<<" ("<<count<<" instances, active "<<active<<")"<<std::endl;
            std::unique_ptr<juce::AudioIODevice> d(type->createDevice(name,name));require(d!=nullptr,"ASIO reopen");
            require(std::abs(d->getCurrentSampleRate()-rate)<.01&&d->getDefaultBufferSize()==block,"ASIO configuration changed");
            juce::BigInteger in,out;in.setRange(0,std::min(2,d->getInputChannelNames().size()),true);out.setRange(0,2,true);
            const auto error=d->open(in,out,rate,block);
            if(error.isNotEmpty())std::cerr<<error.toStdString()<<std::endl;
            require(error.isEmpty()&&d->getCurrentBufferSizeSamples()==block&&std::abs(d->getCurrentSampleRate()-rate)<.01&&d->getActiveOutputChannels().countNumberOfSetBits()==2,"ASIO actual configuration");
            std::cout<<"Actual ASIO rate "<<d->getCurrentSampleRate()<<", buffer "<<d->getCurrentBufferSizeSamples()<<", input latency "<<d->getInputLatencyInSamples()<<", output latency "<<d->getOutputLatencyInSamples()<<std::endl;
            d->start(&c);
            const auto deadline=Clock::now()+std::chrono::seconds(25);
            while(c.done.load(std::memory_order_acquire)<c.rows.size()&&!c.badBlock.load()&&!c.errors.load()&&c.starts.load()==1&&c.stops.load()==0&&Clock::now()<deadline){
                MSG msg{};while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            d->stop();d->close();xruns=d->getXRunCount(); // read after driver stop, not concurrently
        }
        const int result=c.report(raw,summary,index++,xruns,smoke);
        if(result==1)return 1;
        if(result==3)overall=3;
    }
    std::cout<<(smoke?"SELFTEST ONLY, physical ASIO NOT RUN":"Physical ASIO callback diagnostic complete; no acoustic loopback or sample-position proof")<<std::endl;
    return overall;
}
}
