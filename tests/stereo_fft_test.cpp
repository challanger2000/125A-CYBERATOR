#include "dsp/Cybernetic.hpp"
#include "reference/CyberneticSeparate.hpp"
#include <cstdlib>
#include <iostream>
using namespace cyberator;
void check(bool ok,const char* m){if(!ok){std::cerr<<m<<'\n';std::exit(1);}}
int main(){
    double worst=0,worstRelative=0;
    for(double sr:{8000.,44100.,48000.,96000.,192000.})for(bool live:{false,true})for(int kind=0;kind<5;++kind){
        Cybernetic a;CyberneticReference b;a.prepare(sr,live);b.prepare(sr,live);
        double peak=0,err=0,energy=0;
        for(int i=0;i<int(sr*.5);++i){
            double t=i/sr;float x=float(.4*std::sin(2*std::numbers::pi*173*t)+.2*std::cos(2*std::numbers::pi*911*t)),r=0;
            switch(kind){case 0:r=float(.3*std::cos(2*std::numbers::pi*277*t)+.1*std::sin(2*std::numbers::pi*1333*t));break;case 1:r=-x;break;case 2:r=x;break;case 3:r=0;break;case 4:x=float(.5*std::sin(2*std::numbers::pi*937.5*t));r=float(.5*std::cos(2*std::numbers::pi*937.5*t));break;}
            const CyberParams p{6*std::sin(4*t),.4+.3*std::cos(8*t),.4+.25*std::sin(3*t)};
            auto y=a.process({x,r},p),z=b.process({x,r},p);
            const double dl=double(y.left)-z.left,dr=double(y.right)-z.right;
            peak=std::max({peak,std::abs(dl),std::abs(dr)});err+=dl*dl+dr*dr;energy+=double(z.left)*z.left+double(z.right)*z.right;
        }
        const double relative=std::sqrt(err/std::max(1e-24,energy));worst=std::max(worst,peak);worstRelative=std::max(worstRelative,relative);
        check(peak<2e-6&&relative<1e-6,"packed vs separate FFT waveform incl quadrature phase boundary");
    }
    std::cout<<"Worst absolute "<<worst<<", relative RMS "<<worstRelative<<'\n';
}
