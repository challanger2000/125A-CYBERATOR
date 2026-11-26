#pragma once
#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>
#include <vector>
namespace cyberator::test {
// Independent absolute-time oracle: convert integer musical ticks to sample
// indices, then synthesize a constant-source Hann grain at each accepted tick.
inline std::vector<float> frozenGrid(double rate,double bpm,double start,int frames,int grainLength,float amplitude,int firstAllowed=0){
    if(!std::isfinite(rate)||rate<=0||!std::isfinite(bpm)||bpm<=0||!std::isfinite(start)||std::abs(start)>1000||frames<=0||grainLength<3||firstAllowed<0)throw std::invalid_argument("grid fixture");
    const double tickRate=bpm*8/60;
    const double tickStart=start*8;
    const double samplesPerTick=rate/tickRate;
    if(samplesPerTick<grainLength)throw std::invalid_argument("nonoverlapping fixture required");
    std::vector<float> out(static_cast<std::size_t>(frames));
    const auto first=static_cast<long long>(std::ceil(tickStart));
    const auto last=static_cast<long long>(std::ceil(tickStart+frames/samplesPerTick));
    for(auto k=first;k<=last;++k){
        const auto onset=static_cast<long long>(std::ceil((double(k)-tickStart)*samplesPerTick-1e-7));
        if(onset<firstAllowed||onset>=frames)continue;
        for(int i=1;i<grainLength-1&&onset+i<frames;++i){
            const double w=std::sin(std::numbers::pi*double(i)/double(grainLength-1));
            out[static_cast<std::size_t>(onset+i)]=float(amplitude*w*w);
        }
    }
    return out;
}
}
