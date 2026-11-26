#include "dsp/Cybernetic.hpp"
#include "reference/CyberneticP026.hpp"
#include <iostream>
#include <random>
#include <cstdlib>
using namespace cyberator;
void check(bool v,const char* m){if(!v){std::cerr<<m<<'\n';std::exit(1);}}
int main(){
    std::mt19937 rng(9284);std::uniform_real_distribution<float> noise(-.7f,.7f);
    double worst=0,worstRelative=0;
    for(double sr:{8000.,44100.,48000.,96000.,192000.})for(bool live:{false,true})for(int kind=0;kind<4;++kind){
        Cybernetic a;CyberneticP026 b;a.prepare(sr,live);b.prepare(sr,live);
        check(a.latency()==b.latency(),"identical latency");
        double peak=0,err=0,energy=0;
        for(int i=0;i<int(sr*1.1);++i){
            if(i==int(sr*.56)){a.reset();b.reset();}
            const float l=noise(rng),r=kind==0?noise(rng):kind==1?l:kind==2?-l:0;
            CyberParams p;
            if(i%int(sr*.27)>int(sr*.12)&&i%int(sr*.27)<int(sr*.21))p={5,.4,.8};
            const auto y=a.process({l,r},p),z=b.process({l,r},p);
            const double dl=double(y.left)-z.left,dr=double(y.right)-z.right;
            peak=std::max({peak,std::abs(dl),std::abs(dr)});err+=dl*dl+dr*dr;energy+=double(z.left)*z.left+double(z.right)*z.right;
            if(kind==3)check(y.right==0,"silent channel exact");
            if(kind==2)check(y.left==-y.right,"anti-phase exact");
        }
        const double rel=std::sqrt(err/energy);worst=std::max(worst,peak);worstRelative=std::max(worstRelative,rel);
        check(a.missedFrames()==0&&b.missedFrames()==0,"all jobs completed");
        check(peak<2e-6&&rel<1e-6,"neutral and active transitions preserve prior PCM");
    }
    std::cout<<"Neutral shortcut peak "<<worst<<", relative RMS "<<worstRelative<<'\n';
}
