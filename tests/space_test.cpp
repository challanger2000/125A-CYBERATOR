#include "dsp/Space.hpp"
#include <cstdlib>
#include <iostream>
using namespace cyberator;
void check(bool ok,const char* msg){if(!ok){std::cerr<<msg<<'\n';std::exit(1);}}
int main(){
    for(double sr:{8000.,44100.,48000.,96000.,192000.}){
        Space a,b;a.prepare(sr);b.prepare(sr);
        const auto d=std::size_t(std::round(sr*.25));
        check(a.delayFrames()==d,"delay duration");
        double early=0,late=0,stereo=0,peak=0;
        for(std::size_t n=0;n<std::size_t(6*sr);++n){
            const auto in=n==0?StereoFrame{1,0}:StereoFrame{};
            auto x=a.process(in,1,1);auto y=b.process(in,0,0);
            check(y.delay.left==0&&y.delay.right==0&&y.reverb.left==0&&y.reverb.right==0,"zero wet exact");
            double l=0,r=0;
            if(n>0&&n%d==0){const auto k=n/d;const double z=std::pow(.45,double(k-1));if(k%2)l=z;else r=z;}
            check(std::abs(x.delay.left-l)<1e-7&&std::abs(x.delay.right-r)<1e-7,"ping pong impulse timing and gain");
            const double e=double(x.reverb.left)*x.reverb.left+double(x.reverb.right)*x.reverb.right;
            check(std::isfinite(e),"finite reverb");peak=std::max(peak,e);
            if(n<std::size_t(sr))early+=e;
            if(n>=std::size_t(4*sr)&&n<std::size_t(5*sr))late+=e;
            stereo+=std::abs(x.reverb.left-x.reverb.right);
        }
        check(early>.01&&late>0&&late<early*1e-5&&stereo>.01&&peak<4,"reverb decay and stereo");
        a.reset();b.reset();
        for(std::size_t n=0;n<2*d;++n){const auto in=n==0?StereoFrame{1,0}:StereoFrame{};auto x=a.process(in,n<d?0:1,1);auto y=b.process(in,1,1);if(n>=d)check(x.delay.left==y.delay.left&&x.delay.right==y.delay.right,"warm delay");check(x.reverb.left==y.reverb.left&&x.reverb.right==y.reverb.right,"independent reverb");}
        std::cout<<sr<<" Hz decay ratio "<<late/early<<'\n';
    }
    Space a;a.prepare(48000);for(int i=0;i<96000;++i){auto x=a.process({std::numeric_limits<float>::max(),-std::numeric_limits<float>::max()},1,1);check(std::isfinite(x.delay.left)&&std::isfinite(x.reverb.right),"extreme finite");}
    a.reset();auto x=a.process({},1,1);check(x.delay.left==0&&x.reverb.right==0,"reset silence");
}
