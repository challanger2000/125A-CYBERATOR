#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <span>
#include <vector>
namespace cyberator::test {
struct RenderWitness {double energy{},referenceEnergy{},cross{},maxPeak{},maxError{},relativeError{},shapeError{};};
inline RenderWitness compare(std::span<const float> output,std::span<const float> reference){
    if(output.empty()||output.size()!=reference.size())throw std::invalid_argument("render lengths");
    RenderWitness m;double error=0;
    for(std::size_t i=0;i<output.size();++i){const double x=output[i],y=reference[i];
        if(!std::isfinite(x)||!std::isfinite(y))throw std::invalid_argument("nonfinite render");
        m.energy+=x*x;m.referenceEnergy+=y*y;m.cross+=x*y;m.maxPeak=std::max(m.maxPeak,std::abs(x));
        const double d=x-y;error+=d*d;m.maxError=std::max(m.maxError,std::abs(d));}
    m.relativeError=std::sqrt(error/std::max(1e-24,m.referenceEnergy));
    if(m.energy>1e-24&&m.referenceEnergy>1e-24)m.shapeError=std::sqrt(std::max(0.,1-m.cross*m.cross/(m.energy*m.referenceEnergy)));
    return m;
}
inline bool audibleChange(const RenderWitness& m)noexcept{return m.energy>1e-6&&m.referenceEnergy>1e-6&&m.shapeError>.01;}
inline bool recallMatches(const RenderWitness& m)noexcept{return m.energy>1e-6&&m.referenceEnergy>1e-6&&m.maxError<=2e-5&&m.relativeError<=1e-5;}
}
