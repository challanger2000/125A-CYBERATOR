// Offline diagnostic only. Input: little-endian mono float32, 48 kHz, no header.
#include "control/FactoryBank.hpp"
#include <fstream>
#include <iostream>
#include <iomanip>
#include <cstdlib>
using namespace cyberator;
int main(int argc,char** argv){
    if(argc!=4){std::cerr<<"usage: voice_fixture FILE FIRST COUNT\n";return 2;}
    std::ifstream stream(argv[1],std::ios::binary|std::ios::ate);if(!stream)return 2;
    auto bytes=stream.tellg();if(bytes<=0||bytes>48000*4*30||bytes%4!=0)return 2;
    std::vector<float> input(std::size_t(bytes)/4);stream.seekg(0);stream.read(reinterpret_cast<char*>(input.data()),bytes);if(!stream)return 2;
    double peak=0;for(auto x:input){if(!std::isfinite(x))return 2;peak=std::max(peak,std::abs(double(x)));}if(peak<1e-6)return 2;
    for(auto& x:input)x=float(double(x)*.25/peak);
    const auto first=std::size_t(std::strtoul(argv[2],nullptr,10)),count=std::size_t(std::strtoul(argv[3],nullptr,10));
    if(first>=factoryBank.size()||count>factoryBank.size()-first)return 2;
    Engine e;e.prepare(48000,LatencyProfile::studio);const auto latency=e.latency(),end=latency+input.size();
    const auto n=end+48000;double refEnergy=0;for(auto x:input)refEnergy+=2*double(x)*x;
    std::cout<<std::setprecision(8)<<"index,name,peak,rms_gain_db,shape_error,tail_ratio,stereo_difference_ratio,wet_energy_norm,cross_norm,dry_energy_norm,wet_peak,dry_peak,output_db\n";
    for(std::size_t k=first;k<first+count;++k){e.reset();const auto p=factoryBank[k].parameters().engine();double peakOut=0,energy=0,cross=0,tail=0,stereo=0,wetPeak=0;
        for(std::size_t i=0;i<n;++i){const float x=i<input.size()?input[i]:0,ref=i>=latency&&i<end?input[i-latency]:0;
            const auto y=e.process({x,x},p);if(!std::isfinite(y.left)||!std::isfinite(y.right))return 1;
            peakOut=std::max({peakOut,std::abs(double(y.left)),std::abs(double(y.right))});const double en=double(y.left)*y.left+double(y.right)*y.right;
            if(i<end){energy+=en;cross+=double(ref)*(y.left+y.right);}else tail+=en;
            const double d=double(y.left)-y.right;stereo+=d*d;
            wetPeak=std::max({wetPeak,std::abs(double(y.left)-(1-p.mix)*ref),std::abs(double(y.right)-(1-p.mix)*ref)});
        }
        const double shape=std::sqrt(std::max(0.,1-cross*cross/std::max(1e-24,energy*refEnergy)));
        std::cout<<k<<','<<factoryBank[k].name<<','<<peakOut<<','<<10*std::log10(std::max(1e-24,energy/refEnergy))<<','<<shape<<','<<tail/std::max(1e-24,energy)<<','<<stereo/std::max(1e-24,energy+tail)<<','<<(energy-2*(1-p.mix)*cross+(1-p.mix)*(1-p.mix)*refEnergy)/refEnergy<<','<<((1-p.mix)*cross-(1-p.mix)*(1-p.mix)*refEnergy)/refEnergy<<','<<(1-p.mix)*(1-p.mix)<<','<<wetPeak<<','<<.25*(1-p.mix)<<','<<p.outputDb<<'\n';
    }
}
