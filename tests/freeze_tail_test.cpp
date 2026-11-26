#include "dsp/Granular.hpp"
#include <cstdlib>
#include <iostream>
using namespace cyberator;
void check(bool b,const char* m){if(!b){std::cerr<<m<<'\n';std::exit(1);}}
int main(){
    for(double sr:{8000.,48000.,192000.}){
        Granular a;a.prepare(sr);GranularParams p;p.sizeMs=10;p.position=.3;p.density=.5;
        for(int i=0;i<int(sr);++i)(void)a.process({.25f,-.125f},p);
        p.freeze=true;double first=0,late=0;auto launches=a.launches();
        for(int i=0;i<int(sr*32);++i){auto y=a.process({},p);const double en=double(y.left)*y.left+double(y.right)*y.right;if(i<int(sr))first+=en;if(i>=int(sr*31))late+=en;}
        check(first>1&&late>1&&late/first>.95&&late/first<1.05,"freeze remains audible after 30 seconds");
        check(a.launches()-launches>900,"ongoing grain launches");
        p.freeze=false;double residual=0;
        for(int i=0;i<int(sr*5);++i){auto y=a.process({},p);if(i>=int(sr*4))residual+=std::abs(y.left)+std::abs(y.right);}
        check(residual==0,"zero-feedback released capture expires");
        std::cout<<sr<<" Hz: 31st-second energy / first-second energy "<<late/first<<", release residual "<<residual<<'\n';
    }
}
