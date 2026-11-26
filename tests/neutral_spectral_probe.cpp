#include "dsp/Cybernetic.hpp"
#include "reference/CyberneticP026.hpp"
#include <chrono>
#include <iostream>
using namespace cyberator;
template<class T> void probe(double sr,bool active){
    T a;a.prepare(sr);const auto n=std::size_t(sr*2);std::vector<StereoFrame> in(n);
    for(std::size_t i=0;i<n;++i)in[i]={float(.3*std::sin(.017*double(i))),float(.2*std::cos(.029*double(i)))};
    CyberParams p=active?CyberParams{5,.4,.8}:CyberParams{};
    for(auto x:in)(void)a.process(x,p);
    auto t=std::chrono::steady_clock::now();double checksum=0;
    for(auto x:in){auto y=a.process(x,p);checksum+=y.left;}
    std::cout<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-t).count()<<" ms checksum "<<checksum<<'\n';
}
int main(){for(int rep=0;rep<3;++rep)for(double sr:{48000.,192000.})for(bool active:{false,true}){std::cout<<sr<<" active "<<active<<" old ";probe<CyberneticP026>(sr,active);std::cout<<sr<<" active "<<active<<" new ";probe<Cybernetic>(sr,active);}}
