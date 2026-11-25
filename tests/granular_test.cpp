#include "dsp/Granular.hpp"
#include <cstdlib>
#include <iostream>
#include <limits>
using namespace cyberator;
void check(bool x,const char* msg) { if(!x){std::cerr<<msg<<'\n';std::exit(1);} }
int main(){
    for(double sr:{8000.,44100.,48000.,96000.,192000.}) {
        Granular g;g.prepare(sr);GranularParams p;p.density=0;p.sizeMs=20;
        for(int n=0;n<int(sr/2);++n){auto y=g.process({0.5f,-0.25f},p);check(y.left==0&&y.right==0,"zero density");}
        p.density=.5;p.freeze=true;p.sync=true;p.bpm=120;
        auto start=g.launches();double sum=0;
        for(int n=0;n<int(sr);++n){auto y=g.process({},p);sum+=std::abs(y.left);check(std::abs(y.right+.5*y.left)<1e-6,"stereo identity");}
        check(g.launches()-start==16,"16 events per second");check(sum>sr*.01,"active frozen PCM");
        p.bpm=60;start=g.launches();for(int n=0;n<int(sr);++n)(void)g.process({},p);
        check(g.launches()-start==8,"tempo changes event rate");
    }
    Granular a,b;a.prepare(48000);b.prepare(48000);GranularParams p;p.density=.8;p.reverse=.5;p.scatter=.8;p.position=.5;p.spread=.9;
    double difference=0,energy=0;
    for(int n=0;n<48000;++n){float v=float(std::sin(n*.071));auto x=a.process({v,v},p);auto y=b.process({v,v},p);check(x.left==y.left&&x.right==y.right,"deterministic independent instances");difference+=std::abs(x.left-x.right);energy+=std::abs(x.left);}
    check(difference>10 && energy>10,"stereo spread audible");
    a.reset();b.reset();p.spread=0;p.reverse=1;p.feedback=0;
    auto q=p;q.feedback=1;double delta=0;
    for(int n=0;n<96000;++n){float v=n<24000?float(.5*std::sin(n*.071)):0;auto x=a.process({v,v},p);auto y=b.process({v,v},q);check(std::isfinite(y.left)&&std::abs(y.left)<2,"bounded feedback");delta+=std::abs(x.left-y.left);}
    check(delta>10,"feedback affects PCM");
    p.density=std::numeric_limits<double>::quiet_NaN();check(a.process({1,1},p).left==0,"invalid density safe");
    std::cout<<"Granular scheduling contract PASS\n";
}
