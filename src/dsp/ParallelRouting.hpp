#pragma once
#include <algorithm>
#include <cmath>
namespace cyberator {
struct ParallelWeights {double spectral{},granular{};};
// Continuous participation at the origin; slew only the routing coefficients.
// With substantial spectral processing, retain the previous normalized blend.
class ParallelRouting final {
public:
    void prepare(double sr) noexcept {
        step_=1/std::round(.020*(std::isfinite(sr)?std::clamp(sr,8000.,192000.):48000.));
        reset();
    }
    void reset() noexcept {initialized_=false;presence_=strength_=0;}
    [[nodiscard]] ParallelWeights process(double granular,double formant,double android,double spectral) noexcept {
        const double intensity=std::max({unit(std::abs(formant)/12),unit(android),unit(spectral)});
        const double t=unit(5*intensity),target=t*t*(3-2*t),present=intensity>0?1.:0.;
        if(!initialized_){presence_=present;strength_=target;initialized_=true;}
        else {presence_=approach(presence_,present);strength_=approach(strength_,target);}
        const double g=unit(granular);
        return {presence_,g/(1+g*strength_)};
    }
private:
    static double unit(double x) noexcept {return std::isfinite(x)?std::clamp(x,0.,1.):0.;}
    double approach(double from,double to)const noexcept {
        const double d=to-from;
        return std::abs(d)<=step_+1e-12?to:from+std::copysign(step_,d);
    }
    double step_{1./960},presence_{},strength_{};bool initialized_{};
};
} // namespace cyberator
