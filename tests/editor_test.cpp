#include <juce_audio_processors/juce_audio_processors.h>
#include "control/SessionState.hpp"
#include "control/FactoryBank.hpp"
#include "gui/Editor.hpp"
#include <cstdlib>
#include <iostream>
void require(bool b,const char* m){if(!b){std::cerr<<m<<'\n';std::exit(1);}}
juce::Component* find(juce::Component& c,const juce::String& id){
    if(c.getComponentID()==id)return &c;
    for(auto* child:c.getChildren())if(auto* match=find(*child,id))return match;
    return nullptr;
}
cyberator::SessionState state(juce::AudioProcessor& p){
    juce::MemoryBlock b;p.getStateInformation(b);cyberator::SessionState s;
    require(s.restore(std::span(static_cast<const std::uint8_t*>(b.getData()),b.getSize())),"editor state decode");return s;
}
int main(){
    using namespace cyberator;
    juce::ScopedJuceInitialiser_GUI init;
    std::unique_ptr<juce::AudioProcessor> p(createPluginFilter());
    require(p&&p->hasEditor(),"editor advertised");
    const auto before=state(*p).parameters.save();
    // Direct harness is the host: JUCE requires unregister before deleting an active editor.
    auto destroy=[&](juce::AudioProcessorEditor* editor){if(editor){p->editorBeingDeleted(editor);delete editor;}};
    using EditorPtr=std::unique_ptr<juce::AudioProcessorEditor,decltype(destroy)>;
    {
        EditorPtr ed(p->createEditorAndMakeActive(),destroy);
        require(ed!=nullptr&&ed->getWidth()==1020,"initial editor scale");
        for(const auto& k:panel::knobs){
            auto id=juce::String::fromUTF8(specs[std::size_t(k.param)].id.data(),int(specs[std::size_t(k.param)].id.size()));
            auto* slider=dynamic_cast<juce::Slider*>(find(*ed,id));
            require(slider!=nullptr,"all knobs bound");
            require(std::abs(slider->getDoubleClickReturnValue()-specs[std::size_t(k.param)].initial)<1e-6,"actual default reset value");
        }
        require(state(*p).parameters.save()==before,"editor open neutral");
        auto* preset=dynamic_cast<juce::ComboBox*>(find(*ed,"factory-preset"));
        require(preset&&preset->getNumItems()==120,"all factory presets exposed");
        preset->setSelectedId(1,juce::sendNotificationSync);
        auto recalled=state(*p);
        for(std::size_t i=0;i<Parameters::count;++i)require(std::abs(recalled.parameters.get(static_cast<Param>(i))-factoryBank[0].values[i])<.0001f,"preset host state");
        auto* slider=dynamic_cast<juce::Slider*>(find(*ed,"output"));
        require(slider!=nullptr,"output slider");
        slider->setValue(6,juce::sendNotificationSync);
        require(std::abs(state(*p).parameters.get(Param::output)-6)<1e-5,"GUI gesture changes host state");
        ed->setSize(1200,730);require(std::abs(state(*p).scale-1)<.001,"scale saved");
        auto img=ed->createComponentSnapshot(ed->getLocalBounds(),true);
        auto* content=dynamic_cast<ui::Content*>(find(*ed,"cyberator-panel"));
        require(content&&content->artworkReady(),"logo and all active filmstrip families rendered");
        require(img.isValid()&&img.getWidth()==1200&&img.getHeight()==730,"real editor renders");
        juce::File dest=juce::File::getCurrentWorkingDirectory().getChildFile("CYBERATOR-editor-100.png");
        if(auto stream=dest.createOutputStream()){stream->setPosition(0);stream->truncate();juce::PNGImageFormat format;require(format.writeImageToStream(img,*stream),"snapshot PNG");}
        else require(false,"snapshot output");
    }
    require(p->getActiveEditor()==nullptr,"direct host unregisters destroyed editor");
    const auto saved=state(*p);
    EditorPtr ed(p->createEditorAndMakeActive(),destroy);
    if(ed)std::cout<<"Reopen "<<ed->getWidth()<<" x "<<ed->getHeight()<<" saved scale "<<saved.scale<<'\n';
    require(ed&&ed->getWidth()==1200&&ed->getHeight()==730,"reopen scale");
    require(state(*p).parameters.save()==saved.parameters.save(),"reopen preserves preset edits");
    for(float scale:{.65f,1.5f}){
        ed->setSize(juce::roundToInt(panel::width*scale),juce::roundToInt(panel::height*scale));
        auto image=ed->createComponentSnapshot(ed->getLocalBounds(),true);
        auto* content=dynamic_cast<ui::Content*>(find(*ed,"cyberator-panel"));
        require(image.isValid()&&content&&content->artworkReady(),"scaled artwork decoded");
        auto dest=juce::File::getCurrentWorkingDirectory().getChildFile("CYBERATOR-editor-"+juce::String(juce::roundToInt(scale*100))+".png");
        auto stream=dest.createOutputStream();require(stream!=nullptr,"scaled screenshot output");
        stream->setPosition(0);stream->truncate();juce::PNGImageFormat png;
        require(png.writeImageToStream(image,*stream),"scaled screenshot encoded");
    }
    ed->setSize(1200,730);
    p->setRateAndBufferSizeDetails(48000,128);p->prepareToPlay(48000,128);
    juce::MidiBuffer midi;
    std::array<float,SignalCapture::frames> actual{};
    for(int offset=0;offset<32768;offset+=128){
        juce::AudioBuffer<float> audio(2,128);
        for(int i=0;i<128;++i){const float x=float(.25*std::sin(2*std::numbers::pi*220*(offset+i)/48000.));audio.setSample(0,i,x);audio.setSample(1,i,.5f*x);}
        p->processBlock(audio,midi);
        if(offset>=32768-int(actual.size()))for(int i=0;i<128;++i)actual[std::size_t(offset-(32768-int(actual.size()))+i)]=audio.getSample(0,i);
    }
    auto* display=dynamic_cast<ui::SignalDisplay*>(find(*ed,"signal-display"));
    require(display!=nullptr,"real signal display");
    display->update();
    require(display->hasFrame()&&display->latestFrame().sampleRate==48000,"display receives actual audio frame");
    require(display->latestFrame().wet==actual,"display waveform equals processor output");
    auto activeImage=ed->createComponentSnapshot(ed->getLocalBounds(),true);
    auto dest=juce::File::getCurrentWorkingDirectory().getChildFile("CYBERATOR-editor-active.png");
    auto stream=dest.createOutputStream();require(stream!=nullptr,"active screenshot output");
    stream->setPosition(0);stream->truncate();juce::PNGImageFormat png;
    require(png.writeImageToStream(activeImage,*stream),"active screenshot encoded");
    ed.reset();
    // Audio continues while closed; reopening before another callback cannot expose old PCM.
    juce::AudioBuffer<float> closed(2,128);closed.clear();p->processBlock(closed,midi);
    ed.reset(p->createEditorAndMakeActive());require(ed!=nullptr,"reopen while processing");
    display=dynamic_cast<ui::SignalDisplay*>(find(*ed,"signal-display"));
    require(display!=nullptr,"reopened display");
    display->update();require(!display->hasFrame(),"no stale waveform on reopen");
    for(std::size_t i=0;i<SignalCapture::frames/128;++i){
        closed.clear();p->processBlock(closed,midi);
        for(int j=0;j<128;++j)actual[i*128+std::size_t(j)]=closed.getSample(0,j);
    }
    display->update();require(display->hasFrame()&&display->latestFrame().wet==actual,"new epoch exact processor PCM");
    p->releaseResources();
    std::cout<<"Direct native editor smoke PASS\n";
}
