#include "dsp/Harmony.hpp"
#include "reference/HarmonyOriginal.hpp"
#include <cstdlib>
#include <iostream>
#include <random>
using namespace cyberator;
int main(){
    std::mt19937 rng(27183);std::uniform_real_distribution<float> noise(-.7f,.7f);
    for(double sr:{8000.,44100.,48000.,96000.,192000.,44125.,44100.125})for(bool alwaysOn:{false,true}){
        Harmony a;HarmonyOriginal b;a.prepare(sr);b.prepare(sr);
        double peak=0,steadyPeak=0,energy=0,error=0;
        for(int i=0;i<int(sr*3);++i){
            if(i==int(sr*1.5)){a.reset();b.reset();}
            StereoFrame x{noise(rng),noise(rng)};
            const double amount=alwaysOn||i%11003>=1001?.8:0;
            auto y=a.process(x,amount),z=b.process(x,amount);
            double dl=double(y.left)-z.left,dr=double(y.right)-z.right;
            peak=std::max({peak,std::abs(dl),std::abs(dr)});
            if(i>int(sr*.1)&& (i<int(sr*1.5)||i>int(sr*1.6)))steadyPeak=std::max({steadyPeak,std::abs(dl),std::abs(dr)});
            error+=dl*dl+dr*dr;energy+=double(z.left)*z.left+double(z.right)*z.right;
        }
        std::cout<<sr<<" alwaysOn "<<alwaysOn<<" max "<<peak<<" steady "<<steadyPeak<<" rms rel "<<std::sqrt(error/energy)<<'\n';
        if(steadyPeak>2e-6||peak>2e-6||std::sqrt(error/energy)>1e-6)return 1;
    }
}
