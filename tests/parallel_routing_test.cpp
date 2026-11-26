#include "dsp/ParallelRouting.hpp"
#include <cstdlib>
#include <iostream>
#include <limits>
using namespace cyberator;
void require(bool b,const char* msg){if(!b){std::cerr<<msg<<'\n';std::exit(1);}}
static double render(ParallelWeights w){return ((1-w.spectral)*.75+w.spectral*.60)*(1-w.granular)-.75*w.granular;}
int main(){
    for(double sr:{8000.,44100.,48000.,96000.,192000.}){
        ParallelRouting a;a.prepare(sr);auto w=a.process(1,0,0,0);
        require(w.spectral==0&&w.granular==1,"exact inactive endpoints");
        const auto n=static_cast<int>(std::round(.020*sr));
        double prev=render(w),peak=0;
        for(int i=0;i<n;++i){w=a.process(1,0,1,0);const double y=render(w);peak=std::max(peak,std::abs(y-prev));prev=y;}
        require(w.spectral==1&&w.granular==.5,"exact full strength endpoints");
        require(peak<=1.65/n+1e-12,"bounded activation discontinuity");
        peak=0;
        for(int i=0;i<n;++i){w=a.process(1,0,0,0);const double y=render(w);peak=std::max(peak,std::abs(y-prev));prev=y;}
        require(w.spectral==0&&w.granular==1,"exact release endpoints");
        require(peak<=1.65/n+1e-12,"bounded deactivation discontinuity");
        a.reset();w=a.process(1,0,.001,0);
        require(w.spectral==1&&w.granular>.9999,"tiny cyber does not halve full granular");
        require(std::abs(render(w)+.75)<.0002,"small strength produces small routing change");
        a.reset();w=a.process(.8,3,0,0);
        require(w.spectral==1&&w.granular==.8/1.8,"strong static behavior unchanged");
        a.reset();w=a.process(0,0,.001,0);
        require(w.spectral==1&&w.granular==0,"subtle spectral-only effect not attenuated");
        a.reset();const double nan=std::numeric_limits<double>::quiet_NaN();w=a.process(nan,nan,nan,nan);
        require(w.spectral==0&&w.granular==0,"nonfinite controls fail neutral");
        a.reset();(void)a.process(1,0,0,0);(void)a.process(1,0,1,0);w=a.process(1,0,0,0);
        require(w.spectral==0&&w.granular==1,"immediate retarget from current coefficient");
        std::cout<<sr<<" Hz: "<<n<<" samples, maximum deactivation step "<<peak<<'\n';
    }
}
