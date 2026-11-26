#include "dsp/Granular.hpp"
#include <iostream>
using namespace cyberator;
int main(){for(double pitch:{0.,12.,-12.})for(double rev:{0.,1.}){
    const double sr=8000;Granular g;g.prepare(sr);GranularParams p;p.sizeMs=250;p.density=1;p.position=1;p.pitch=pitch;p.reverse=rev;p.feedback=1;
    for(int i=0;i<int(sr*4);++i)(void)g.process({float(.25*std::sin(.037*i)),float(.2*std::cos(.059*i))},p);
    double peak=0;int last=-1;for(int i=0;i<int(sr*150);++i){auto y=g.process({},p);peak=std::max({peak,std::abs(double(y.left)),std::abs(double(y.right))});if((i+1)%int(sr)==0){int sec=(i+1)/int(sr);if(peak>std::pow(10.,-90./20))last=sec;if(sec==30||sec==60||sec==90||sec==120||sec==150)std::cout<<pitch<<' '<<rev<<' '<<sec<<' '<<peak<<'\n';peak=0;}}
    std::cout<<"last above -90 dBFS "<<last<<'\n';
}}
