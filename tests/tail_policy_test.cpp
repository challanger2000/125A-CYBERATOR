#include "control/TailPolicy.hpp"
#include <cstdlib>
#include <iostream>
#include <numbers>
#include <array>
using namespace cyberator;
void check(bool b,const char* m){if(!b){std::cerr<<m<<'\n';std::exit(1);}}
int main(){
    double l1=0;
    // Independent reconstruction of the pinned 48-tap Blackman kernel table.
    for(int band=0;band<9;++band)for(int p=0;p<=256;++p){
        const double cutoff=band==0?1.:.92/std::exp2(band/4.);
        std::array<float,48> a{};double sum=0;
        for(int k=0;k<48;++k){const double x=k-23-p/256.,z=std::numbers::pi*cutoff*x;
            const double w=cutoff*(z==0?1.:std::sin(z)/z)*(.42+.5*std::cos(std::numbers::pi*x/24)+.08*std::cos(2*std::numbers::pi*x/24));
            a[std::size_t(k)]=float(w);sum+=w;}
        double norm=0;for(auto v:a)norm+=std::abs(double(v/float(sum)));l1=std::max(l1,norm);
    }
    check(l1<3&&l1>2.6,"conservative sinc norm witness");
    check(tailSeconds(false,0)==30&&std::isinf(tailSeconds(true,0)),"zero and freeze policy");
    double old=30;
    for(int i=1;i<=1000;++i){const double f=i/1000.,q=.85*f*3,tail=tailSeconds(false,f);
        check(tail>=old,"monotone reported decay");old=tail;
        if(std::isfinite(tail)){check(q<1&&std::pow(q,(tail-30)/3.25)<=1e-6,"relative decay bound");check(tail*192000<std::numeric_limits<int>::max(),"VST3 conversion range");}
        if(q>=1)check(std::isinf(tail),"no false finite guarantee");}
    check(std::isinf(tailSeconds(false,std::numeric_limits<double>::quiet_NaN())),"invalid feedback conservative");
    std::cout<<"Max sinc L1 "<<l1<<", tail .1 "<<tailSeconds(false,.1)<<", .3 "<<tailSeconds(false,.3)<<", .5 infinite "<<std::isinf(tailSeconds(false,.5))<<'\n';
}
