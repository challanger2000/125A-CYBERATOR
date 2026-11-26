#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "CyberAssets.h"
#include "gui/PanelLayout.hpp"
#include "gui/KnobCrop.hpp"
#include "gui/SignalCapture.hpp"
#include "control/FactoryBank.hpp"
#include "control/SessionState.hpp"
#include <functional>

namespace cyberator::ui {
using namespace juce;
using APVTS=AudioProcessorValueTreeState;
inline const Colour chassis{0xff161b20}, inset{0xff20272e}, edge{0xff3d4851}, text{0xffdae3e8}, muted{0xff8ea0ad}, cyan{0xff80dce2}, violet{0xffb09ae8}, screen{0xff10171d};
inline Rectangle<int> rect(panel::Rect r){return {r.x,r.y,r.w,r.h};}
inline String str(std::string_view s){return String::fromUTF8(s.data(),static_cast<int>(s.size()));}
inline String id(Param p){return str(specs[static_cast<std::size_t>(p)].id);}
inline const char* asset(const String& original,int& bytes){
    for(int i=0;i<CyberAssets::namedResourceListSize;++i){
        const auto* key=CyberAssets::namedResourceList[i];
        if(original==CyberAssets::getNamedResourceOriginalFilename(key))return CyberAssets::getNamedResource(key,bytes);
    }
    bytes=0;return nullptr;
}
inline String format(Param p,double x){
    if(p==Param::size)return String(x,0)+" ms";
    if(p==Param::pitch||p==Param::formant)return String(x,1)+" st";
    if(p==Param::output)return String(x,1)+" dB";
    return String(x*100,0)+"%";
}
inline double parse(Param p,const String& s){
    const auto v=s.getDoubleValue();
    return p==Param::size||p==Param::pitch||p==Param::formant||p==Param::output?v:v*.01;
}
class HardwareLook final : public LookAndFeel_V4 {
public:
    HardwareLook(){
        setColour(Slider::textBoxTextColourId,text);setColour(Slider::textBoxBackgroundColourId,screen);
        setColour(Slider::textBoxOutlineColourId,edge);setColour(Slider::textBoxHighlightColourId,cyan.withAlpha(.2f));
        setColour(ComboBox::backgroundColourId,screen);setColour(ComboBox::outlineColourId,edge);
        setColour(ComboBox::textColourId,text);setColour(ComboBox::arrowColourId,muted);
        setColour(PopupMenu::backgroundColourId,screen);setColour(PopupMenu::textColourId,text);
        setColour(PopupMenu::highlightedBackgroundColourId,Colour(0xff354954));setColour(PopupMenu::highlightedTextColourId,text);
        setColour(TextButton::buttonColourId,screen);setColour(TextButton::buttonOnColourId,Colour(0xff365760));
        setColour(TextButton::textColourOffId,text);setColour(TextButton::textColourOnId,cyan);
    }
    void drawRotarySlider(Graphics& g,int x,int y,int w,int h,float pos,float,float,Slider& slider)override{
        const auto f=slider.getProperties()["family"].toString();
        const int family=f=="s"?0:f=="m"?1:2;
        const int maxDiameter=std::array<int,3>{64,88,104}[static_cast<std::size_t>(family)];
        const float scale=Component::getApproximateScaleFactorForComponent(&slider);
        const auto& crop=chooseCrop(static_cast<std::size_t>(family),float(maxDiameter)*scale);
        const int selected=crop.resolution;
        auto& cache=cache_[static_cast<std::size_t>(family)];
        if(cache.scale!=selected){
            cache.scale=selected;cache.strip={};
            const auto resource="knob_"+f+"_"+String(selected)+".png";
            int size=0;
            if(const auto* data=asset(resource,size)){
                auto decoded=ImageFileFormat::loadFrom(data,static_cast<std::size_t>(size));
                const int expected=crop.frame;
                if(decoded.isValid()&&decoded.getWidth()==expected&&decoded.getHeight()==expected*128)
                    cache.strip=std::move(decoded);
            }
        }
        if(!cache.strip.isValid()){
            g.setColour(Colours::orange);g.drawFittedText("ASSET ERROR",x,y,w,h,Justification::centred,1);return;
        }
        const int d=std::min({w,h,static_cast<int>(slider.getProperties()["diameter"])});
        const int frame=std::clamp(roundToInt(std::clamp(pos,0.f,1.f)*127),0,127);
        g.setImageResamplingQuality(Graphics::highResamplingQuality);
        g.drawImage(cache.strip,x+(w-d)/2,y+(h-d)/2,d,d,crop.x,frame*crop.frame+crop.y,crop.size,crop.size,false);
    }
    [[nodiscard]] bool artworkReady()const noexcept{for(const auto& c:cache_)if(!c.strip.isValid())return false;return true;}
private:
    struct Cache{int scale{};Image strip;};
    std::array<Cache,3> cache_;
};
class SignalDisplay final:public Component {
public:
    explicit SignalDisplay(SignalCapture& source):source_(source){setComponentID("signal-display");setInterceptsMouseClicks(false,false);}
    void update(){
        SignalCapture::Snapshot next;
        const auto now=Time::getMillisecondCounterHiRes();
        if(source_.read(next)&&next.serial!=serial_){
            serial_=next.serial;data_=next;last_=now;valid_=true;analyse();repaint();
        }else if(valid_&&now-last_>550){valid_=false;repaint();}
    }
    [[nodiscard]] bool hasFrame()const noexcept{return valid_;}
    [[nodiscard]] const SignalCapture::Snapshot& latestFrame()const noexcept{return data_;}
    void paint(Graphics& g)override{
        const auto bounds=getLocalBounds().toFloat();g.setColour(screen);g.fillRoundedRectangle(bounds,5);
        g.setColour(edge);g.drawRoundedRectangle(bounds.reduced(.5f),5,1);
        g.setFont(FontOptions(11.f));g.setColour(muted);
        g.drawText("ALIGNED DRY / OUTPUT  /  LEFT",12,7,getWidth()-24,16,Justification::centredLeft);
        const Rectangle<float> wave{12,31,float(getWidth()-24),120};
        const Rectangle<float> spec{12,181,float(getWidth()-24),73};
        g.setColour(edge.withAlpha(.65f));
        for(int i=0;i<=4;++i)g.drawHorizontalLine(roundToInt(wave.getY()+wave.getHeight()*float(i)/4),wave.getX(),wave.getRight());
        for(int i=0;i<=3;++i)g.drawHorizontalLine(roundToInt(spec.getY()+spec.getHeight()*float(i)/3),spec.getX(),spec.getRight());
        g.setColour(muted);g.drawText("OUTPUT SPECTRUM  /  dBFS",12,160,getWidth()-24,16,Justification::centredLeft);
        if(valid_){
            trace(g,wave,data_.dry,muted.withAlpha(.65f));trace(g,wave,data_.wet,cyan);
            const float bw=spec.getWidth()/float(bands_.size());
            g.setColour(cyan.withAlpha(.85f));
            for(std::size_t i=0;i<bands_.size();++i){
                const float h=spec.getHeight()*bands_[i];g.fillRect(spec.getX()+float(i)*bw,spec.getBottom()-h,std::max(1.f,bw-2),h);
            }
            g.setColour(muted);g.drawText("40 Hz",12,254,100,12,Justification::centredLeft);
            g.drawText(String(std::min(20000.,data_.sampleRate*.5)/1000.,1)+" kHz",getWidth()-112,254,100,12,Justification::centredRight);
        }else{
            g.setColour(muted);g.drawText("NO AUDIO FRAME",wave.toNearestInt(),Justification::centred);
        }
        auto metric=[&](int x,int w,const String& name,const String& value,Colour colour){
            g.setColour(muted);g.setFont(FontOptions(10.f));g.drawText(name,x,274,w,13,Justification::centredLeft);
            g.setColour(colour);g.setFont(FontOptions(13.f));g.drawText(valid_?value:String("-"),x,288,w,17,Justification::centredLeft);
        };
        metric(12,72,"GRAINS",String(static_cast<int64>(data_.grains)),cyan);
        metric(84,91,"ECHO PRE-MIX",db(data_.echoPeak),violet);
        metric(177,91,"SWELL PRE-MIX",db(data_.swellPeak),violet);
        metric(270,118,"OUT L / R",db(data_.peakL)+" / "+db(data_.peakR),data_.peakL>=1||data_.peakR>=1?Colours::orange:cyan);
    }
private:
    static String db(float v){return v>0?String(20*std::log10(v),1):String("-inf");}
    static void trace(Graphics& g,Rectangle<float> r,const std::array<float,SignalCapture::frames>& data,Colour c){
        Path path;const float mid=r.getCentreY(),amp=r.getHeight()*.47f;
        for(std::size_t i=0;i<data.size();++i){
            const float x=r.getX()+r.getWidth()*float(i)/float(data.size()-1);
            const float y=mid-amp*std::clamp(data[i],-1.f,1.f);
            if(i==0)path.startNewSubPath(x,y);else path.lineTo(x,y);
        }
        g.setColour(c);g.strokePath(path,PathStrokeType(1.f));
    }
    void analyse(){
        constexpr auto n=SignalCapture::frames;
        fftData_.fill(0);
        for(std::size_t i=0;i<n;++i)fftData_[i]=data_.wet[i]*(.5f-.5f*std::cos(float(2*std::numbers::pi*double(i)/double(n))));
        fft_.performFrequencyOnlyForwardTransform(fftData_.data(),true);
        const double high=std::min(20000.,data_.sampleRate*.5);
        if(high<=40){bands_.fill(0);return;}
        for(std::size_t j=0;j<bands_.size();++j){
            const double a=40*std::pow(high/40,double(j)/double(bands_.size()));
            const double b=40*std::pow(high/40,double(j+1)/double(bands_.size()));
            const auto lo=std::clamp(std::size_t(std::ceil(a*n/data_.sampleRate)),std::size_t(1),n/2);
            const auto hi=std::clamp(std::size_t(std::ceil(b*n/data_.sampleRate)),lo+std::size_t(lo<n/2),n/2+1);
            float peak=0;for(auto k=lo;k<hi;++k)peak=std::max(peak,fftData_[k]*4.f/float(n));
            bands_[j]=std::clamp((20*std::log10(std::max(peak,1e-9f))+72)/72,0.f,1.f);
        }
    }
    SignalCapture& source_;SignalCapture::Snapshot data_{};std::uint64_t serial_{};double last_{};bool valid_{};
    dsp::FFT fft_{11};std::array<float,SignalCapture::frames*2> fftData_{};std::array<float,40> bands_{};
};
class Content final:public Component,private Timer {
public:
    using Apply=std::function<bool(const Parameters&)>;
    Content(APVTS& state,SignalCapture& capture,Apply apply):state_(state),apply_(std::move(apply)),display_(capture){
        setComponentID("cyberator-panel");setSize(panel::width,panel::height);setLookAndFeel(&look_);
        int length=0;if(auto* bytes=asset("125A_Logo_Master_FINAL.svg",length))
            logo_=Drawable::createFromImageData(bytes,static_cast<std::size_t>(length));
        for(std::size_t i=0;i<panel::knobs.size();++i){
            const auto& k=panel::knobs[i];const auto p=k.param;auto slider=std::make_unique<Slider>();
            slider->setComponentID(id(p));slider->setName(k.name);slider->setTooltip(k.name);
            slider->getProperties().set("family",String::charToString(k.family));slider->getProperties().set("diameter",k.diameter);
            slider->setSliderStyle(Slider::RotaryHorizontalVerticalDrag);
            slider->setTextBoxStyle(Slider::TextBoxBelow,false,std::min(84,k.bounds.w-8),20);
            addAndMakeVisible(*slider);
            slider->setBounds(k.bounds.x,k.bounds.y+18,k.bounds.w,k.bounds.h-18);
            bindings_[i]=std::make_unique<APVTS::SliderAttachment>(state_,id(p),*slider);
            slider->setDoubleClickReturnValue(true,specs[static_cast<std::size_t>(p)].initial,ModifierKeys::ctrlModifier);
            slider->textFromValueFunction=[p](double v){return format(p,v);};
            slider->valueFromTextFunction=[p](const String& s){return parse(p,s);};
            slider->updateText();sliders_[i]=std::move(slider);
        }
        mode_.setComponentID("mode");mode_.addItemList({"DREAM","CYBER","GHOST","ANDROID","PULSE","SHIMMER","SHATTER","DRIFT","ABYSS","PHANTOM"},1);
        addAndMakeVisible(mode_);mode_.setBounds(rect(panel::mode));
        modeBinding_=std::make_unique<APVTS::ComboBoxAttachment>(state_,"mode",mode_);
        freeze_.setComponentID("freeze");sync_.setComponentID("sync");
        freeze_.setClickingTogglesState(true);sync_.setClickingTogglesState(true);
        addAndMakeVisible(freeze_);addAndMakeVisible(sync_);freeze_.setBounds(rect(panel::freeze));sync_.setBounds(rect(panel::sync));
        freezeBinding_=std::make_unique<APVTS::ButtonAttachment>(state_,"freeze",freeze_);
        syncBinding_=std::make_unique<APVTS::ButtonAttachment>(state_,"sync",sync_);
        preset_.setComponentID("factory-preset");preset_.setTextWhenNothingSelected("CUSTOM / FACTORY PRESETS");
        constexpr std::array<const char*,7> names{"Cyber Vocals","Ghost Voices","Granular Atmospheres","Rhythmic","Dark & Industrial","Ambient & Shimmer","Extreme & Experimental"};
        unsigned previousCategory=99;
        for(std::size_t i=0;i<factoryBank.size();++i){
            const auto cat=static_cast<unsigned>(factoryBank[i].category);
            if(cat!=previousCategory){preset_.addSectionHeading(names[cat]);previousCategory=cat;}
            preset_.addItem(str(factoryBank[i].name),int(i+1));
        }
        previous_.setComponentID("preset-previous");next_.setComponentID("preset-next");
        addAndMakeVisible(preset_);addAndMakeVisible(previous_);addAndMakeVisible(next_);
        preset_.setBounds(rect(panel::preset));previous_.setBounds(rect(panel::previous));next_.setBounds(rect(panel::next));
        preset_.onChange=[this]{choose(preset_.getSelectedId());};
        previous_.onClick=[this]{const auto index=matchingPreset();choose(index<=1?int(factoryBank.size()):index-1);};
        next_.onClick=[this]{const auto index=matchingPreset();choose(index>=int(factoryBank.size())?1:index+1);};
        addAndMakeVisible(display_);display_.setBounds(rect(panel::display));
        startTimerHz(30);
    }
    ~Content()override{stopTimer();setLookAndFeel(nullptr);}
    [[nodiscard]] bool artworkReady()const noexcept{return logo_!=nullptr&&look_.artworkReady();}
    void paint(Graphics& g)override{
        g.fillAll(chassis);g.setColour(inset);g.fillRect(0,0,panel::width,90);
        g.setColour(edge);g.drawHorizontalLine(89,0,float(panel::width));
        if(logo_)logo_->drawWithin(g,rect(panel::logo).toFloat(),RectanglePlacement::centred,1);
        else{g.setColour(Colours::orange);g.drawText("LOGO ERROR",rect(panel::logo),Justification::centred);}
        g.setColour(text);g.setFont(FontOptions(31.f,Font::bold));g.drawText("CYBERATOR",158,17,530,35,Justification::centredLeft);
        g.setColour(muted);g.setFont(FontOptions(12.f));g.drawText("GRANULAR  /  SPECTRAL  /  REVERSE FX",160,52,540,18,Justification::centredLeft);
        paintPanel(g,panel::granular,"01 / GRANULAR",cyan);paintPanel(g,panel::signal,"SIGNAL / ANALYSIS",cyan);
        paintPanel(g,panel::cybernetic,"02 / CYBERNETIC",cyan);paintPanel(g,panel::ghost,"03 / GHOST",violet);
        paintPanel(g,panel::space,"04 / SPACE",violet);paintPanel(g,panel::global,"MASTER",text);
        g.setFont(FontOptions(13.f));g.setColour(text);
        for(const auto& k:panel::knobs)g.drawText(k.name,k.bounds.x,k.bounds.y,k.bounds.w,18,Justification::centred);
    }
private:
    static void paintPanel(Graphics& g,panel::Rect area,const String& title,Colour accent){
        const auto r=rect(area).toFloat();g.setColour(inset);g.fillRoundedRectangle(r,8);g.setColour(edge);g.drawRoundedRectangle(r.reduced(.5f),8,1);
        g.drawHorizontalLine(area.y+35,float(area.x+12),float(area.x+area.w-12));
        g.setColour(accent);g.setFont(FontOptions(15.f,Font::bold));g.drawText(title,area.x+16,area.y+5,area.w-32,26,Justification::centredLeft);
    }
    int matchingPreset()const{
        std::array<float,Parameters::count> current{};
        for(std::size_t i=0;i<current.size();++i){
            auto* raw=state_.getRawParameterValue(str(specs[i].id));if(!raw)return 0;current[i]=raw->load();
        }
        for(std::size_t i=0;i<factoryBank.size();++i){
            bool same=true;
            for(std::size_t k=0;k<current.size();++k)if(std::abs(current[k]-factoryBank[i].values[k])>1e-5f*(specs[k].high-specs[k].low)){same=false;break;}
            if(same)return int(i+1);
        }
        return 0;
    }
    void choose(int index){if(index<1||index>int(factoryBank.size()))return;
        if(apply_(factoryBank[std::size_t(index-1)].parameters()))preset_.setSelectedId(index,dontSendNotification);
    }
    void timerCallback()override{display_.update();if(!preset_.isPopupActive())preset_.setSelectedId(matchingPreset(),dontSendNotification);}
    APVTS& state_;Apply apply_;HardwareLook look_;std::unique_ptr<Drawable> logo_;
    std::array<std::unique_ptr<Slider>,20> sliders_;
    std::array<std::unique_ptr<APVTS::SliderAttachment>,20> bindings_;
    ComboBox mode_,preset_;TextButton freeze_{"FREEZE"},sync_{"SYNC"},previous_{"<"},next_{">"};
    std::unique_ptr<APVTS::ComboBoxAttachment> modeBinding_;
    std::unique_ptr<APVTS::ButtonAttachment> freezeBinding_,syncBinding_;
    SignalDisplay display_;
};
class Editor final:public AudioProcessorEditor,private Timer {
public:
    Editor(AudioProcessor& p,APVTS& state,SignalCapture& capture,std::atomic<float>& scale,Content::Apply apply)
        :AudioProcessorEditor(p),scale_(scale),content_(state,capture,std::move(apply)){
        const auto saved=std::clamp(scale_.load(),SessionState::minScale,SessionState::maxScale);
        addAndMakeVisible(content_);setResizable(true,true);
        setResizeLimits(roundToInt(panel::width*SessionState::minScale),roundToInt(panel::height*SessionState::minScale),
                        roundToInt(panel::width*SessionState::maxScale),roundToInt(panel::height*SessionState::maxScale));
        getConstrainer()->setFixedAspectRatio(double(panel::width)/panel::height);
        setSize(roundToInt(panel::width*saved),roundToInt(panel::height*saved));startTimerHz(10);
    }
    ~Editor()override{stopTimer();}
    void paint(Graphics& g)override{g.fillAll(chassis);}
    void resized()override{
        if(getWidth()<=0)return;
        const float scale=float(getWidth())/panel::width;
        content_.setTransform(AffineTransform::scale(scale));scale_.store(std::clamp(scale,SessionState::minScale,SessionState::maxScale));
    }
private:
    void timerCallback()override{
        const float target=std::clamp(scale_.load(),SessionState::minScale,SessionState::maxScale);
        if(std::abs(float(getWidth())/panel::width-target)>.002f)setSize(roundToInt(panel::width*target),roundToInt(panel::height*target));
    }
    std::atomic<float>& scale_;Content content_;
};
} // namespace cyberator::ui
