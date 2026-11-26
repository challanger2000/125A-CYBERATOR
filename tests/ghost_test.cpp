#include "dsp/Ghost.hpp"
#include <cstdlib>
#include <iostream>
using namespace cyberator;
void check(bool x,const char* m){if(!x){std::cerr<<m<<'\n';std::exit(1);}}
int main(){
    for(double sr:{8000.,44100.,48000.,96000.,192000.}){
        Ghost g;g.prepare(sr);const auto L=std::size_t(std::round(sr*.160));
        const auto W=2*std::size_t(std::round(sr*.090));
        check(g.latency()==L,"latency");
        double first=0,last=0,total=0,echo=0;std::size_t count=0;
        for(std::size_t n=0;n<=g.tail()+1;++n){
            auto y=g.process(n==0?StereoFrame{1,0}:StereoFrame{},1,1);
            check(y.dry.left==(n==L?1:0)&&y.dry.right==0,"aligned dry impulse");
            check(y.echo.right==0&&y.swell.right==0,"no channel leak");
            if(n<=L)check(y.echo.left==0,"echo after aligned dry");
            if(n>=L)check(y.swell.left==0,"swell before aligned dry");
            const double power=double(y.swell.left)*y.swell.left;
            if(n<L/4)first+=power;
            if(n>=3*L/4)last+=power;
            if(power>0)++count;
            total+=power;echo+=std::abs(y.echo.left);
            if(n==g.tail()+1)check(y.echo.left==0,"tail bound");
        }
        check(count==96&&std::abs(total-1)<1e-6,"swell sparse energy");
        check(last>first*100,"swell rises"); check(echo>.01,"echo audible impulse");
        g.reset();
        // Independent linear-source identity: within each head the source goes backwards;
        // for a linear ramp, the two weighted head contributions have known source indices.
        for(std::size_t n=0;n<L+7*W;++n){
            auto y=g.process({float(n)*.00001f,0},1,0);
            if(n<L+5*W)continue;
            const auto phase=n%W;
            // Independent oracle uses segment start and reversed source sample index.
            double expected=0;
            for(std::size_t shift:{std::size_t(0),W/2}){
                const auto k=(n+shift)%W;
                const auto start=n-k;
                const double w=.5-.5*std::cos(2*std::numbers::pi*double(k)/double(W));
                expected+=w*(double(start-L-1-k)*1e-5 + .45*double(start-L-1-k-W)*1e-5 + .2*double(start-L-1-k-2*W)*1e-5)/1.65;
            }
            (void)phase;
            check(std::abs(y.echo.left-expected)<2e-7,"reversed source identity");
            check(y.swell.left==0,"zero swell");
        }
        g.reset();for(std::size_t n=0;n<L+2;++n){auto y=g.process({.5f,-.25f},0,0);check(y.echo.left==0&&y.swell.left==0,"both zero");}
    }
    Ghost g;g.prepare(48000);for(int n=0;n<50000;++n){auto y=g.process({std::numeric_limits<float>::max(),-std::numeric_limits<float>::max()},1,1);check(std::isfinite(y.echo.left)&&std::isfinite(y.swell.right),"finite extreme");}
    std::cout<<"Ghost contract PASS\n";
}
