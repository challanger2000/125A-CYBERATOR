#include "dsp/Ghost.hpp"
#include "reference/GhostOriginal.hpp"
#include <random>
#include <cstdlib>
#include <iostream>
#include <chrono>
using namespace cyberator;
void require(bool b,const char* m){if(!b){std::cerr<<m<<'\n';std::exit(1);}}
void same(StereoFrame a,StereoFrame b){require(a.left==b.left&&a.right==b.right,"sparse swell PCM mismatch");}
int main(){
    std::mt19937 rng(38471);std::uniform_real_distribution<float> dist(-1,1);
    for(double sr:{8000.,44100.,48000.,96000.,192000.})for(bool live:{false,true}){
        Ghost a;GhostOriginal b;a.prepare(sr,live);b.prepare(sr,live);
        require(a.latency()==b.latency()&&a.tail()==b.tail(),"timing unchanged");
        for(std::size_t i=0;i<3*a.tail()+123;++i){
            if(i==a.tail()+17){a.reset();b.reset();}
            StereoFrame x{dist(rng),dist(rng)};
            if(i%301==0)x.left=std::numeric_limits<float>::quiet_NaN();
            if(i%499==0)x.right=std::numeric_limits<float>::max();
            const double e=i%1001<500?.7:0,s=i%901<700?.8:0;
            const auto y=a.process(x,e,s);const auto z=b.process(x,e,s);
            same(y.dry,z.dry);same(y.echo,z.echo);same(y.swell,z.swell);
        }
    }
    std::cout<<"All sparse swell/echo samples match previous scalar implementation\n";
}
