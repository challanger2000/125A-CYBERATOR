#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>

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
    require(p.getLatencySamples()==9728,"VST3 reported latency");
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
    auto create=[&]{
        juce::String error;
        auto p=format.createInstanceFromDescription(*types[0],48000,127,error);
        if(!p)std::cerr<<error.toStdString()<<'\n';
        require(p!=nullptr,"load actual VST3");
        return p;
    };
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
