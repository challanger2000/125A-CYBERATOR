#include "control/ParameterRamp.hpp"
#include <cstdlib>
#include <iostream>
using namespace cyberator;
void check(bool b,const char* m){if(!b){std::cerr<<m<<'\n';std::exit(1);}}
int main(){
    for(double sr:{8000.,44100.,48000.,96000.,192000.}){
        ParameterRamp r;r.prepare(sr);Parameters p;r.target(p);
        check(r.advance().get(Param::mix)==1,"initial snapshot exact");
        check(p.set(Param::mix,0)&&p.set(Param::output,12)&&p.set(Param::freeze,1),"fixture");
        r.target(p);const auto n=std::size_t(std::round(.01*sr));
        for(std::size_t i=1;i<=n;++i){
            if(i%7==0)r.target(p);
            const auto& q=r.advance();const double t=double(i)/double(n);
            check(std::abs(q.get(Param::mix)-(1-t))<2e-5,"linear mix ramp");
            check(std::abs(q.get(Param::output)-12*t)<.0003,"linear output ramp");
            check(q.get(Param::freeze)==1,"discrete immediate");
        }
        check(r.advance().get(Param::mix)==0&&r.advance().get(Param::output)==12,"exact end");
        check(p.set(Param::mix,1),"retarget fixture");r.target(p);
        for(std::size_t i=0;i<n/2;++i)(void)r.advance();
        const float before=r.advance().get(Param::mix);
        check(p.set(Param::mix,0),"reverse fixture");r.target(p);
        const float after=r.advance().get(Param::mix);
        check(after<before&&std::abs(after-before)<.02,"retarget continuous");
        r.reset();r.target(p);check(r.advance().get(Param::mix)==0,"reset snap");
    }
    std::cout<<"Parameter ramp contract PASS\n";
}
