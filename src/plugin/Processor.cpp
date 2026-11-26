#include <juce_audio_processors/juce_audio_processors.h>
#include "control/ParameterRamp.hpp"
#include "control/SessionState.hpp"
#include "gui/Editor.hpp"
#include <atomic>
#include <memory>

namespace cyberator {
namespace {
juce::String idString(std::string_view s) { return juce::String::fromUTF8(s.data(), static_cast<int>(s.size())); }
juce::AudioProcessorValueTreeState::ParameterLayout makeLayout() {
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (std::size_t i=0;i<specs.size();++i) {
        const auto& s=specs[i];const auto name=idString(s.id);
        const juce::ParameterID id{name,1};
        if (i==static_cast<std::size_t>(Param::mode)) {
            layout.add(std::make_unique<juce::AudioParameterChoice>(id,name,
                juce::StringArray{"DREAM","CYBER","GHOST","ANDROID","PULSE","SHIMMER","SHATTER","DRIFT","ABYSS","PHANTOM"},0));
        } else if(s.discrete) {
            layout.add(std::make_unique<juce::AudioParameterBool>(id,name,s.initial!=0));
        } else {
            layout.add(std::make_unique<juce::AudioParameterFloat>(id,name,juce::NormalisableRange<float>{s.low,s.high},s.initial));
        }
    }
    return layout;
}
}
class Processor final : public juce::AudioProcessor {
public:
    Processor(): AudioProcessor(BusesProperties().withInput("Input",juce::AudioChannelSet::stereo(),true)
        .withOutput("Output",juce::AudioChannelSet::stereo(),true)),
        state_(*this,nullptr,"CYBERATOR",makeLayout()) {
        for(std::size_t i=0;i<raw_.size();++i) raw_[i]=state_.getRawParameterValue(idString(specs[i].id));
    }
    const juce::String getName()const override {return "125A CYBERATOR";}
    bool acceptsMidi()const override{return false;}
    bool producesMidi()const override{return false;}
    bool isMidiEffect()const override{return false;}
    double getTailLengthSeconds()const override{return 30.0;} // provisional conservative reporting; measure before release
    int getNumPrograms()override{return 1;}
    int getCurrentProgram()override{return 0;}
    void setCurrentProgram(int)override{}
    const juce::String getProgramName(int)override{return {};}
    void changeProgramName(int,const juce::String&)override{}
    bool hasEditor()const override{return true;}
    juce::AudioProcessorEditor* createEditor()override{return new ui::Editor(*this,state_,capture_,uiScale_,[this](const Parameters& p){return applyParameters(p);});}
    bool isBusesLayoutSupported(const BusesLayout& l)const override {
        const auto in=l.getMainInputChannelSet(),out=l.getMainOutputChannelSet();
        return in==out&&(in==juce::AudioChannelSet::mono()||in==juce::AudioChannelSet::stereo());
    }
    void prepareToPlay(double sr,int)override {
        prepared_=false;setLatencySamples(0);
        if(!std::isfinite(sr)||sr<8000||sr>192000)return;
        // Studio path retains the full anticipatory swell. Profile switching needs a host lifecycle contract.
        engine_.prepare(sr,LatencyProfile::studio);ramp_.prepare(sr);sampleRate_=sr;capture_.reset(sr);
        setLatencySamples(static_cast<int>(engine_.latency()));prepared_=true;
    }
    void releaseResources()override{prepared_=false;capture_.reset(sampleRate_);}
    void reset()override {if(prepared_){engine_.reset();ramp_.reset();capture_.reset(sampleRate_);}}
    void processBlock(juce::AudioBuffer<float>& b,juce::MidiBuffer& m)override{render(b,m,false);}
    void processBlockBypassed(juce::AudioBuffer<float>& b,juce::MidiBuffer& m)override{render(b,m,true);}
    void getStateInformation(juce::MemoryBlock& dest)override {
        const auto tree=state_.copyState();SessionState session;auto& p=session.parameters;
        for(std::size_t i=0;i<specs.size();++i){
            const auto child=tree.getChildWithProperty("id",idString(specs[i].id));
            if(!child.isValid()||!child.hasProperty("value")||
               !p.set(static_cast<Param>(i),static_cast<float>(child.getProperty("value")))){
                dest.reset();return;
            }
        }
        session.scale=uiScale_.load();const auto bytes=session.save();dest.replaceAll(bytes.data(),bytes.size());
    }
    void setStateInformation(const void* data,int size)override {
        if(data==nullptr||size<=0)return;
        SessionState session;
        if(!session.restore(std::span(static_cast<const std::uint8_t*>(data),static_cast<std::size_t>(size))))return;
        (void)applyParameters(session.parameters,&session.scale);
    }
private:
    bool applyParameters(const Parameters& p,const float* scale=nullptr){
        if(restoring_.test_and_set(std::memory_order_acquire))return false;
        struct Guard {std::atomic_flag& flag;~Guard(){flag.clear(std::memory_order_release);}} guard{restoring_};
        auto tree=state_.copyState();
        for(std::size_t i=0;i<specs.size();++i){
            auto child=tree.getChildWithProperty("id",idString(specs[i].id));
            if(!child.isValid())return false;
            child.setProperty("value",p.get(static_cast<Param>(i)),nullptr);
        }
        generation_.fetch_add(1);
        struct Generation {std::atomic<unsigned>& g;~Generation(){g.fetch_add(1);}} generation{generation_};
        state_.replaceState(tree);if(scale)uiScale_.store(*scale);return true;
    }

    void render(juce::AudioBuffer<float>& b,juce::MidiBuffer& midi,bool bypass) {
        juce::ScopedNoDenormals noDenormals;
        midi.clear();
        const auto channels=std::min(getTotalNumInputChannels(),b.getNumChannels());
        for(int c=channels;c<b.getNumChannels();++c)b.clear(c,0,b.getNumSamples());
        if(!prepared_||channels<=0||b.getNumSamples()==0)return;
        const auto begin=generation_.load();
        if((begin&1u)==0u){
            Parameters candidate=active_;
            for(std::size_t i=0;i<raw_.size();++i)
                if(raw_[i]!=nullptr)(void)candidate.set(static_cast<Param>(i),raw_[i]->load());
            if(begin==generation_.load())active_=candidate;
        }
        double bpm=120.;TransportSample transport{};
        if(auto* playhead=getPlayHead())if(auto pos=playhead->getPosition())if(auto tempo=pos->getBpm()){
            bpm=*tempo;
            if(auto ppq=pos->getPpqPosition())transport={*ppq,*tempo,pos->getIsPlaying(),true};
        }
        const double ppqStep=usableGrid(transport)?transport.bpm/(60*sampleRate_):0.;
        ramp_.target(active_);
        auto* left=b.getWritePointer(0);auto* right=channels>1?b.getWritePointer(1):nullptr;
        for(int i=0;i<b.getNumSamples();++i){
            auto p=ramp_.advance().engine(bpm);if(bypass)p.mix=0;
            p.grain.transport=transport;p.grain.transport.ppq+=ppqStep*i;
            EngineSignals signals;auto y=engine_.process({left[i],right?right[i]:left[i]},p,&signals);
            if(right){left[i]=y.left;right[i]=y.right;}
            else{left[i]=finiteSample(.5*(double(y.left)+y.right));y={left[i],left[i]};}
            capture_.push(signals.dry,y,signals.echo,signals.swell,signals.launches);
        }
    }
    juce::AudioProcessorValueTreeState state_;
    std::array<std::atomic<float>*,Parameters::count> raw_{};
    std::atomic<unsigned> generation_{0};
    std::atomic_flag restoring_=ATOMIC_FLAG_INIT;
    Parameters active_;ParameterRamp ramp_;Engine engine_;SignalCapture capture_;std::atomic<float> uiScale_{SessionState::defaultScale};double sampleRate_{48000};bool prepared_{};
};
} // namespace cyberator
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new cyberator::Processor();}
