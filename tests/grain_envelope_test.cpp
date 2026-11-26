#include "dsp/GrainBank.hpp"
#include "reference/GrainBankP026.hpp"
#include <iostream>
#include <random>
#include <cstdlib>
using namespace cyberator;
void check(bool x,const char* m){if(!x){std::cerr<<m<<'\n';std::exit(1);}}
int main(){
    std::mt19937 rng(55218);std::uniform_real_distribution<float> noise(-.7f,.7f);
    double worst=0,err=0,energy=0;
    for(auto length:{3u,4u,17u,255u,256u,257u,258u,1024u,48000u}){
        GrainBank a;cyberator_p026::GrainBank b;a.prepare(240128);b.prepare(240128);
        for(unsigned i=0;i<240128;++i){float l=noise(rng),r=noise(rng);(void)a.process({l,r},false);(void)b.process({l,r},false);}
        for(unsigned i=0;i<32;++i){double rate=i%2?-.75:2.;double age=110000+i*.3125;check(a.trigger(age,rate,length,.4)==b.trigger(age,rate,length,.4),"admission equal");}
        check(a.active()==32&&b.active()==32,"full pool");check(!a.trigger(110000,1,length)&&!b.trigger(110000,1,length),"full pool rejects");
        for(unsigned i=0;i<length+8;++i){float l=noise(rng),r=noise(rng);auto x=a.process({l,r},i%7==0);auto y=b.process({l,r},i%7==0);check(a.active()==b.active(),"retirement equal");double dl=double(x.left)-y.left,dr=double(x.right)-y.right;worst=std::max({worst,std::abs(dl),std::abs(dr)});err+=dl*dl+dr*dr;energy+=double(y.left)*y.left+double(y.right)*y.right;}
        check(a.active()==0,"all retired");check(a.trigger(110000,1,3)&&b.trigger(110000,1,3),"slot reuse");a.reset();b.reset();check(a.active()==0&&b.active()==0,"reset clears slots");
    }
    // Mixed-length voices leave holes; lowest free slot admission and summation order must remain stable.
    GrainBank a;cyberator_p026::GrainBank b;a.prepare(4096);b.prepare(4096);
    for(int i=0;i<4096;++i){float l=noise(rng),r=noise(rng);(void)a.process({l,r},false);(void)b.process({l,r},false);}
    for(int i=0;i<2048;++i){
        if(i%7==0)check(a.trigger(1500,1,std::size_t(3+i%100),.4)==b.trigger(1500,1,std::size_t(3+i%100),.4),"mixed length allocation");
        const float l=noise(rng),r=noise(rng);auto x=a.process({l,r},i%5==0);auto y=b.process({l,r},i%5==0);
        check(a.active()==b.active(),"mixed active count");check(std::abs(double(x.left)-y.left)<=2e-7&&std::abs(double(x.right)-y.right)<=2e-7,"mixed voice output");
    }
    std::cout<<"Max error "<<worst<<", relative RMS "<<std::sqrt(err/energy)<<'\n';
    check(worst<=2e-7&&std::sqrt(err/energy)<1e-6,"window waveform equivalent");
}
