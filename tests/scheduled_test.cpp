#include "dsp/Engine.hpp"
#include "reference/EngineImmediate.hpp"
#include <cstdlib>
#include <iostream>
using namespace cyberator;
void require(bool b,const char* msg){if(!b){std::cerr<<msg<<'\n';std::exit(1);}}
int main(){
    for(double sr:{8000.,44100.,48000.,96000.,192000.})for(bool live:{false,true}){
        const auto profile=live?LatencyProfile::live:LatencyProfile::studio;
        Engine a;EngineImmediateReference b;a.prepare(sr,profile);b.prepare(sr,profile);
        const auto extra=a.latency()-b.latency();History align;align.prepare(extra+1);
        for(int mode=0;mode<3;++mode){
            a.reset();b.reset();align.reset();EngineParams p;
            if(mode==1){p.granular=.7;p.grain.sizeMs=60;p.grain.density=.8;p.grain.scatter=.4;p.grain.reverse=.4;p.cyber={4,.4,.6};p.harmony=.6;p.swell=.3;p.ghost=.4;p.delay=.4;p.space=.4;p.motion=.3;p.outputDb=-3;}
            if(mode==2){p.granular=.8;p.grain.pitch=-12;p.grain.feedback=.5;p.cyber={-6,1,1};p.harmony=.4;p.swell=.7;p.ghost=.8;p.space=.5;p.mix=.7;p.mode=Mode::abyss;}
            double peak=0;for(int i=0;i<int(sr*.45)+int(a.latency());++i){const double t=i/sr;const StereoFrame x=i<int(sr*.4)?StereoFrame{float(.2*std::sin(1031*t)+.3*std::cos(277*t)),float(.3*std::cos(2971*t))}:StereoFrame{};
                const auto y=a.process(x,p);align.capture(b.process(x,p));const auto z=align.read(extra);
                peak=std::max({peak,std::abs(double(y.left)-z.left),std::abs(double(y.right)-z.right)});}
            if(peak!=0)std::cerr<<sr<<' '<<live<<' '<<mode<<" peak "<<peak<<'\n';
            require(peak==0,"complete graph identical with declared extra latency");
        }
    }
    std::cout<<"All staged engine fixtures bit-identical after latency alignment\n";
}
