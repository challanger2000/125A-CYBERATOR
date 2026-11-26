#include "dsp/Engine.hpp"
#include <cstdlib>
#include <iostream>
using namespace cyberator;
void check(bool ok,const char* msg){if(!ok){std::cerr<<msg<<'\n';std::exit(1);}}
int main(){
    for(double sr:{8000.,44100.,48000.,96000.,192000.}){
        Harmony h;h.prepare(sr);std::array<double,3> re{},im{};
        for(int i=0;i<int(2*sr);++i){const float x=float(.5*std::sin(2*std::numbers::pi*200*i/sr));auto y=h.process({x,-x},1);check(y.left==-y.right,"harmony stereo polarity");if(i>=int(sr)){for(std::size_t j=0;j<3;++j){const double t=2*std::numbers::pi*100*double(1u<<j)*i/sr;re[j]+=y.left*std::cos(t);im[j]+=y.left*std::sin(t);}}}
        const double lo=std::hypot(re[0],im[0])/sr,mid=std::hypot(re[1],im[1])/sr,hi=std::hypot(re[2],im[2])/sr;
        std::cout<<sr<<" harmony "<<lo<<' '<<mid<<' '<<hi<<'\n';check(lo>.10&&hi>.10&&mid<.01,"octave frequencies");
        check(h.process({1,1},0).left==0,"harmony zero");
        Engine e;e.prepare(sr);const auto L=e.latency();check(L>std::size_t(.16*sr),"combined latency");
        for(int mode=0;mode<2;++mode){e.reset();EngineParams p;if(mode==1){p.mix=0;p.granular=p.harmony=p.ghost=p.swell=p.delay=p.space=1;p.cyber={12,1,1};p.outputDb=12;}
            for(std::size_t i=0;i<L+400;++i){auto y=e.process(i==0?StereoFrame{.25f,-.5f}:StereoFrame{},p);check(y.left==(i==L?.25f:0)&&y.right==(i==L?-.5f:0),"exact aligned dry");}}
    }
    Engine a,b;a.prepare(48000);b.prepare(48000);const auto L=a.latency();
    for(int mode=0;mode<7;++mode){a.reset();b.reset();EngineParams p,q;switch(mode){case 0:p.granular=.8;break;case 1:p.cyber.formant=8;break;case 2:p.harmony=1;break;case 3:p.ghost=1;break;case 4:p.swell=1;break;case 5:p.delay=1;break;case 6:p.space=1;break;}
        double delta=0;for(std::size_t i=0;i<L+48000;++i){float x=i<24000?float(.3*std::sin(.031*double(i))+.2*std::cos(.13*double(i))):0;const auto u=a.process({x,x*.7f},p),v=b.process({x,x*.7f},q);check(std::isfinite(u.left)&&std::isfinite(u.right),"graph finite");delta+=std::abs(u.left-v.left);}std::cout<<"mode "<<mode<<" delta "<<delta<<'\n';check(delta>1,"independent effect changes PCM");}
}
