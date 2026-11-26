#include "dsp/Engine.hpp"
#include <cstdlib>
#include <iostream>
using namespace cyberator;
void check(bool ok,const char* msg){if(!ok){std::cerr<<msg<<'\n';std::exit(1);}}
int main(){
    for(double sr:{8000.,44100.,48000.,96000.,192000.}){
        Engine e;e.prepare(sr,LatencyProfile::live);auto L=e.latency();
        check(double(L)/sr<.037,"live latency under 37ms");
        EngineParams p;p.mix=0;p.swell=p.ghost=p.granular=1;p.cyber.android=1;
        for(std::size_t i=0;i<L+50;++i){auto y=e.process(i==0?StereoFrame{.5f,-.25f}:StereoFrame{},p);check(y.left==(i==L?.5f:0)&&y.right==(i==L?-.25f:0),"live aligned dry");}
        Ghost g;g.prepare(sr,true);const auto l=g.latency();double energy=0;
        for(std::size_t i=0;i<=l;++i){auto y=g.process(i==0?StereoFrame{1,0}:StereoFrame{},0,1);check(y.dry.left==(i==l?1:0),"live ghost dry");if(i<l)energy+=double(y.swell.left)*y.swell.left;else check(y.swell.left==0,"live swell before dry");}
        check(std::abs(energy-1)<1e-6,"live swell energy");
        std::cout<<sr<<" live latency ms="<<1000*double(L)/sr<<'\n';
    }
    Engine e;e.prepare(48000,LatencyProfile::live);
    std::array<std::vector<float>,10> output;
    for(unsigned m=0;m<10;++m){e.reset();EngineParams p;p.mode=static_cast<Mode>(m);p.granular=.6;p.grain.position=.25;p.grain.scatter=.4;p.grain.spread=.5;p.cyber={3,.4,.4};p.harmony=p.ghost=p.swell=p.delay=p.space=.35;p.motion=.7;
        for(int i=0;i<48000;++i){float v=float(.2*std::sin(.037*i)+.17*std::cos(.171*i));auto y=e.process({v,v*.7f},p);check(std::isfinite(y.left),"mode finite");output[m].push_back(y.left);}
    }
    double minimum=1e10;
    for(unsigned a=0;a<10;++a)for(unsigned b=a+1;b<10;++b){double diff=0;for(std::size_t i=0;i<output[a].size();++i)diff+=std::abs(output[a][i]-output[b][i]);minimum=std::min(minimum,diff);check(diff>10,"distinct mode audio");}
    std::cout<<"min pairwise L1="<<minimum<<'\n';
    for(unsigned m=0;m<10;++m){e.reset();EngineParams p;p.mode=static_cast<Mode>(m);p.motion=1;auto L=e.latency();for(std::size_t i=0;i<L+2;++i){auto y=e.process(i==0?StereoFrame{1,1}:StereoFrame{},p);check(y.left==(i==L?1:0)&&y.right==y.left,"mode does not activate zero effects");}}
    Engine a,b;a.prepare(48000,LatencyProfile::live);b.prepare(48000,LatencyProfile::live);EngineParams p,q;p.granular=q.granular=1;p.cyber.android=1;double diff=0;
    for(int i=0;i<48000;++i){float x=float(.4*std::sin(.037*i));auto u=a.process({x,x},p),v=b.process({x,x},q);diff+=std::abs(u.left-v.left);}check(diff>10,"cyber remains audible at full granular");
    a.reset();b.reset();p=q;p.grain.position=q.grain.position=.5;p.motion=1;diff=0;
    for(int i=0;i<48000;++i){float x=float(.4*std::sin(.037*i));auto u=a.process({x,x},p),v=b.process({x,x},q);diff+=std::abs(u.left-v.left);}check(diff>10,"motion changes PCM");
}
