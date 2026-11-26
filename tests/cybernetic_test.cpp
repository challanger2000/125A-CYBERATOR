#include "dsp/Cybernetic.hpp"
#include <cstdlib>
#include <iostream>
using namespace cyberator;
void check(bool x,const char* m){if(!x){std::cerr<<m<<'\n';std::exit(1);}}
int main(){
    for(double sr:{8000.,44100.,48000.,96000.,192000.}){
        Cybernetic g;g.prepare(sr);const auto N=g.latency();
        std::vector<StereoFrame> source(6*N);
        for(std::size_t i=0;i<source.size();++i)source[i]={float(.7*std::sin(.031*double(i))+.1*std::cos(.17*double(i))),float(.4*std::cos(.047*double(i)))};
        double err=0;
        for(std::size_t i=0;i<source.size()+N;++i){auto y=g.process(i<source.size()?source[i]:StereoFrame{},{});auto x=i>=N?source[i-N]:StereoFrame{};err=std::max({err,std::abs(double(y.left-x.left)),std::abs(double(y.right-x.right))});}
        check(err<2e-7,"neutral STFT reconstruction and latency");
        g.reset();double later=0;
        for(std::size_t i=0;i<9*N;++i){auto y=g.process(i<N?StereoFrame{float(std::sin(.071*double(i))),0}:StereoFrame{},CyberParams{0,0,1});if(i>3*N)later+=std::abs(y.left);check(y.right==0,"no channel crosstalk");}
        check(later>1,"spectral memory tail");
        g.reset();double delta=0;
        for(std::size_t i=0;i<5*N;++i){float x=float(std::sin(.071*double(i)));auto y=g.process({x,-x},CyberParams{0,1,0});if(i>=N)delta+=std::abs(y.left-float(std::sin(.071*double(i-N))));check(y.left==-y.right,"android stereo polarity");}
        check(delta>double(N)*.1,"android transforms audio");
        g.reset();
        for(std::size_t i=0;i<3*N;++i){auto y=g.process({std::numeric_limits<float>::max(),-std::numeric_limits<float>::max()},CyberParams{12,1,1});check(std::isfinite(y.left)&&std::isfinite(y.right),"extreme finite");}
        std::cout<<sr<<" Hz N="<<N<<" neutral max="<<err<<'\n';
    }
    // A synthetic harmonic comb with an envelope peaking at 1 kHz. Measure
    // relative levels at 1 and 2 kHz after +12 semitones of envelope warping.
    Cybernetic a,b;a.prepare(48000);b.prepare(48000);double ar=0,ai=0,br=0,bi=0,cr=0,ci=0,dr=0,di=0;
    for(int i=0;i<48000;++i){double x=0;for(int k=1;k<=40;++k){const double f=100*k;const double amp=.01+.14*std::exp(-std::pow((f-1000)/450,2));x+=amp*std::sin(2*std::numbers::pi*f*i/48000);}
        auto y=a.process({float(x),float(x)},{});auto z=b.process({float(x),float(x)},CyberParams{12,0,0});
        if(i>=24000){double t=2*std::numbers::pi*1000*i/48000;ar+=y.left*std::cos(t);ai+=y.left*std::sin(t);br+=y.left*std::cos(2*t);bi+=y.left*std::sin(2*t);cr+=z.left*std::cos(t);ci+=z.left*std::sin(t);dr+=z.left*std::cos(2*t);di+=z.left*std::sin(2*t);}}
    double old=std::hypot(br,bi)/std::hypot(ar,ai),now=std::hypot(dr,di)/std::hypot(cr,ci);std::cout<<"formant ratio "<<old<<" -> "<<now<<'\n';check(now>old*3,"formant envelope moves upward");
    std::cout<<"Cybernetic prototype PASS\n";
}
