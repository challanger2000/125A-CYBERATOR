#include "dsp/GrainBank.hpp"
#include "reference/SincGrainBank.hpp"
#include <bit>
#include <cstdlib>
#include <iostream>
#include <random>
using namespace cyberator;
void require(bool b,const char* m){if(!b){std::cerr<<m<<'\n';std::exit(1);}}
void same(StereoFrame a,cyberator_reference::StereoFrame b){require(a.left==b.left&&a.right==b.right,"optimized PCM equals scalar reference");}
int main(){
    std::mt19937 rng(9137);std::uniform_real_distribution<float> noise(-1,1);
    SincReader a;a.prepare();cyberator_reference::SincReader b;b.prepare();SincReader::KernelCache cache;
    for(std::size_t capacity:{1,23,47,48,49,64,97,1024,131071}){
        History h;cyberator_reference::History ref;h.prepare(capacity);ref.prepare(capacity);
        for(std::size_t i=0;i<3*capacity+300;++i){
            if(i==capacity+17){h.reset();ref.reset();}
            float l=noise(rng),r=noise(rng);if(i%43==0)l=std::numeric_limits<float>::quiet_NaN();
            const bool freeze=i%7==0;h.capture({l,r},freeze);ref.capture({l,r},freeze);
            require(h.capacity()==ref.capacity()&&h.valid()==ref.valid(),"history size invariants");
            for(std::size_t j=0;j<4;++j){
                const auto age=std::size_t(rng())%(capacity+1),count=std::size_t(rng()%49);
                same(h.read(age),ref.read(age));
                const auto* span=h.backwardWindow(age,count);
                const bool valid=count>0&&age<h.valid()&&count<=h.valid()-age;
                require((span!=nullptr)==valid,"contiguous span validity");
                if(span)for(std::size_t k=0;k<count;++k)same(*(span-static_cast<std::ptrdiff_t>(k)),ref.read(age+k));
            }
            const double age=23+double(rng()%(capacity+1))+.375;
            const int band=int(rng()%9);
            same(a.read(h,age,band,&cache),b.read(ref,age,band));
            same(a.read(h,age,band),b.read(ref,age,band));
            same(a.read(h,age,band,&cache),b.read(ref,age,band));
            const double fractional=23+double(rng()%(capacity+1))+double(rng()%997)/997.;
            same(a.read(h,fractional,band),b.read(ref,fractional,band));
        }
    }
    for(double rate:{-4.,-2.,-1.,-.75,.25,.5,1.,1.037,2.,4.}){
        GrainBank a;cyberator_reference::GrainBank b;a.prepare(16000);b.prepare(16000);
        for(int i=0;i<16000;++i){float l=noise(rng),r=noise(rng);same(a.process({l,r},false),b.process({l,r},false));}
        for(int j=0;j<32;++j){const double age=8000+j*.124987;require(a.trigger(age,rate,1000,.2)==b.trigger(age,rate,1000,.2),"admission equal");}
        require(a.active()==32&&b.active()==32,"maximum polyphony exercised");
        for(int i=0;i<1500;++i){float l=noise(rng),r=noise(rng);same(a.process({l,r},i%17==0),b.process({l,r},i%17==0));}
        require(a.active()==0&&b.active()==0,"voice retirement equal");
    }
    std::cout<<"Mirrored history and SIMD/cache interpolation match original scalar PCM\n";
}
